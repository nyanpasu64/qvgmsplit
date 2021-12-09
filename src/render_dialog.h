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
class QListView;
class JobModel;

#ifndef render_dialog_INTERNAL
#define render_dialog_INTERNAL  private
#endif

class RenderDialog : public QDialog {
render_dialog_INTERNAL:
    Backend * _backend;
    MainWindow * _win;
    JobModel * _model;

    QProgressBar * _progress;
    QListView * _job_list;
    QPlainTextEdit * _error_log;
    QPushButton * _cancel_close;

    QTimer _status_timer;
    bool _done = false;
    bool _close_on_end = false;

public:
    RenderDialog(Backend * backend, MainWindow * parent_win);
    ~RenderDialog();

render_dialog_INTERNAL:
    void update_status();
    void cancel_or_close();

// impl QWidget
protected:
    void closeEvent(QCloseEvent * event) override;
};



