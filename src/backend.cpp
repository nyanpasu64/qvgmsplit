#include "backend.h"
#include "lib/box_array.h"
#include "lib/enumerate.h"
#include "lib/format.h"
#include "lib/release_assert.h"
#include "vgm.h"
#include "wave_writer.h"

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

using Amplitude = int16_t;
static constexpr uint8_t BIT_DEPTH = 16;
static constexpr uint32_t BUFFER_LEN = 2048;
static constexpr uint32_t CHANNEL_COUNT = 2;

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
        bool show_chip_name = devices.size() > 1;

        for (auto const& [chip_idx, device] : enumerate<uint8_t>(devices)) {
            constexpr UINT8 OPTS = 0x01;  // enable long names
            const char* chipName = SndEmu_GetDevName(device.type, OPTS, device.devCfg);
            std::cerr << chipName << "\n";

            chips.push_back(ChipMetadata {
                .name = chipName,
                .chip_idx = chip_idx,
            });

            for (
                ChannelMetadata & channel :
                get_chip_metadata(device, show_chip_name)
            ) {
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

QString FlatChannelMetadata::numbered_name(size_t row) const {
    // TODO maybe pass in a "show numbers" setting?

    auto channel_name = QString::fromStdString(name);
    if (row > 0) {
        channel_name = QStringLiteral("%1 - %2")
            .arg(row)
            .arg(channel_name);
    }
    return channel_name;
}

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

struct RenderJobState {
    QString _out_path;

    /// Implicitly shared, read-only.
    QByteArray _file_data;

    /// Points within _file_data, unique per job.
    BoxDataLoader _loader;

    std::unique_ptr<PlayerA> _player;
    BoxArray<Amplitude, BUFFER_LEN * CHANNEL_COUNT> _buffer = {};

    // TODO use something other than QFuture with richer progress info?
    QFutureInterface<QString> _status{};
};

class RenderJob : public QRunnable, private RenderJobState {
// impl
public:  // Public for std::make_unique.
    RenderJob(RenderJobState state)
        : RenderJobState(move(state))
    {}

public:
    static Result<std::unique_ptr<RenderJob>, QString> make(
        QString out_path,
        QByteArray file_data,
        Metadata const& metadata,
        RenderSettings const& opt)
    {
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
            return Err(Backend::tr(
                "Unsupported channel count/bit depth (this should never happen)"
            ));
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

        // Calling PlayerBase::SetDeviceMuting() with channel indices fails if you
        // haven't called PlayerA::Start(). (Calling PlayerBase::SetDeviceMuting() with
        // PLR_DEV_ID(chip, instance) works before calling PlayerA::Start().)
        //
        // It's not necessary to call Start() before GetTotalTime() (but it is
        // necessary to call it before converting to/from samples).
        player->Start();

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
                        mute.disable |= (uint8_t) (1 << subchip_idx);
                        // Probably unnecessary, but do it anyway.
                        mute.chnMute[subchip_idx] = ~0u;

                    } else {
                        mute.chnMute[subchip_idx] = (~0u) ^ (1 << solo.chan_idx);
                    }
                }
                status = p_engine->SetDeviceMuting((uint32_t) chip_idx, mute);
                assert(status == 0);
            }
        }

        auto max_time_s =
            (int) (player->GetTotalTime(/*includeLoops=*/ 1) + opt.fade_duration);

        auto out = std::make_unique<RenderJob>(RenderJobState {
            ._out_path = move(out_path),
            ._file_data = move(file_data),
            ._loader = move(loader),
            ._player = move(player),
        });
        // Progress range is [0..time in seconds].
        out->_status.setProgressRange(0, max_time_s);
        return Ok(std::move(out));
    }

    QFuture<QString> future() {
        return _status.future();
    }

private:
    void callback() {
        uint32_t sample_rate = _player->GetSampleRate();

        auto maybe_writer = Wave_Writer::make(sample_rate, _out_path);
        if (maybe_writer.is_err()) {
            _status.reportResult(
                Backend::tr("Error opening file: %1").arg(maybe_writer.err_value())
            );
            return;
        }
        auto writer = std::move(maybe_writer.value());
        writer->enable_stereo();

        PlayerBase * engine = _player->GetPlayer();

        /* figure out how many total frames we're going to render */
        uint32_t render_nsamp = engine->Tick2Sample(engine->GetTotalPlayTicks(
            _player->GetLoopCount()
        ));

        /* we only want to fade if there's a looping section. Assumption is
         * if the VGM doesn't specify a loop, it's a song with an actual ending */
        if(engine->GetLoopTicks()) {
            render_nsamp += _player->GetConfiguration().fadeSmpls;
        }

        uint32_t curr_samp = 0;
        while (curr_samp < render_nsamp) {
            if (_status.isCanceled()) {
                _status.reportResult(Backend::tr("Cancelled by user"));
                return;
            }

            std::fill(_buffer.begin(), _buffer.end(), 0);

            /* default to BUFFER_LEN PCM frames unless we have under BUFFER_LEN remaining */
            auto curr_frames = std::min(render_nsamp - curr_samp, BUFFER_LEN);

            // Render audio. Pass buffer size in bytes.
            _player->Render(
                curr_frames * CHANNEL_COUNT * (BIT_DEPTH / 8), _buffer.data()
            );

            // Write audio. Pass buffer size in samples.
            if (
                auto err = writer->write(_buffer.data(), curr_frames * CHANNEL_COUNT);
                !err.isEmpty()
            ) {
                _status.reportResult(Backend::tr("Error writing data: %1").arg(err));
                return;
            }
            curr_samp += curr_frames;
            // Set current time in seconds.
            _status.setProgressValue((int) (curr_samp / sample_rate));
        }

        if (auto err = writer->close(); !err.isEmpty()) {
            _status.reportResult(Backend::tr("Error finalizing file: %1").arg(err));
            return;
        }
    }

// impl QRunnable
public:
    void run() override {
        // Based off https://invent.kde.org/qt/qt/qtbase/-/blob/kde/5.15/src/concurrent/qtconcurrentrunbase.h#L95-121
        if (_status.isCanceled()) {
            _status.reportResult(Backend::tr("Cancelled by user"));
            _status.reportFinished();
            return;
        }

        try {
            callback();
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
