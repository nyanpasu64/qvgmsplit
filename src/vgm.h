#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct PLR_DEV_INFO;

/// Uniquely identifies a channel within a single chip.
/// The metadata used to mute a particular channel by setting
/// PLR_MUTE_OPTS::chnMute[subchip_idx] |= 1u << chan_idx
/// and calling SetDeviceMuting(chip_idx, muteOpts).
struct ChannelMetadata {
    /// Usually 0. YM2608's PSG channels have it set to 1.
    uint8_t subchip_idx;

    /// ChannelMetadata with the same subchip_idx are grouped together
    /// and have chan_idx monotonically increasing from 0.
    uint8_t chan_idx;

    /// Includes chip and channel name.
    std::string name;
};

std::vector<ChannelMetadata> get_chip_metadata(PLR_DEV_INFO const& device);
