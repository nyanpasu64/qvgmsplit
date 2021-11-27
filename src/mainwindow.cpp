#include "mainwindow.h"
#include "backend.h"
#include "lib/layout_macros.h"

// Layout
#include <QBoxLayout>
#include <QMenuBar>
#include <QToolBar>

// Qt
#include <QErrorMessage>
#include <QFileDialog>
#include <QFileInfo>

#include <utility>  // std::move

static void setup_error_dialog(QErrorMessage & dialog) {
    static constexpr int W = 640;
    static constexpr int H = 360;
    dialog.resize(W, H);
    dialog.setModal(true);
}


class MainWindowImpl : public MainWindow {
    Backend _backend;

    QErrorMessage _error_dialog{this};

    QAction * _open;
    QAction * _render;
    QAction * _exit;

    QString _file_title;
    QString _file_path;

public:
    MainWindowImpl(QWidget *parent, QString path)
        : MainWindow(parent)
        , _file_path(std::move(path))
    {
        setup_error_dialog(_error_dialog);

        // Setup GUI layout

        auto main = this;

        {main__m();
            {m__m(tr("&File"));
                {m__action(tr("&Open"));
                    _open = a;
                }
                {m__action(tr("&Render"));
                    _render = a;
                }
                {m__action(tr("E&xit"));
                    _exit = a;
                }
            }
        }

        {main__tb(QToolBar);
            tb->setMovable(false);

            tb->addAction(_open);
            tb->addAction(_render);
        }

        {main__central_c_l(QWidget, QVBoxLayout);
        }

        // Bind actions
        _open->setShortcuts(QKeySequence::Open);
        connect(_open, &QAction::triggered, this, &MainWindowImpl::on_open);

        _render->setShortcut(QKeySequence(tr("Ctrl+R")));
        connect(_render, &QAction::triggered, this, &MainWindowImpl::on_render);

        _exit->setShortcuts(QKeySequence::Quit);
        connect(_exit, &QAction::triggered, this, &MainWindowImpl::close);

        // TODO load file
    }

    void load_path() {
        _file_title = (!_file_path.isEmpty())
            ? QFileInfo(_file_path).fileName()
            : tr("Untitled");

        if (!_file_title.isEmpty()) {
            setWindowTitle(QStringLiteral("%1[*] - %2").arg(
                _file_title, "qvgmsplit"  // TODO add central/translated app name
            ));
        } else {
            setWindowTitle(QStringLiteral("qvgmsplit"));
        }

        setWindowFilePath(_file_path);
        // setWindowModified(false);

        auto err = _backend.load_path(_file_path);
        if (!err.isEmpty()) {
            _error_dialog.close();
            _error_dialog.showMessage(err);
        }
    }

    void on_open() {
        // TODO save recent dirs, using SQLite or QSettings
        auto path = QFileDialog::getOpenFileName(
            this,
            tr("Open File"),
            QString(),
            tr("VGM files (*.vgm *.vgz);;All files (*)"));

        if (path.isEmpty()) {
            return;
        }

        _file_path = std::move(path);
        load_path();
    }

    void on_render() {

    }
};

std::unique_ptr<MainWindow> MainWindow::new_with_path(QString path) {
    return std::make_unique<MainWindowImpl>(nullptr, std::move(path));
}
