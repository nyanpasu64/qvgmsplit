#include "backend.h"

// File loading
#include <utils/DataLoader.h>
#include <utils/FileLoader.h>

// Playback
#include <player/playera.hpp>

Backend::Backend() {

}

PlayerBase * f(PlayerA & mainPlr) {
    UINT32 masterVol = 0x10000;	// fixed point 16.16
    UINT32 maxLoops = 2;
    UINT32 sampleRate = 44100;

    PlayerA::Config pCfg = mainPlr.GetConfiguration();
    pCfg.masterVol = masterVol;
    pCfg.loopCount = maxLoops;
    pCfg.fadeSmpls = sampleRate * 4;	// fade over 4 seconds
    pCfg.endSilenceSmpls = sampleRate / 2;	// 0.5 seconds of silence at the end
    pCfg.pbSpeed = 1.0;
    mainPlr.SetConfiguration(pCfg);

    UINT8 retVal;

    DATA_LOADER *dLoad = FileLoader_Init("");

    DataLoader_SetPreloadBytes(dLoad,0x100);
    retVal = DataLoader_Load(dLoad);
    if (retVal)
    {
        DataLoader_CancelLoading(dLoad);
        DataLoader_Deinit(dLoad);
        fprintf(stderr, "Error 0x%02X loading file!\n", retVal);
        return;
    }
    retVal = mainPlr.LoadFile(dLoad);
    if (retVal)
    {
        DataLoader_CancelLoading(dLoad);
        DataLoader_Deinit(dLoad);
        fprintf(stderr, "Error 0x%02X loading file!\n", retVal);
        return;
    }

    return mainPlr.GetPlayer();
}

void g(PlayerBase* player) {
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
