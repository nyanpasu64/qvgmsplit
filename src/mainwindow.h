#pragma once

#include <QMainWindow>

#include <memory>

class MainWindow : public QMainWindow {
    Q_OBJECT

protected:
    // MainWindow()
    using QMainWindow::QMainWindow;

public:
    /// If path is empty, ignored.
    static std::unique_ptr<MainWindow> new_with_path(QString path);
};
