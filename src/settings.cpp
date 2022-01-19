#include "settings.h"

#include <QSettings>

#include <utility>  // std::move

using std::move;

struct SettingsData {
    QSettings persist;
    AppSettings app;
};

Settings::Settings(std::unique_ptr<SettingsData> data)
    : _data(move(data))
{}

/// Read the current value from QSettings. If missing or invalid, overwrite it with
/// default_.
static uint32_t sync_u32(QSettings & settings, QString const& key, uint32_t default_) {
    bool ok;
    auto val = settings.value(key).toUInt(&ok);
    if (ok) {
        return val;
    } else {
        settings.setValue(key, default_);
        return default_;
    }
}

/// Read the current value from QSettings. If missing or invalid, overwrite it with
/// default_.
static bool sync_bool(QSettings & settings, QString const& key, bool default_) {
    // When converting from QString to bool, QVariant's API makes it impossible to
    // distinguish between valid and invalid boolean string literals. This makes it
    // impossible to fallback to default_ when the settings contains an invalid string
    // value.
    auto var = settings.value(key);
    if (var.isValid()) {
        return var.toBool();
    } else {
        settings.setValue(key, default_);
        return default_;
    }
}

static const QString APP_USE_CHIP_RATE = QStringLiteral("app/use_chip_rate");
static const QString APP_SAMPLE_RATE = QStringLiteral("app/sample_rate");

/// Read the current settings from the system. If certain settings are missing or
/// invalid, overwrite them with defaults.
static void load_app_settings(SettingsData & data) {
    QSettings & persist = data.persist;

    data.app = AppSettings {
        .use_chip_rate = sync_bool(persist, APP_USE_CHIP_RATE, true),
        .sample_rate = sync_u32(persist, APP_SAMPLE_RATE, 44100),
    };
}

Settings Settings::make() {
    auto data = std::unique_ptr<SettingsData>(new SettingsData {
        .persist = QSettings(
            QSettings::IniFormat,
            QSettings::UserScope,
            QStringLiteral("qvgmsplit"),
            QStringLiteral("qvgmsplit")),
        .app = {},
    });

    load_app_settings(*data);

    return Settings(move(data));
}

AppSettings const& Settings::app_settings() const {
    return _data->app;
}

void Settings::set_app_settings(AppSettings app) {
    // Even if settings are unchanged, persist the data. This makes changing settings
    // in multiple simultaneous instances more predictable (last change wins), at least
    // until we implement opening multiple windows from a single process.

    _data->app = app;
    _data->persist.setValue(APP_USE_CHIP_RATE, _data->app.use_chip_rate);
    _data->persist.setValue(APP_SAMPLE_RATE, _data->app.sample_rate);
}

Settings::~Settings() = default;
