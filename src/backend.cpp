#include "backend.h"
#include "lib/box_array.h"
#include "lib/format.h"

// File loading
#include <utils/DataLoader.h>
#include <utils/MemoryLoader.h>

// Playback
#include <player/playera.hpp>

#include <player/vgmplayer.hpp>
#include <player/s98player.hpp>
#include <player/droplayer.hpp>
#include <player/gymplayer.hpp>

#include <stx/result.h>

#include <QFile>

#include <atomic>
#include <cstdint>

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
            return Err(Backend::tr("Failed to extract file, error %1")
                .arg(format_hex_2(status)));
        }

        status = player->LoadFile(loader.get());
        if (status) {
            return Err(Backend::tr("Failed to load file, error %1")
                .arg(format_hex_2(status)));
        }

        return Ok(std::make_shared<Job>(
            std::move(file_data),
            std::move(loader),
            std::move(player)));
    }
};

JobHandle::~JobHandle() = default;

void JobHandle::cancel() {
    _job->_cancel.store(true, std::memory_order_relaxed);
}


Backend::Backend() {
}

Backend::~Backend() = default;

QString Backend::load_path(QString path) {
    _file_data.clear();
    _master_audio.reset();
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

    std::shared_ptr<Job> master_job;
    {
        auto master_player = std::make_unique<PlayerA>();

        /* Register all player engines.
         * libvgm will automatically choose the correct one depending on the file format. */
        master_player->RegisterPlayerEngine(new VGMPlayer);
        master_player->RegisterPlayerEngine(new S98Player);
        master_player->RegisterPlayerEngine(new DROPlayer);
        master_player->RegisterPlayerEngine(new GYMPlayer);

        // TODO SetEventCallback/SetFileReqCallback/SetLogCallback

        // TODO SetConfiguration() (either now or after loading file)

        auto result = Job::make(_file_data, std::move(master_player));
        if (result.is_err()) {
            return std::move(result.err_value());
        }
        master_job = std::move(result.value());
    }

    // _master_audio = ???;

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

Result<Unit, QString> asdf(PlayerA & player) {
    return OK;
}

Result<Unit, QString> load_song(
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

void change_mute(PlayerBase * player) {
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
