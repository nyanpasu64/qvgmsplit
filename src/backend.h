#pragma once

#include <player/playera.hpp>

#include <QCoreApplication>
#include <QString>

#include <memory>
#include <vector>
#include <cstdint>

class Metadata;
class Job;

struct ChipMetadata {
    std::string name;
    uint8_t chip_idx;
};

/// Uniquely identifies a channel in a .vgm file.
/// The metadata used to mute a particular channel by setting
/// PLR_MUTE_OPTS::chnMute[subchip_idx] |= 1u << chan_idx
/// and calling SetDeviceMuting(chip_idx, muteOpts).
struct FlatChannelMetadata {
    /// Includes chip and channel name.
    std::string name;

    /// Depends on the .vgm file. If -1, all chips/channels are rendered
    /// (master audio).
    uint8_t chip_idx;

    /// Usually 0. YM2608's PSG channels have it set to 1.
    uint8_t subchip_idx;

    /// ChannelMetadata with the same subchip_idx are grouped together
    /// and have chan_idx monotonically increasing from 0.
    uint8_t chan_idx;

    /// Whether to output the channel or not.
    bool enabled = true;
};

class Backend {
    Q_DECLARE_TR_FUNCTIONS(Backend)

    /// Whether the GUI is being updated in response to events.
    bool _during_update = false;

    QByteArray _file_data;
    std::unique_ptr<Metadata> _metadata;

    friend class StateTransaction;
public:
    Backend();
    ~Backend();

    /// If non-empty, holds error message.
    QString load_path(QString path);

    std::vector<ChipMetadata> const& chips() const;
    std::vector<ChipMetadata> & chips_mut();
    void sort_channels();

    /// Includes an extra entry for "Master Audio".
    std::vector<FlatChannelMetadata> const& channels() const;
    std::vector<FlatChannelMetadata> & channels_mut();
};

