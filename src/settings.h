#pragma once

#include "lib/copy_move.h"

#include <memory>
#include <cstdint>

struct SettingsData;

struct AppSettings {
    /// Whether to use the FM sampling rate (if present) rather than the user-selected
    /// sampling rate.
    bool use_chip_rate;

    /// The fallback sampling rate to use if no FM chips are present.
    uint32_t sample_rate;
};

class Settings {
    std::unique_ptr<SettingsData> _data;

private:
    Settings(std::unique_ptr<SettingsData> data);

public:
    static Settings make();
    ~Settings();

    DEFAULT_MOVE(Settings)

    AppSettings const& app_settings() const;
    void set_app_settings(AppSettings app);
};
