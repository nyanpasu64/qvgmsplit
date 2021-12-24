#pragma once

#include <QDialog>
#include <QTimer>

#include <memory>
#include <vector>

class Backend;

class RenderDialog : public QDialog {
    Q_OBJECT

private:
    // RenderDialog()
    using QDialog::QDialog;
    // This is public despite being marked as private. See
    // https://stackoverflow.com/q/21015909.

public:
    static RenderDialog * make(Backend * backend, QWidget * parent = nullptr);
};
