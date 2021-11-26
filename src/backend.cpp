#include "backend.h"
#include "lib/format.h"

// File loading
#include <utils/DataLoader.h>
#include <utils/FileLoader.h>

// Playback
#include <player/playera.hpp>

#include <player/vgmplayer.hpp>
#include <player/s98player.hpp>
#include <player/droplayer.hpp>
#include <player/gymplayer.hpp>

#include <stx/result.h>

#include <QCoreApplication>
#include <QString>

using stx::Result, stx::Ok, stx::Err;

Backend::Backend() {

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

static constexpr UINT32 BUFFER_LEN = 2048;

struct Unit {};
#define OK  Ok(Unit{})

void setup(PlayerA & player) {
    /* Register all player engines.
     * libvgm will automatically choose the correct one depending on the file format. */
    player.RegisterPlayerEngine(new VGMPlayer);
    player.RegisterPlayerEngine(new S98Player);
    player.RegisterPlayerEngine(new DROPlayer);
    player.RegisterPlayerEngine(new GYMPlayer);
}

Result<Unit, QString> asdf(PlayerA & player) {
    return OK;
}

using format::format_hex_2;

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

    DATA_LOADER *dLoad =
#if HAVE_FILELOADER_W
        FileLoader_InitW(path.utf16());
#else
        FileLoader_Init(path.toUtf8().data());
#endif

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
