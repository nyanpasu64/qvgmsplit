#pragma once

#include <QDialog>

class Backend;
class MainWindow;

class OptionsDialog : public QDialog {
    Q_OBJECT

protected:
    OptionsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {}

public:
    static OptionsDialog * make(Backend * backend, MainWindow * parent_main);
};

