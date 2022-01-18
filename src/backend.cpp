#include "backend.h"
#include "mainwindow.h"
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
#include <emu/SoundDevs.h>
#include <emu/SoundEmu.h>

#include <stx/result.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureInterface>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>

#include <atomic>
#include <algorithm>  // std::stable_sort
#include <cstdint>
#include <iostream>
#include <optional>
#include <unordered_map>

//#define BACKEND_DEBUG

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

static bool compare_chips(PLR_DEV_INFO const& a, PLR_DEV_INFO const& b) {
    static constexpr auto key = [](uint8_t type) -> int {
        // Order PSG after YM2612. Keep PSG before 32X, because base console chips
        // should come before expansions.
        static_assert(DEVID_SN76496 < DEVID_YM2612, "PSG already after YM2612");
        if (type == DEVID_SN76496) {
            return (int) (DEVID_YM2612 << 8) + 1;
        }

        // Add more overrides as necessary.
        return (int) (type << 8);
    };
    return key(a.type) < key(b.type);
}

/// Sort chips in a more natural order for end users.
static void sort_chips(std::vector<PLR_DEV_INFO> & devices) {
    std::stable_sort(devices.begin(), devices.end(), compare_chips);
}

struct Metadata {
    // If zero, no file is loaded.
    uint32_t player_type;

    std::vector<ChipMetadata> chips;
    std::vector<FlatChannelMetadata> flat_channels;

    uint32_t sample_rate;

    /// Master audio renders more slowly than individual channels, so make it take up
    /// more space in the overall progress bar.
    float master_audio_time_multiplier;

// impl
public:
    /// Calls load_settings().
    static Result<std::unique_ptr<Metadata>, QString> make(
        QByteArray file_data, AppSettings const& app
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
        sort_chips(devices);

        std::vector<ChipMetadata> chips;
        std::vector<FlatChannelMetadata> flat_channels = {
            FlatChannelMetadata {
                .name = "Master Audio",
                .maybe_chip_id = NO_CHIP,
                .subchip_idx = 0,
                .chan_idx = 0,
                .enabled = true,
            }
        };

        chips.reserve(devices.size());
        bool show_chip_name = devices.size() > 1;

        for (auto const& device : devices) {
            constexpr UINT8 OPTS = 0x01;  // enable long names
            const char* chipName = SndEmu_GetDevName(device.type, OPTS, device.devCfg);
#ifdef BACKEND_DEBUG
            qDebug() << chipName;
#endif

            ChipId chip_id =
                PLR_DEV_ID((uint32_t) device.type, (uint32_t) device.instance);
            chips.push_back(ChipMetadata {
                .name = chipName,
                .chip_id = chip_id,
            });

            for (
                ChannelMetadata & channel :
                get_chip_metadata(device, show_chip_name)
            ) {
                flat_channels.push_back(FlatChannelMetadata {
                    .name = move(channel.name),
                    .maybe_chip_id = chip_id,
                    .subchip_idx = channel.subchip_idx,
                    .chan_idx = channel.chan_idx,
                    .enabled = true,
                });
            }
        }

        /*
        Calculate master_audio_time_multiplier based off the total number of channels,
        not the number of channels enabled in a particular render.

        In my testing with OPL3 and 32X .vgm files, rendering the master audio takes
        around (total channels / 2) as long as rendering a single channel.

        In Master System SN76489-like .vgm files, rendering the master audio is no
        slower than a single channel, so this calculation is wrong. But it's not a big
        deal, since Master System .vgm files render quickly, and have ≤ channels than
        most people have CPU cores.
        */
        float master_audio_time_multiplier =
            std::max(1.f, (float) flat_channels.size() / 2.f);

        auto out = std::make_unique<Metadata>(Metadata {
            .player_type = engine->GetPlayerType(),
            .chips = move(chips),
            .flat_channels = move(flat_channels),
            .sample_rate = 0,
            .master_audio_time_multiplier = master_audio_time_multiplier,
        });
        // Destroy the PlayerA object before loading settings, to reduce peak RAM usage.
        player.reset();

        // This constructs a new PlayerA object. This is slower than optimal, but
        // avoids separate codepaths for loading a file, and changing settings after
        // opening a file, which may behave differently.
        auto err = out->load_settings(file_data, app);
        if (!err.isEmpty()) {
            return Err(move(err));
        }

        return Ok(move(out));
    }

    bool is_file_loaded() const {
        return this->player_type != 0;
    }

    /// If non-empty, holds error message.
    [[nodiscard]] QString load_settings(
        QByteArray const& file_data, AppSettings const& app
    ) {
        // Set this->sample_rate.
        if (!is_file_loaded()) {
            this->sample_rate = 0;
        } else if (app.use_chip_rate) {
            PlayerA player;

            std::unique_ptr<PlayerBase> engine_move;

            switch (player_type) {
            case FCC_VGM: engine_move = std::make_unique<VGMPlayer>(); break;
            case FCC_S98: engine_move = std::make_unique<S98Player>(); break;
            case FCC_DRO: engine_move = std::make_unique<DROPlayer>(); break;
            case FCC_GYM: engine_move = std::make_unique<GYMPlayer>(); break;
            default:
                return Backend::tr("Failed to load settings, unrecognized file type 0x%1")
                    .arg(QString::number(player_type, 16).toUpper());
            }

            UINT8 status;

            auto loader = BoxDataLoader(MemoryLoader_Init(
                (UINT8 const*) file_data.data(), (UINT32) file_data.size()
            ));
            if (loader == nullptr) {
                return Backend::tr("Failed to allocate MemoryLoader_Init");
            }

            DataLoader_SetPreloadBytes(loader.get(), 0x100);
            status = DataLoader_Load(loader.get());
            if (status) {
                return Backend::tr("Failed to extract file, error 0x%1")
                    .arg(format_hex_2(status));
            }

            // Register the correct playback engine. This saves memory compared to
            // creating 4 different engines for each channel in the file.
            player.RegisterPlayerEngine(engine_move.release());

            // Load the file using the playback engine.
            status = player.LoadFile(loader.get());
            if (status) {
                return Backend::tr("Failed to load file, error 0x%1")
                    .arg(format_hex_2(status));
            }
            player.Start();

            PlayerBase * engine = player.GetPlayer();
            std::vector<PLR_DEV_INFO> devices;
            engine->GetSongDeviceInfo(devices);
            sort_chips(devices);

#ifdef BACKEND_DEBUG
            for (PLR_DEV_INFO const& device : devices) {
                qDebug() << device.smplRate;
            }
#endif

            bool done = false;
            for (PLR_DEV_INFO const& device : devices) {
                // Avoid writing WAV files with sampling rates above 100 KHz.
                //
                // TODO add an alternative mode for recording each channel with its
                // exact sampling rate (except that 32X has variable sampling rate)?
                //
                // TODO add a setting toggle/integer for "maximum sampling rate to
                // auto-detect"?
                if (device.smplRate <= 100'000) {
                    this->sample_rate = device.smplRate;
                    done = true;
                    break;
                }
            }
            if (!done) {
                this->sample_rate = app.sample_rate;
            }
        } else {
            this->sample_rate = app.sample_rate;
        }

        return QString();
    }
};

QString FlatChannelMetadata::numbered_name(size_t row) const {
    // TODO maybe pass in a "show numbers" setting?

    auto channel_name = QString::fromStdString(name);
    if (row > 0) {
        // Pad the channel number with leading zeros, so the exported .wav files are
        // ordered correctly when played in Winamp or dragged into Audacity.
        channel_name = QStringLiteral("%1 - %2")
            .arg(row, 2, 10, QChar('0'))
            .arg(channel_name);
    }
    return channel_name;
}

struct SoloSettings {
    /// Encodes the chip type and which index it is.
    ChipId chip_id;

    /// Usually 0. YM2608's PSG channels have it set to 1.
    uint8_t subchip_idx;

    /// ChannelMetadata with the same subchip_idx are grouped together
    /// and have chan_idx monotonically increasing from 0.
    uint8_t chan_idx;
};

struct RenderSettings {
    std::optional<SoloSettings> solo;

    uint32_t sample_rate;

    /// Q15.16 signed floating point value. 0x1'0000 is 100% volume.
    int32_t volume = 0x1'0000;

    uint32_t loop_count;

    /// The fadeout duration for looped songs. In seconds.
    float fade_duration = 4.0;
    /// How long to keep playing after the last command, for unlooped songs. In seconds.
    float unlooped_tail = 1.0;

    // TODO duration override?
};

struct RenderJobState {
    /// Only shown for debugging purposes.
    QString _name;

    QString _out_path;

    float _time_multiplier;

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
        QString name,
        QString out_path,
        QByteArray file_data,
        Metadata const& metadata,
        RenderSettings const& opt)
    {
        std::unique_ptr<PlayerBase> engine_move;

        switch (metadata.player_type) {
        case FCC_VGM: engine_move = std::make_unique<VGMPlayer>(); break;
        case FCC_S98: engine_move = std::make_unique<S98Player>(); break;
        case FCC_DRO: engine_move = std::make_unique<DROPlayer>(); break;
        case FCC_GYM: engine_move = std::make_unique<GYMPlayer>(); break;
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

        /* setup the player's output parameters and allocate internal buffers */
        if (player->SetOutputSettings(
            opt.sample_rate, CHANNEL_COUNT, BIT_DEPTH, BUFFER_LEN
        )) {
            return Err(Backend::tr(
                "Unsupported channel count/bit depth (this should never happen)"
            ));
        }

        // Register the correct playback engine. This saves memory compared to
        // creating 4 different engines for each channel in the file.
        player->RegisterPlayerEngine(engine_move.release());

        // Load the file using the playback engine.
        status = player->LoadFile(loader.get());
        if (status) {
            return Err(Backend::tr("Failed to load file, error 0x%1")
                .arg(format_hex_2(status)));
        }

        PlayerBase * engine = player->GetPlayer();

        bool is_song_looped = engine->GetLoopTicks() > 0;
        uint32_t extra_nsamp;

        /* set playback parameters */
        {
            PlayerA::Config cfg = player->GetConfiguration();
            cfg.masterVol = opt.volume;
            cfg.loopCount = is_song_looped ? opt.loop_count : 0;
            cfg.fadeSmpls = is_song_looped
                ? (uint32_t) ((float) opt.sample_rate * opt.fade_duration)
                : 0u;
            cfg.endSilenceSmpls = is_song_looped
                ? 0u
                : (uint32_t) ((float) opt.sample_rate * opt.unlooped_tail);
            cfg.pbSpeed = 1.0;

            extra_nsamp = cfg.fadeSmpls + cfg.endSilenceSmpls;
            player->SetConfiguration(cfg);
        }

        if (metadata.player_type == FCC_VGM) {
            VGMPlayer* vgmplay = dynamic_cast<VGMPlayer *>(engine);
            release_assert(vgmplay);
            player->SetLoopCount(vgmplay->GetModifiedLoopCount(opt.loop_count));
        }

        // Calling PlayerBase::SetDeviceMuting() with channel indices fails if you
        // haven't called PlayerA::Start(). (Calling PlayerBase::SetDeviceMuting() with
        // PLR_DEV_ID(chip, instance) works before calling PlayerA::Start().)
        //
        // It's not necessary to call Start() before GetTotalTime() (but it is
        // necessary to call it before Tick2Sample()).
        player->Start();

        /* figure out how many total frames we're going to render */
        uint32_t render_nsamp =
            engine->Tick2Sample(engine->GetTotalPlayTicks(player->GetLoopCount()))
            + extra_nsamp;

        float time_multiplier = 1.f;

        // Mute all but one channel.
        if (opt.solo) {
            SoloSettings const& solo = *opt.solo;

            for (ChipMetadata const& chip : metadata.chips) {
                PLR_MUTE_OPTS mute{};

                if (chip.chip_id != solo.chip_id) {
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
                status = engine->SetDeviceMuting(chip.chip_id, mute);
                assert(status == 0);
            }
        } else {
            time_multiplier = metadata.master_audio_time_multiplier;
        }

        auto out = std::make_unique<RenderJob>(RenderJobState {
            ._name = move(name),
            ._out_path = move(out_path),
            ._time_multiplier = time_multiplier,
            ._file_data = move(file_data),
            ._loader = move(loader),
            ._player = move(player),
        });
        // Progress range is [0..time in seconds].
        out->_status.setProgressRange(0, (int) (render_nsamp / opt.sample_rate));

        // The initial progress value is already 0, but we need to call
        // setProgressValue(0) so reportResult() doesn't increment the progress value.
        out->_status.setProgressValue(0);

        return Ok(std::move(out));
    }

    void start_consume(QThreadPool * pool) {
        // Based off https://invent.kde.org/qt/qt/qtbase/-/blob/kde/5.15/src/concurrent/qtconcurrentrunbase.h#L72-93.
        // I'm not sure why you need to call setThreadPool or setRunnable, especially
        // since Qt 6's QPromise (https://invent.kde.org/qt/qt/qtbase/-/blob/dev/src/corelib/thread/qpromise.h)
        // sets neither.
        _status.setThreadPool(pool);
        _status.setRunnable(this);
        _status.reportStarted();
        pool->start(this);
    }

    RenderJobHandle future() {
        return RenderJobHandle {
            .name = _name,
            .path = _out_path,
            .time_multiplier = _time_multiplier,
            .future = _status.future(),
        };
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

        uint32_t curr_samp = 0;
        int curr_progress = 0;

        bool done = false;
        while (!done) {
            if (_status.isCanceled()) {
                return;
            }

            std::fill(_buffer.begin(), _buffer.end(), 0);

            // Render audio.
            //
            // PlayerA::Render() takes buffer size in bytes, and returns bytes written.
            // We convert it into frames written.
            //
            // On song end, PlayerA::Render() performs a short write and sets
            // PlayerA::GetState() |= PLAYSTATE_FIN.
            uint32_t curr_frames =
                _player->Render(
                    BUFFER_LEN * CHANNEL_COUNT * (BIT_DEPTH / 8), _buffer.data()
                ) / CHANNEL_COUNT / (BIT_DEPTH / 8);
            if (_player->GetState() & PLAYSTATE_FIN) {
                done = true;
            }

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
            auto progress = (int) (curr_samp / sample_rate);
            if (progress != curr_progress) {
                // Only call setProgressValue() when progress has changed, to avoid
                // unnecessary mutex locking.
                _status.setProgressValue(progress);
                curr_progress = progress;
            }
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
            // Ideally I'd report "Cancelled by user", but after QFuture::cancel() is
            // called (and QFutureInterface::isCanceled() is set),
            // QFutureInterface::reportResult() drops all values.
            _status.reportFinished();
            return;
        }

        // Reduce the thread priority of the worker thread, to avoid slowing down the
        // entire PC on Windows.
        //
        // Previously I tried backing up QThread::priority() and restoring once
        // rendering finished. This didn't work since QThread::priority() returned
        // QThread::InheritPriority, which can't be passed to QThread::setPriority().

        QThread::currentThread()->setPriority(QThread::LowestPriority);

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
    : _settings(Settings::make())
    , _metadata(std::make_unique<Metadata>(Metadata {}))
{
}

Settings & Backend::settings_mut(StateTransaction & tx) {
    tx.settings_changed();
    return _settings;
}

Backend::~Backend() = default;

QString Backend::load_path(QString const& path) {
    if (is_rendering()) {
        return tr("Cannot load path while rendering");
    }
    _render_jobs.clear();

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
        auto result = Metadata::make(file_data, _settings.app_settings());
        if (result.is_err()) {
            return move(result.err_value());
        }

        _file_data = move(file_data);
        _metadata = move(result.value());
    }

#ifdef BACKEND_DEBUG
    for (auto const& metadata : _metadata->flat_channels) {
        qDebug()
            << QStringLiteral("0x%1")
                .arg(metadata.maybe_chip_id, 8, 16, QLatin1Char('0'))
            << metadata.subchip_idx
            << metadata.chan_idx
            << QString::fromStdString(metadata.name);
    }
#endif

    return {};
}

void Backend::reload_settings() {
    QString err = _metadata->load_settings(_file_data, _settings.app_settings());

    // Assert on debug builds.
    assert(err.isEmpty());
}

std::vector<ChipMetadata> const& Backend::chips() const {
    return _metadata->chips;
}

std::vector<ChipMetadata> & Backend::chips_mut() {
    return _metadata->chips;
}

void Backend::sort_channels() {
    auto const& chips = _metadata->chips;
    std::unordered_map<ChipId, uint16_t> chip_id_to_order(chips.size() + 1);
    chip_id_to_order.insert({NO_CHIP, 0});

    for (auto const& [i, chip] : enumerate<uint8_t>(chips)) {
        chip_id_to_order.insert({chip.chip_id, i + 1});
    }

    auto & channels = _metadata->flat_channels;

    std::stable_sort(
        channels.begin(), channels.end(),
        [&chip_id_to_order](FlatChannelMetadata const& a, FlatChannelMetadata const& b) {
            return
                chip_id_to_order.at(a.maybe_chip_id)
                < chip_id_to_order.at(b.maybe_chip_id);
        });
}

bool Backend::is_file_loaded() const {
    return _metadata->is_file_loaded();
}

uint32_t Backend::sample_rate() const {
    return _metadata->sample_rate;
}

std::vector<FlatChannelMetadata> const& Backend::channels() const {
    return _metadata->flat_channels;
}

std::vector<FlatChannelMetadata> & Backend::channels_mut() {
    return _metadata->flat_channels;
}

std::vector<RenderJobHandle> const& Backend::render_jobs() const {
    return _render_jobs;
}

bool Backend::is_rendering() const {
    for (RenderJobHandle const& job : _render_jobs) {
        if (!job.future.isFinished()) {
            return true;
        }
    }
    return false;
}

void Backend::cancel_render() {
    for (RenderJobHandle & job : _render_jobs) {
        job.future.cancel();
    }
}

std::vector<QString> Backend::start_render(QString const& path) {
    if (is_rendering()) {
        return {tr("Cannot start render while render is active")};
    }

    _render_jobs.clear();

    // The number of cores (or hyper-threads).
    int cores = QThread::idealThreadCount();

    _render_thread_pool.setMaxThreadCount(cores);

    std::vector<QString> errors;
    std::vector<std::unique_ptr<RenderJob>> queued_jobs;

    for (auto const& [chan_idx, channel] : enumerate<size_t>(_metadata->flat_channels)) {
        if (!channel.enabled) {
            continue;
        }

        std::optional<SoloSettings> solo;
        auto const channel_name = channel.numbered_name(chan_idx);

        QString channel_path;

        // For per-channel jobs, solo the channel.
        if (channel.maybe_chip_id != NO_CHIP) {
            solo = SoloSettings {
                .chip_id = channel.maybe_chip_id,
                .subchip_idx = channel.subchip_idx,
                .chan_idx = channel.chan_idx,
            };
            auto info = QFileInfo(path);
            channel_path = info.dir()
                .absoluteFilePath(QStringLiteral("%1 - %2.wav").arg(
                    info.baseName(), channel_name
                ));
        } else {
            channel_path = path;
        }

        auto settings = RenderSettings {
            .solo = solo,
            .sample_rate = _metadata->sample_rate,
            .loop_count = 2,
        };
        auto job = RenderJob::make(
            channel_name, move(channel_path), _file_data, *_metadata, settings
        );
        if (job.is_err()) {
            errors.push_back(tr("Error rendering %1: %2")
                .arg(channel_name, job.err_value()));
        } else {
            queued_jobs.push_back(move(job.value()));
        }
    }

    if (!errors.empty()) {
        return errors;
    }

    for (auto & job : queued_jobs) {
        _render_jobs.push_back(job->future());
        job.release()->start_consume(&_render_thread_pool);
    }
    return errors;
}
