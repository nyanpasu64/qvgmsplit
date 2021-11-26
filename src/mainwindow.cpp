#include "mainwindow.h"
#include "backend.h"
#include "lib/layout_macros.h"

// Layout
#include <QBoxLayout>
#include <QMenuBar>
#include <QToolBar>

// Qt
#include <QFileDialog>

class MainWindowImpl : public MainWindow {
    Backend _backend;

    QAction * _open;
    QAction * _render;
    QAction * _exit;

    QString _path;

public:
    MainWindowImpl(QWidget *parent, QString path)
        : MainWindow(parent)
        , _path(std::move(path))
    {
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
        // TODO PlayerBase::GetDeviceOptions(UINT32 id, PLR_DEV_OPTS& devOpts) const = 0;
        _backend.load_path(_path);
    }

    void on_open() {
        // TODO save recent dirs, using SQLite or QSettings
        auto path = QFileDialog::getOpenFileName(
            this,
            tr("Open File"),
            QString(),
            tr("VGM files (*.vgm);;All files (*)"));

        if (path.isEmpty()) {
            return;
        }

        // TODO open_path(std::move(path));
    }

    void on_render() {

    }
};

std::unique_ptr<MainWindow> MainWindow::new_with_path(QString path) {
    return std::make_unique<MainWindowImpl>(nullptr, std::move(path));
}
