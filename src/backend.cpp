#include "backend.h"
#include "lib/box_array.h"
#include "lib/enumerate.h"
#include "lib/format.h"
#include "lib/release_assert.h"
#include "vgm.h"

// File loading
#include <utils/DataLoader.h>
#include <utils/MemoryLoader.h>

// Playback
#include <player/playera.hpp>
#include <player/vgmplayer.hpp>
#include <player/s98player.hpp>
#include <player/droplayer.hpp>
#include <player/gymplayer.hpp>
#include <emu/SoundEmu.h>

#include <stx/result.h>

#include <QFile>
#include <QFuture>
#include <QFutureInterface>
#include <QRunnable>
#include <QThreadPool>

#include <atomic>
#include <algorithm>  // std::stable_sort
#include <cstdint>
#include <iostream>
#include <optional>

using std::move;
using stx::Result, stx::Ok, stx::Err;
using format::format_hex_2;

static constexpr uint8_t BIT_DEPTH = 16;
static constexpr size_t BUFFER_LEN = 2048;
static constexpr size_t CHANNEL_COUNT = 2;

struct DeleteDataLoader {
    void operator()(DATA_LOADER * obj) {
        // DataLoader_Deinit has this check too, but this check can be inlined,
        // which may make moves faster.
        if (obj == nullptr) {
            return;
        }
        DataLoader_Deinit(obj);
    }
};

using BoxDataLoader = std::unique_ptr<DATA_LOADER, DeleteDataLoader>;

struct Metadata {
    uint32_t player_type;

    std::vector<ChipMetadata> chips;
    std::vector<FlatChannelMetadata> flat_channels;

// impl
public:
    static Result<std::unique_ptr<Metadata>, QString> make(QByteArray file_data) {
        UINT8 status;

        auto loader = BoxDataLoader(MemoryLoader_Init(
            (UINT8 const*) file_data.data(), (UINT32) file_data.size()
        ));
        if (loader == nullptr) {
            return Err(Backend::tr("Failed to allocate MemoryLoader_Init"));
        }

        DataLoader_SetPreloadBytes(loader.get(), 0x100);
        status = DataLoader_Load(loader.get());
        if (status) {
            // BoxDataLoader calls DataLoader_Deinit upon destruction.
            // It seems DataLoader_Deinit calls DataLoader_Reset, which calls
            // DataLoader_CancelLoading. So we don't need to explicitly call
            // DataLoader_CancelLoading beforehand, and IDK why player.cpp does so.
            return Err(Backend::tr("Failed to extract file, error 0x%1")
                .arg(format_hex_2(status)));
        }

        auto player = std::make_unique<PlayerA>();
        /* Register all player engines.
         * libvgm will automatically choose the correct one depending on the file format. */
        player->RegisterPlayerEngine(new VGMPlayer);
        player->RegisterPlayerEngine(new S98Player);
        player->RegisterPlayerEngine(new DROPlayer);
        player->RegisterPlayerEngine(new GYMPlayer);

        status = player->LoadFile(loader.get());
        if (status) {
            return Err(Backend::tr("Failed to load file, error 0x%1")
                .arg(format_hex_2(status)));
        }

        std::vector<PLR_DEV_INFO> devices;

        PlayerBase * engine = player->GetPlayer();
        engine->GetSongDeviceInfo(devices);

        std::vector<ChipMetadata> chips;
        std::vector<FlatChannelMetadata> flat_channels = {
            FlatChannelMetadata {
                .name = "Master Audio",
                .chip_idx = (uint8_t) -1,
                .subchip_idx = (uint8_t) -1,
                .chan_idx = (uint8_t) -1,
                .enabled = true,
            }
        };

        chips.reserve(devices.size());

        for (auto const& [chip_idx, device] : enumerate<uint8_t>(devices)) {
            constexpr UINT8 OPTS = 0x01;  // enable long names
            const char* chipName = SndEmu_GetDevName(device.type, OPTS, device.devCfg);
            std::cerr << chipName << "\n";

            chips.push_back(ChipMetadata {
                .name = chipName,
                .chip_idx = chip_idx,
            });

            for (ChannelMetadata & channel : get_chip_metadata(device)) {
                flat_channels.push_back(FlatChannelMetadata {
                    .name = move(channel.name),
                    .chip_idx = chip_idx,
                    .subchip_idx = channel.subchip_idx,
                    .chan_idx = channel.chan_idx,
                    .enabled = true,
                });
            }
        }

        return Ok(std::make_unique<Metadata>(Metadata {
            .player_type = engine->GetPlayerType(),
            .chips = move(chips),
            .flat_channels = move(flat_channels),
        }));
    }
};

struct SoloSettings {
    /// Depends on the .vgm file.
    uint8_t chip_idx;

    /// Usually 0. YM2608's PSG channels have it set to 1.
    uint8_t subchip_idx;

    /// ChannelMetadata with the same subchip_idx are grouped together
    /// and have chan_idx monotonically increasing from 0.
    uint8_t chan_idx;
};

struct RenderSettings {
    std::optional<SoloSettings> solo;

    uint32_t sample_rate = 44100;

    /// Q15.16 signed floating point value. 0x1'0000 is 100% volume.
    int32_t volume = 0x1'0000;

    uint32_t loop_count;

    /// In seconds.
    float fade_duration = 4.0;

    // TODO duration override?
};

class Job : public QRunnable {
private:
    /// Implicitly shared, read-only.
    QByteArray _file_data;

    /// Points within _file_data, unique per job.
    BoxDataLoader _loader;

    std::unique_ptr<PlayerA> _player;
    BoxArray<int16_t, CHANNEL_COUNT * BUFFER_LEN> _buffer = {};

    // TODO add return type for success vs. cancel
    // TODO use something other than QFuture with richer progress info?
    QFutureInterface<void> _status;

// impl
public:
    // Internal. Do not call from outside make().
    explicit Job(QByteArray file_data, BoxDataLoader loader, std::unique_ptr<PlayerA> player)
        : _file_data(move(file_data))
        , _loader(move(loader))
        , _player(move(player))
    {}

    // TODO add settings
    static Result<std::unique_ptr<Job>, QString> make(
        QByteArray file_data, Metadata const& metadata, RenderSettings const& opt
    ) {
        std::unique_ptr<PlayerBase> engine;

        switch (metadata.player_type) {
        case FCC_VGM: engine = std::make_unique<VGMPlayer>(); break;
        case FCC_S98: engine = std::make_unique<S98Player>(); break;
        case FCC_DRO: engine = std::make_unique<DROPlayer>(); break;
        case FCC_GYM: engine = std::make_unique<GYMPlayer>(); break;
        default:
            return Err(Backend::tr("Failed to render, unrecognized file type 0x%1")
                .arg(QString::number(metadata.player_type, 16).toUpper()));
        }

        UINT8 status;

        auto loader = BoxDataLoader(MemoryLoader_Init(
            (UINT8 const*) file_data.data(), (UINT32) file_data.size()
        ));
        if (loader == nullptr) {
            return Err(Backend::tr("Failed to allocate MemoryLoader_Init"));
        }

        DataLoader_SetPreloadBytes(loader.get(), 0x100);
        status = DataLoader_Load(loader.get());
        if (status) {
            return Err(Backend::tr("Failed to extract file, error 0x%1")
                .arg(format_hex_2(status)));
        }

        auto player = std::make_unique<PlayerA>();

        // Register the correct playback engine. This saves memory compared to
        // creating 4 different engines for each channel in the file.
        player->RegisterPlayerEngine(engine.release());

        /* setup the player's output parameters and allocate internal buffers */
        if (player->SetOutputSettings(
            opt.sample_rate, CHANNEL_COUNT, BIT_DEPTH, BUFFER_LEN
        )) {
            return Err(Backend::tr("Unsupported channel count/bit depth (this should never happen)"));
        }

        /* set playback parameters */
        {
            PlayerA::Config pCfg = player->GetConfiguration();
            pCfg.masterVol = opt.volume;
            pCfg.loopCount = opt.loop_count;
            pCfg.fadeSmpls = (uint32_t) ((float) opt.sample_rate * opt.fade_duration);
            pCfg.endSilenceSmpls = 0;
            pCfg.pbSpeed = 1.0;
            player->SetConfiguration(pCfg);
        }

        status = player->LoadFile(loader.get());
        if (status) {
            return Err(Backend::tr("Failed to load file, error 0x%1")
                .arg(format_hex_2(status)));
        }

        PlayerBase * p_engine = player->GetPlayer();
        if (metadata.player_type == FCC_VGM) {
            VGMPlayer* vgmplay = dynamic_cast<VGMPlayer *>(p_engine);
            release_assert(vgmplay);
            player->SetLoopCount(vgmplay->GetModifiedLoopCount(opt.loop_count));
        }

        if (opt.solo) {
            SoloSettings const& solo = *opt.solo;

            auto nchip = metadata.chips.size();
            for (size_t chip_idx = 0; chip_idx < nchip; chip_idx++) {
                PLR_MUTE_OPTS mute{};

                if (chip_idx != solo.chip_idx) {
                    mute.disable = 0xff;
                    // Probably unnecessary, but do it anyway.
                    mute.chnMute[0] = mute.chnMute[1] = ~0u;

                } else for (size_t subchip_idx = 0; subchip_idx < 2; subchip_idx++) {
                    if (subchip_idx != solo.subchip_idx) {
                        mute.disable |= 1 << subchip_idx;
                        // Probably unnecessary, but do it anyway.
                        mute.chnMute[subchip_idx] = ~0u;

                    } else {
                        mute.chnMute[subchip_idx] = (~0u) ^ (1 << solo.chan_idx);
                    }
                }
                p_engine->SetDeviceMuting((uint32_t) chip_idx, mute);
            }
        }

        // It's not necessary to call Start() before GetTotalTime()
        // (only before converting to/from samples), but call Start() now so
        // if we add calls to Tick2Sample(), they won't fail.
        player->Start();
        auto max_time = (int) player->GetTotalTime(/*includeLoops=*/ 1);

        auto out = std::make_unique<Job>(
            move(file_data),
            move(loader),
            move(player));
        out->_status.setProgressRange(0, max_time);
        return Ok(std::move(out));
    }

    QFuture<void> status() {
        return _status.future();
    }

// impl QRunnable
public:
    void run() override {
        // Based off https://invent.kde.org/qt/qt/qtbase/-/blob/kde/5.15/src/concurrent/qtconcurrentrunbase.h#L95-121
        if (_status.isCanceled()) {
            _status.reportFinished();
            return;
        }

        try {
            // TODO open wav file
            throw "TODO";
        } catch (QException & e) {
            _status.reportException(e);
        } catch (...) {
            _status.reportException(QUnhandledException());
        }

        _status.reportFinished();
    }
};

Backend::Backend()
    : _metadata(std::make_unique<Metadata>(Metadata {}))
{
}

Backend::~Backend() = default;

QString Backend::load_path(QString path) {
    QByteArray file_data;

    {
        auto file = QFile(path);
        if (!file.open(QFile::ReadOnly)) {
            return tr("Failed to open file: %1").arg(file.errorString());
        }

        qint64 size = file.size();
        file_data = file.readAll();
        if (file_data.size() != size) {
            return tr("Failed to read file data, expected %1 bytes, read %2 bytes, error %3")
                .arg(size)
                .arg(file_data.size())
                .arg(file.errorString());
        }
        file.close();
    }

    {
        auto result = Metadata::make(file_data);
        if (result.is_err()) {
            return move(result.err_value());
        }

        _file_data = move(file_data);
        _metadata = move(result.value());
    }

    for (auto const& metadata : _metadata->flat_channels) {
        std::cerr
            << (int) metadata.chip_idx << " "
            << (int) metadata.subchip_idx << " "
            << (int) metadata.chan_idx << " "
            << metadata.name << "\n";
    }

    // TODO load _channels

    return {};
}

std::vector<ChipMetadata> const& Backend::chips() const {
    return _metadata->chips;
}

std::vector<ChipMetadata> & Backend::chips_mut() {
    return _metadata->chips;
}

void Backend::sort_channels() {
    auto const& chips = _metadata->chips;
    std::vector<uint8_t> chip_idx_to_order(chips.size());
    for (auto const& [i, chip] : enumerate<uint8_t>(chips)) {
        chip_idx_to_order[chip.chip_idx] = i;
    }

    auto & channels = _metadata->flat_channels;

    std::stable_sort(
        channels.begin(), channels.end(),
        [&chip_idx_to_order](FlatChannelMetadata const& a, FlatChannelMetadata const& b) {
            return chip_idx_to_order[a.chip_idx] < chip_idx_to_order[b.chip_idx];
        });
}

std::vector<FlatChannelMetadata> const& Backend::channels() const {
    return _metadata->flat_channels;
}

std::vector<FlatChannelMetadata> & Backend::channels_mut() {
    return _metadata->flat_channels;
}

void Backend::start_render() {

}

// TODO put in header
/// Translate a string in a global context, outside of a class.
static QString gtr(
    const char *context,
    const char *sourceText,
    const char *disambiguation = nullptr,
    int n = -1)
{
    return QCoreApplication::translate(context, sourceText, disambiguation, n);
}

struct Unit {};
#define OK  Ok(Unit{})

static void _change_mute(PlayerBase * player) {
    UINT8 retVal;
    PLR_DEV_OPTS devOpts;
    int chipID = 0;

    retVal = player->GetDeviceOptions((UINT32)chipID, devOpts);
    if (retVal & 0x80) {  // bad device ID
        return;
    }

    {
        PLR_MUTE_OPTS& muteOpts = devOpts.muteOpts;

        int letter = '\0';
        if (letter == 'E')
            muteOpts.disable = 0x00;
        else if (letter == 'D')
            muteOpts.disable = 0xFF;
        else if (letter == 'O')
            muteOpts.chnMute[0] = 0;
        else if (letter == 'X')
            muteOpts.chnMute[0] = ~(UINT32)0;

        player->SetDeviceMuting((UINT32)chipID, muteOpts);
        printf("-> Chip %s [0x%02X], Channel Mask: 0x%02X\n",
            (muteOpts.disable & 0x01) ? "Off" : "On", muteOpts.disable, muteOpts.chnMute[0]);
    }
}
