#include "backend.h"
#include "lib/box_array.h"
#include "lib/enumerate.h"
#include "lib/format.h"
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

#include <atomic>
#include <cstdint>
#include <iostream>

using std::move;
using stx::Result, stx::Ok, stx::Err;
using format::format_hex_2;

static constexpr size_t BUFFER_LEN = 2048;

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

class Job {
public:
    /// Implicitly shared, read-only.
    QByteArray _file_data;

    /// Points within _file_data, unique per job.
    BoxDataLoader _loader;

    std::unique_ptr<PlayerA> _player;
    BoxArray<unsigned char, sizeof(int32_t) * 2 * BUFFER_LEN> _buffer = {};
    std::atomic<bool> _cancel = false;

    friend class JobHandle;

// impl
public:
    /// Please don't call directly. This is only public for make_shared().
    explicit Job(QByteArray file_data, BoxDataLoader loader, std::unique_ptr<PlayerA> player)
        : _file_data(move(file_data))
        , _loader(move(loader))
        , _player(move(player))
    {}

    static Result<std::shared_ptr<Job>, QString> make(
        QByteArray file_data, std::unique_ptr<PlayerA> player
    ) {
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

        status = player->LoadFile(loader.get());
        if (status) {
            return Err(Backend::tr("Failed to load file, error 0x%1")
                .arg(format_hex_2(status)));
        }

        return Ok(std::make_shared<Job>(
            move(file_data),
            move(loader),
            move(player)));
    }
};

JobHandle::~JobHandle() = default;

void JobHandle::cancel() {
    _job->_cancel.store(true, std::memory_order_relaxed);
}

class Metadata {
public:
    uint32_t _player_type;

    std::vector<FlatChannelMetadata> _channel_metadata;

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

        std::vector<FlatChannelMetadata> channel_metadata;

        for (auto const& [chip_idx, device] : enumerate<uint8_t>(devices)) {
            constexpr UINT8 OPTS = 0x01;  // enable long names
            const char* chipName = SndEmu_GetDevName(device.type, OPTS, device.devCfg);
            std::cerr << chipName << "\n";

            for (ChannelMetadata & meta : get_metadata(device)) {
                channel_metadata.push_back(FlatChannelMetadata {
                    .name = move(meta.name),
                    .chip_idx = chip_idx,
                    .subchip_idx = meta.subchip_idx,
                    .chan_idx = meta.chan_idx,
                    .enabled = true,
                });
            }
        }

        return Ok(std::make_unique<Metadata>(Metadata {
            ._player_type = engine->GetPlayerType(),
            ._channel_metadata = move(channel_metadata),
        }));
    }
};

Backend::Backend() {
}

Backend::~Backend() = default;

QString Backend::load_path(QString path) {
    _file_data.clear();
    _metadata.reset();
    _channels.clear();

    {
        auto file = QFile(path);
        if (!file.open(QFile::ReadOnly)) {
            return tr("Failed to open file: %1").arg(file.errorString());
        }

        qint64 size = file.size();
        _file_data = file.readAll();
        if (_file_data.size() != size) {
            return tr("Failed to read file data, expected %1 bytes, read %2 bytes, error %3")
                .arg(size)
                .arg(_file_data.size())
                .arg(file.errorString());
        }
        file.close();
    }

    {
        auto result = Metadata::make(_file_data);
        if (result.is_err()) {
            return move(result.err_value());
        }
        _metadata = move(result.value());
    }

    for (auto const& metadata : _metadata->_channel_metadata) {
        std::cerr
            << (int) metadata.chip_idx << " "
            << (int) metadata.subchip_idx << " "
            << (int) metadata.chan_idx << " "
            << metadata.name << "\n";
    }

    // TODO load _channels

    return {};
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

static Result<Unit, QString> _load_song(
    PlayerA & player, QString path, uint32_t sample_rate, uint8_t bit_depth
) {
    /* setup the player's output parameters and allocate internal buffers */
    if (player.SetOutputSettings(sample_rate, 2, bit_depth, BUFFER_LEN)) {
        return Err(gtr("backend", "Unsupported sample rate / bps"));
    }

    INT32 masterVol = 0x10000;	// fixed point 16.16
    UINT32 maxLoops = 2;
    UINT32 sampleRate = 44100;

    PlayerA::Config pCfg = player.GetConfiguration();
    pCfg.masterVol = masterVol;
    pCfg.loopCount = maxLoops;
    pCfg.fadeSmpls = sampleRate * 4;	// fade over 4 seconds
    pCfg.endSilenceSmpls = sampleRate / 2;	// 0.5 seconds of silence at the end
    pCfg.pbSpeed = 1.0;
    player.SetConfiguration(pCfg);

    UINT8 status;

    exit(1);
    DATA_LOADER *dLoad;

    /* attempt to load 256 bytes, bail if not possible */
    DataLoader_SetPreloadBytes(dLoad,0x100);
    status = DataLoader_Load(dLoad);
    if (status) {
        DataLoader_CancelLoading(dLoad);
        DataLoader_Deinit(dLoad);
        return Err(gtr("backend", "Error 0x%1 reading file")
            .arg(format_hex_2(status)));
    }

    /* associate the fileloader to the player -
     * automatically reads the rest of the file */
    status = player.LoadFile(dLoad);
    if (status) {
        DataLoader_CancelLoading(dLoad);
        DataLoader_Deinit(dLoad);
        return Err(gtr("backend", "Error 0x%1 parsing file")
            .arg(format_hex_2(status)));
    }

    auto engine = player.GetPlayer();
    if (VGMPlayer* vgmplay = dynamic_cast<VGMPlayer*>(engine)) {
        player.SetLoopCount(vgmplay->GetModifiedLoopCount(maxLoops));
    }

    return OK;
}

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
