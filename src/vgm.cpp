#include "vgm.h"
#include "lib/release_assert.h"

#include <stdtype.h>
#include <emu/SoundDevs.h>
#include <emu/SoundEmu.h>
#include <player/playerbase.hpp>

#include <fmt/format.h>
#include <fmt/compile.h>

#include <iterator>  // std::back_inserter
#include <string_view>

std::vector<ChannelMetadata> get_metadata(const PLR_DEV_INFO &device) {
    // This is a rewrite of https://github.com/ValleyBell/in_vgm-libvgm/blob/e8a1fe7981ef/dlg_cfg.cpp#L842.
    // I verified the results look acceptable for YM2608 (sub-chip),
    // Sega Genesis (PSG+YM2612, no sub-chip), and OPL3 (18 channels) .vgm files.

    UINT8 nchannel = 0;
    std::string_view channel_names[0x40] = {};

    // Groups 0 and 1 belong to the chip (muted by PLR_MUTE_OPTS::chnMute[0]),
    // and group 2 belongs to the sub-chip (muted by PLR_MUTE_OPTS::chnMute[1]).
    // For example, when playing YM2608 VGM files, setting PLR_MUTE_OPTS::chnMute[0] = ~0
    // and calling SetDeviceMuting(0, muteOpts) mutes the FM and drum samples,
    // but not PSG.
    UINT8 ngroup = 0;
    UINT8 const subchip_from_group[0x04] = {0, 0, 1, 1};
    UINT8 channels_per_group[0x04] = {0};
    std::string_view group_names[0x04] = {};

    auto chip_type = device.type;

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
        channel_names[channels_per_group[0] + 6] = "Delta-T";

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

    constexpr uint8_t OPTS = 0x01;  // enable long names
    std::string const chip_name = SndEmu_GetDevName(device.type, OPTS, device.devCfg);

    std::vector<ChannelMetadata> out;
    out.reserve(nchannel);

    fmt::memory_buffer name;
    name.append(chip_name.data(), chip_name.data() + chip_name.size());
    name.push_back(' ');
    size_t const chip_name_end = name.size();

    auto format_channel_name = [&channel_names](
        fmt::memory_buffer & out, std::string_view group_name, uint8_t chan_in_group
    ) {
        if (channel_names[chan_in_group].empty()) {
            if (1 + chan_in_group < 10)
                fmt::format_to(std::back_inserter(out),
                    "{} {}", group_name, 1 + chan_in_group);
            else
                fmt::format_to(std::back_inserter(out),
                    "{} {} ({})",
                    group_name, 1 + chan_in_group, (char) ('A' + (chan_in_group - 9)));
        } else {
            out.append(
                channel_names[chan_in_group].begin(), channel_names[chan_in_group].end()
            );
        }
    };

    if (!ngroup) {
        uint8_t subchip_idx = 0;
        // I have no clue why the OPL4 uses mute mask index 1.
        if (chip_type == DEVID_YMF278B) {
            subchip_idx = 1;
        }

        for (UINT8 chan_idx = 0; chan_idx < nchannel; chan_idx ++) {
            name.resize(chip_name_end);
            format_channel_name(name, "Channel", chan_idx);

            out.push_back(ChannelMetadata {
                .subchip_idx = subchip_idx,
                .chan_idx = chan_idx,
                .name = std::string(name.begin(), name.size()),
            });
        }
    } else {
        uint8_t chan_per_subchip[2] = {0, 0};

        for (UINT8 group_idx = 0; group_idx < ngroup; group_idx++) {
            auto const nchan_in_group = channels_per_group[group_idx];
            if (nchan_in_group == 0) {
                continue;
            }

            uint8_t const subchip_idx = subchip_from_group[group_idx];
            for (
                uint8_t chan_in_group = 0;
                chan_in_group < nchan_in_group;
                chan_in_group++)
            {
                name.resize(chip_name_end);
                uint8_t chan_in_subchip = chan_per_subchip[subchip_idx]++;
                format_channel_name(name, group_names[group_idx], chan_in_group);

                out.push_back(ChannelMetadata {
                    .subchip_idx = subchip_idx,
                    .chan_idx = chan_in_subchip,
                    .name = std::string(name.begin(), name.size()),
                });
            }
        }
    }
    release_assert_equal(out.size(), nchannel);
    return out;
}
