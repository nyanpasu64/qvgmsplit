#include "gui_app.h"

#include <QFont>
#include <QScreen>
#include <QWindow>

static void win32_set_font() {
    #if defined(_WIN32) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // On Windows, Qt 5's default system font (MS Shell Dlg 2) is outdated.
    // Interestingly, the QMessageBox font is correct and comes from lfMessageFont
    // (Segoe UI on English computers).
    // So use it for the entire application.
    // This code will become unnecessary and obsolete once we switch to Qt 6.
    QApplication::setFont(QApplication::font("QMessageBox"));
    #endif
}

/// Emits QWindow::screenChanged() for a window and all (MDI?) child windows.
static void recursive_screenChanged(QWindow * win) {
    QScreen *screen = win->screen();
    emit win->screenChanged(screen);
    for (QObject *child : win->children()) {
        if (child->isWindowType())
            recursive_screenChanged(static_cast<QWindow *>(child));
    }
}

/// Hook a QScreen to emit QWindow::screenChanged() for all windows within the screen,
/// whenever the QScreen changes DPI.
static void emit_screenChanged_upon_dpi_changed(QScreen * screen) {
    // We can't emit QWindow::screenChanged() too early,
    // since it only works after QScreenPrivate::updateGeometriesWithSignals() and
    // QGuiApplicationPrivate::resetCachedDevicePixelRatio() have been called.
    // So use a QueuedConnection.
    //
    // Unfortunately we can't use a UniqueConnection:
    // (https://doc.qt.io/qt-5/qobject.html#connect)
    // > Qt::UniqueConnections do not work for lambdas, non-member functions
    // > and functors; they only apply to connecting to member functions.
    //
    // So far I've only seen each QScreen get QGuiApplication::screenAdded() once,
    // but just in case it happens multiple times, ensure we only connect once.
    static constexpr char const* SCREEN_DPI_CONNECTED = "exo_dpi_connected";
    if (screen->property(SCREEN_DPI_CONNECTED).toBool()) {
        return;
    }
    screen->setProperty(SCREEN_DPI_CONNECTED, true);

    QObject::connect(
        screen, &QScreen::logicalDotsPerInchChanged,
        screen, [screen] {
            const auto allWindows = QGuiApplication::allWindows();
            for (QWindow * window : allWindows) {
                if (!window->isTopLevel() || window->screen() != screen)
                    continue;
                recursive_screenChanged(window);
            }
        },
        Qt::QueuedConnection);

    // It may be worth emitting QWindow::screenChanged()
    // on other QScreen::fooChanged() properties as well.
    // This will fix applications which only pick up resolution changes
    // upon QWindow::screenChanged().
    // However it's not necessary to fix Qt Widgets itself.
};

/// Hook all current and future QScreen objects
/// so they emit QWindow::screenChanged whenever the screen's logical DPI changes.
/// This is a workaround for a bug (https://bugreports.qt.io/browse/QTBUG-95925)
/// where QWidget only relayouts when changing QScreen,
/// not when a QScreen's DPI changes.
static void hook_all_screens(QGuiApplication & app) {
    // Note that we don't need to emit QWindow::screenChanged right away,
    // only when the DPI property changes.

    // Hook all existing QScreen.
    auto screens = QGuiApplication::screens();
    for (QScreen * screen : qAsConst(screens)) {
        emit_screenChanged_upon_dpi_changed(screen);
    }

    // Hook every QScreen added later.
    // This *should* not hook the same QScreen twice,
    // since every QScreen gets screenAdded() exactly once in my testing.
    QObject::connect(
        &app, &QGuiApplication::screenAdded,
        &app, emit_screenChanged_upon_dpi_changed);
}

GuiApp::GuiApp(int &argc, char **argv, int flags)
    : QApplication(argc, argv, flags)
{
    win32_set_font();
    hook_all_screens(*this);
}

QString GuiApp::app_name() {
    return tr("qvgmsplit");
}
