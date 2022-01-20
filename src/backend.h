#pragma once

#include "settings.h"

#include <player/playera.hpp>

#include <QCoreApplication>
#include <QFuture>
#include <QString>
#include <QThreadPool>

#include <cstdint>
#include <memory>
#include <vector>

struct Metadata;

// It would be nice to have a relational view of data, so ChipMetadata and
// FlatChannelMetadata would be separate tables, and nchan would be either
// stored denormalized, or computed with a count over FlatChannelMetadata.
// But I'm not sure how to best integrate it into C++. And QAbstractItemModel isn't
// "best integrated", but worst. Maybe an ECS would do better?

/// Obtained by calling PLR_DEV_ID(PLR_DEV_INFO::type, PLR_DEV_INFO::instance).
using ChipId = uint32_t;

struct ChipMetadata {
    std::string name;
    ChipId chip_id;
};

struct RenderJobHandle {
    QString name;
    QString path;
    float time_multiplier;
    QFuture<QString> future;
};

/// Uniquely identifies a channel in a .vgm file.
/// The metadata used to mute a particular channel by setting
/// PLR_MUTE_OPTS::chnMute[subchip_idx] |= 1u << chan_idx
/// and calling SetDeviceMuting(chip_id, muteOpts).
struct FlatChannelMetadata {
    /// Includes chip and channel name.
    std::string name;

    /// Depends on the .vgm file. If -1, all chips/channels are rendered
    /// (master audio).
    ChipId maybe_chip_id;

    /// Usually 0. YM2608's PSG channels have it set to 1.
    uint8_t subchip_idx;

    /// ChannelMetadata with the same subchip_idx are grouped together
    /// and have chan_idx monotonically increasing from 0.
    uint8_t chan_idx;

    /// Whether to output the channel or not.
    bool enabled = true;

    QString numbered_name(size_t row) const;
};

constexpr ChipId NO_CHIP = (ChipId) -1;

class StateTransaction;
class Backend {
    Q_DECLARE_TR_FUNCTIONS(Backend)

    /// Whether the GUI is being updated in response to events.
    bool _during_update = false;

    Settings _settings;
    QByteArray _file_data;
    std::unique_ptr<Metadata> _metadata;
    QThreadPool _render_thread_pool;
    std::vector<RenderJobHandle> _render_jobs;

    friend class StateTransaction;
public:
    Backend();
    ~Backend();

    Settings const& settings() const {
        return _settings;
    }
    Settings & settings_mut(StateTransaction & tx);

    /// If non-empty, holds error message.
    [[nodiscard]] QString load_path(StateTransaction & tx, QString const& path);
    /// If non-empty, holds error message.
    [[nodiscard]] QString reload_settings();

    std::vector<ChipMetadata> const& chips() const;
    std::vector<ChipMetadata> & chips_mut();
    void sort_channels();

    bool is_file_loaded() const;
    uint32_t sample_rate() const;

    /// Includes an extra entry for "Master Audio".
    std::vector<FlatChannelMetadata> const& channels() const;
    std::vector<FlatChannelMetadata> & channels_mut();

    /// Returns a list of all render jobs. Length is either empty or matches the number
    /// of enabled channels in channels() when the last render was started.
    std::vector<RenderJobHandle> const& render_jobs() const;

    /// Returns whether there are unfinished render jobs.
    bool is_rendering() const;

    /// Cancel all active render jobs.
    void cancel_render();

    /// Returns empty vector if succeeded, a message if a render is in progress,
    /// or messages if starting the render fails.
    [[nodiscard]] std::vector<QString> start_render(QString const& path);
};

