#pragma once

#include <QDialog>
#include <QTimer>

#include <memory>
#include <vector>

class MainWindow;
class Backend;

class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTreeView;
class JobModel;

class RenderDialog : public QDialog {
private:
    Backend * _backend;
    MainWindow * _win;
    JobModel * _model;

    QProgressBar * _progress;
    QTreeView * _job_list;
    QPlainTextEdit * _error_log;
    QPushButton * _cancel_close;

    QTimer _status_timer;
    bool _done = false;
    bool _close_on_end = false;

public:
    RenderDialog(Backend * backend, MainWindow * parent_win);
    ~RenderDialog();

private:
    void update_status();
    void cancel_or_close();

// impl QWidget
protected:
    void closeEvent(QCloseEvent * event) override;
};
