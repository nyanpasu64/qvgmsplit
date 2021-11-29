#pragma once

// Do *not* include any other widgets in this header and create an include cycle.
// Other widgets include app.h, since they rely on GuiApp for data/signals.

#include <QApplication>

class GuiApp : public QApplication {
    Q_OBJECT

    /*
    On Windows, QFont defaults to MS Shell Dlg 2, which is Tahoma instead of Segoe UI,
    and also HiDPI-incompatible.

    Running `QApplication::setFont(QApplication::font("QMessageBox"))` fixes this,
    but the code must run after QApplication is constructed (otherwise MS Sans Serif),
    but before we construct and save any QFonts based off the default font.
    */
public:
    GuiApp(int &argc, char **argv, int = ApplicationFlags);

    static QString app_name();
};
