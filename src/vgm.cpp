#include "vgm.h"

#include <stdtype.h>
#include <emu/SoundDevs.h>

#include <string>
#include <vector>

struct ChannelMetadata {
    std::string name;
    uint8_t subchip_idx;
    uint8_t chan_idx;
};

// Based off https://github.com/ValleyBell/in_vgm-libvgm/blob/e8a1fe7981efd49fe4f3712e2c6e70f9a2e3fdb3/dlg_cfg.cpp#L842
// TODO return std::vector<ChannelMetadata>
static void ShowMutingCheckBoxes(UINT8 chip_type, UINT8 ChipSet)
{
    UINT8 nchannel = 0;
    const char* channel_names[0x40] = {nullptr};
    uint32_t mute_mask[2] = {0};

    // Groups 0 and 1 belong to the chip (muted by PLR_MUTE_OPTS::chnMute[0]),
    // and group 2 belongs to the sub-chip (muted by PLR_MUTE_OPTS::chnMute[1]).
    // For example, when playing YM2608 VGM files, setting PLR_MUTE_OPTS::chnMute[0] = ~0
    // and calling SetDeviceMuting(0, muteOpts) mutes the FM and drum samples,
    // but not PSG.
    constexpr UINT8 GROUP_SIZE = 8;
    UINT8 ngroup = 0;
    UINT8 channels_per_group[0x04] = {0};
    const char* group_names[0x04] = {nullptr};

    char TempName[0x18] = {};

    switch(chip_type) {
    case DEVID_SN76496:
        nchannel = 4;
        channel_names[3] = "Noise";
        break;
    case DEVID_YM2413:
    case DEVID_YM3812:
    case DEVID_YM3526:
    case DEVID_Y8950:
        nchannel = 14;	// 9 + 5
        channel_names[ 9] = "Bass Drum";
        channel_names[10] = "Snare Drum";
        channel_names[11] = "Tom Tom";
        channel_names[12] = "Cymbal";
        channel_names[13] = "Hi-Hat";
        if (chip_type == DEVID_Y8950)
        {
            nchannel = 15;
            channel_names[14] = "Delta-T";
        }
        break;
    case DEVID_YM2612:
        nchannel = 7;	// 6 + DAC
        channel_names[6] = "DAC";
        break;
    case DEVID_YM2151:
        nchannel = 8;
        break;
    case DEVID_SEGAPCM:
        nchannel = 16;
        break;
    case DEVID_RF5C68:
        nchannel = 8;
        break;
    case DEVID_YM2203:
        nchannel = 6;	// 3 FM + 3 AY8910
        ngroup = 3;

        channels_per_group[0] = 3;
        group_names[0] = "FM Chn";

        channels_per_group[2] = 3;
        group_names[2] = "SSG Chn";
        break;
    case DEVID_YM2608:
    case DEVID_YM2610:
        nchannel = 16;	// 6 FM + 6 ADPCM + 1 DeltaT + 3 AY8910
        ngroup = 3;

        channels_per_group[0] = 6;
        group_names[0] = "FM Chn";

        channels_per_group[1] = 7;
        group_names[1] = "PCM Chn";
        channel_names[1 * GROUP_SIZE + 6] = "Delta-T";

        channels_per_group[2] = 3;
        group_names[2] = "SSG Chn";
        break;
    case DEVID_YMF262:
        nchannel = 23;	// 18 + 5
        channel_names[18] = "Bass Drum";
        channel_names[19] = "Snare Drum";
        channel_names[20] = "Tom Tom";
        channel_names[21] = "Cymbal";
        channel_names[22] = "Hi-Hat";
        break;
    case DEVID_YMF278B:
        nchannel = 24;
        break;
    case DEVID_YMF271:
        nchannel = 12;
        break;
    case DEVID_YMZ280B:
        nchannel = 8;
        break;
    case DEVID_32X_PWM:
        nchannel = 1;
        break;
    case DEVID_AY8910:
        nchannel = 3;
        break;
    case DEVID_GB_DMG:
        channel_names[0] = "Square 1";
        channel_names[1] = "Square 2";
        channel_names[2] = "Progr. Wave";
        channel_names[3] = "Noise";
        nchannel = 4;
        break;
    case DEVID_NES_APU:
        channel_names[0] = "Square 1";
        channel_names[1] = "Square 2";
        channel_names[2] = "Triangle";
        channel_names[3] = "Noise";
        channel_names[4] = "DPCM";
        channel_names[5] = "FDS";
        nchannel = 6;
        break;
    case DEVID_YMW258:	// Multi PCM
        nchannel = 28;
        break;
    case DEVID_uPD7759:
        nchannel = 1;
        break;
    case DEVID_OKIM6258:
        nchannel = 1;
        break;
    case DEVID_OKIM6295:
        nchannel = 4;
        break;
    case DEVID_K051649:
        nchannel = 5;
        break;
    case DEVID_K054539:
        nchannel = 8;
        break;
    case DEVID_C6280:
        nchannel = 6;
        break;
    case DEVID_C140:
        nchannel = 24;
        break;
    case DEVID_C219:
        nchannel = 16;
        break;
    case DEVID_K053260:
        nchannel = 4;
        break;
    case DEVID_POKEY:
        nchannel = 4;
        break;
    case DEVID_QSOUND:
        nchannel = 16;
        break;
    case DEVID_SCSP:
        nchannel = 32;
        break;
    case DEVID_WSWAN:
        nchannel = 4;
        break;
    case DEVID_VBOY_VSU:
        nchannel = 6;
        break;
    case DEVID_SAA1099:
        nchannel = 6;
        break;
    case DEVID_ES5503:
        nchannel = 32;
        break;
    case DEVID_ES5506:
        nchannel = 32;
        break;
    case DEVID_X1_010:
        nchannel = 16;
        break;
    case DEVID_C352:
        nchannel = 32;
        break;
    case DEVID_GA20:
        nchannel = 4;
        break;
    default:
        nchannel = 0;
        break;
    }

    if (! ngroup)
    {
        for (UINT8 chan_idx = 0; chan_idx < nchannel; chan_idx ++)
        {
            if (channel_names[chan_idx] == NULL)
            {
                if (1 + chan_idx < 10)
                    sprintf(TempName, "Channel %u", 1 + chan_idx);
                else
                    sprintf(TempName, "Channel %u (%c)", 1 + chan_idx, 'A' + (chan_idx - 9));
            }

            bool muted;
            if (chip_type == DEVID_YMF278B)
                muted = (mute_mask[1] >> chan_idx) & 0x01;
            else
                muted = (mute_mask[0] >> chan_idx) & 0x01;
        }
    }
    else
    {
        /// Divides GUI checkboxes into 3 groups of 8, and labels each group differently.
        for (UINT8 group_idx = 0; group_idx < ngroup; group_idx ++)
        {
            UINT8 const gui_chan_0 = group_idx * GROUP_SIZE;
            if (group_names[group_idx] == NULL)
                group_names[group_idx] = "Channel";

            UINT8 chan_idx = 0;
            UINT8 gui_chan = gui_chan_0;
            for (; chan_idx < channels_per_group[group_idx]; chan_idx ++, gui_chan ++)
            {
                if (channel_names[gui_chan] == NULL)
                {
                    if (1 + chan_idx < 10)
                        sprintf(TempName, "%s %u", group_names[group_idx], 1 + chan_idx);
                    else
                        sprintf(TempName, "%s %u (%c)", group_names[group_idx], 1 + chan_idx,
                                'A' + (chan_idx - 9));
                }

                bool checked;
                switch(group_idx)
                {
                case 0:
                    checked = (mute_mask[0] >> chan_idx) & 0x01;
                    break;
                case 1:
                    checked = (mute_mask[0] >> (channels_per_group[0] + chan_idx)) & 0x01;
                    break;
                case 2:
                    checked = (mute_mask[1] >> chan_idx) & 0x01;
                    break;
                }
            }
        }
    }
}
