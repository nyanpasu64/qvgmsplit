#include "mainwindow.h"
#include "lib/layout_macros.h"

#include <QBoxLayout>
#include <QMenuBar>
#include <QToolBar>

class MainWindowImpl : public MainWindow {
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
        _exit->setShortcuts(QKeySequence::Quit);
        connect(_exit, &QAction::triggered, this, &MainWindowImpl::close);
    }

};

std::unique_ptr<MainWindow> MainWindow::new_with_path(QString path) {
    return std::make_unique<MainWindowImpl>(nullptr, std::move(path));
}
