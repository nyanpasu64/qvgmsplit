#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    auto w = MainWindow::new_with_path("");
    w->show();
    return a.exec();
}
