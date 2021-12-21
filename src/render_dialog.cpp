#include "render_dialog.h"
#include "mainwindow.h"
#include "backend.h"
#include "lib/layout_macros.h"
#include "lib/release_assert.h"

#include <QBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>

#include <QTreeView>
#include <QAbstractTableModel>

#include <QDebug>
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QPointer>

static QString format_duration(int seconds) {
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 0, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

struct ProgressState {
    int curr;
    int max;
    float time_multiplier;
    bool finished;
    bool error;
    bool canceled;
};

class JobModel : public QAbstractTableModel {
    Backend * _backend;
    std::vector<ProgressState> _progress;

public:
    enum Column {
        NameColumn,
        ProgressColumn,
        COLUMN_COUNT,
    };

public:
    JobModel(Backend *backend, QObject *parent = nullptr)
        : QAbstractTableModel(parent)
        , _backend(backend)
        , _progress(
            _backend->render_jobs().size(), ProgressState{0, 1, 1, false, false, false}
        )
    {}

    void set_progress(std::vector<ProgressState> const& progress) {
        release_assert_equal(progress.size(), _progress.size());
        _progress = progress;
        emit dataChanged(
            index(0, ProgressColumn),
            index((int) (_progress.size() - 1), ProgressColumn));
    }

// impl QAbstractItemModel
public:
    int rowCount(const QModelIndex &parent) const override {
        if (parent.isValid()) {
            return 0;
        }
        return (int) _backend->render_jobs().size();
    }

    int columnCount(const QModelIndex &parent) const override {
        if (parent.isValid()) {
            return 0;
        }
        return COLUMN_COUNT;
    }

    QVariant headerData(
        int section, Qt::Orientation orientation, int role
    ) const override {
        if (orientation != Qt::Horizontal) {
            return {};
        }
        switch (role) {
        case Qt::DisplayRole:
            switch (section) {
            case NameColumn: return tr("Name");
            case ProgressColumn: return tr("Progress");
            default: return {};
            }
        default:
            return {};
        }
    }

    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.parent().isValid()) {
            return {};
        }
        auto column = index.column();
        if (column < 0 || column >= COLUMN_COUNT) {
            return {};
        }

        auto const& jobs = _backend->render_jobs();
        release_assert_equal(_progress.size(), jobs.size());

        auto row = index.row();
        if (row < 0 || row >= (int) jobs.size()) {
            return {};
        }

        switch (column) {
        case NameColumn:
            switch (role) {
            case Qt::DisplayRole:
                return jobs[(size_t) row].name;
            default:
                return {};
            }

        case ProgressColumn:
            switch (role) {
            case Qt::DisplayRole: {
                auto const& progress = _progress[(size_t) row];
                return tr("%1%, %2/%3")
                    .arg(progress.curr * 100 / progress.max)
                    .arg(format_duration(progress.curr), format_duration(progress.max));
            }
            default:
                return {};
            }

        default:
            return {};
        }
    }
};

/// Create a consistent view of job progress.
///
/// Invariants:
/// - If a job was canceled or ran into an error, and if finished == true, then
///   (error || cancelled) == true.
static std::vector<ProgressState> get_progress(Backend * backend) {
    std::vector<ProgressState> progress;

    auto const& jobs = backend->render_jobs();
    progress.reserve(jobs.size());
    for (auto const& job : jobs) {
        // RenderJob reports error/canceled before finished. Make sure to check finished
        // before checking error, so we don't see finished without error.
        progress.push_back(ProgressState {
            .curr = job.future.progressValue(),
            .max = job.future.progressMaximum(),
            .time_multiplier = job.time_multiplier,
            .finished = job.future.isFinished(),
            .error = job.future.isResultReadyAt(0),
            .canceled = job.future.isCanceled(),
        });
    }

    return progress;
}

class RenderDialogImpl : public RenderDialog {
private:
    Backend * _backend;
    JobModel * _model;

    QProgressBar * _progress;
    QTreeView * _job_list;
    QPlainTextEdit * _error_log;
    QPushButton * _cancel_close;

    QTimer _status_timer;
    bool _is_done = false;
    bool _has_errors = false;
    bool _close_on_end = false;

    QPointer<QMessageBox> _maybe_cancel_dialog;
    QPointer<QMessageBox> _maybe_done_dialog;

public:
    RenderDialogImpl(Backend * backend, MainWindow * parent_win);

private:
    /// May open done dialog, which calls `done_dialog_closed()`.
    void update_status();
    void done_dialog_closed();

    void cancel_close_clicked();

    /// Opens cancel dialog, which may call `cancel_dialog_accepted()`.
    void prompt_for_cancel();
    void cancel_dialog_accepted();

// impl QWidget
protected:
    void keyPressEvent(QKeyEvent * e) override;
    void closeEvent(QCloseEvent * event) override;
};


RenderDialogImpl::RenderDialogImpl(Backend *backend, MainWindow *parent_win)
    : RenderDialog(parent_win)
    , _backend(backend)
    , _model(new JobModel(_backend, this))
{
    setModal(true);
    setWindowTitle(tr("Rendering..."));
    resize(700, 900);

    auto c = this;
    auto l = new QVBoxLayout(c);

    {l__w(QProgressBar);
        _progress = w;
    }
    {l__splitl(QSplitter);
        l->setOrientation(Qt::Vertical);
        {l__c_l(QWidget, QVBoxLayout);
            l->setContentsMargins(0, 0, 0, 0);
            {l__w(QLabel(tr("Progress:")));
            }
            {l__w(QTreeView);
                _job_list = w;
                w->setModel(_model);
                // Hide the tree view on Linux, and unindent the rows on Windows. We're
                // displaying a table model where rows never have children.
                w->setRootIsDecorated(false);
                w->resizeColumnToContents(JobModel::NameColumn);
            }
        }
        {l__c_l(QWidget, QVBoxLayout);
            l->setContentsMargins(0, -1, 0, 0);
            {l__w(QLabel(tr("Errors:")));
            }
            {l__w(QPlainTextEdit);
                _error_log = w;
                w->setReadOnly(true);
            }
        }
    }
    {l__w(QPushButton(tr("Cancel")));
        _cancel_close = w;
    }

    int total_progress = 0;

    for (auto const& job : _backend->render_jobs()) {
        auto watch = new QFutureWatcher<QString>(this);
        connect(
            watch, &QFutureWatcher<QString>::resultReadyAt,
            this, [this, job=job](int idx) {
                _error_log->appendPlainText(
                    tr("Error rendering to \"%1\": %2").arg(
                        job.path, job.future.resultAt(idx)
                    )
                );
            });

        // "To avoid a race condition, it is important to call this function *after*
        // doing the connections."
        watch->setFuture(job.future);
        total_progress += job.future.progressMaximum();
    }

    _progress->setMaximum(total_progress);

    // Connect GUI.
    connect(
        _cancel_close, &QPushButton::clicked,
        this, &RenderDialogImpl::cancel_close_clicked);

    // Setup status timer.
    _status_timer.setInterval(50);
    connect(
        &_status_timer, &QTimer::timeout,
        this, &RenderDialogImpl::update_status);

    _status_timer.start();
    update_status();
}

/// Called on a timer. Updates the progress table and checks if all render jobs are
/// complete. If so, closes the dialog or switches the Cancel button to Close.
///
/// Does not check for errors and append them to the text area. That's handled by
/// QFutureWatcher.
void RenderDialogImpl::update_status() {
    auto job_progress = get_progress(_backend);

    bool all_finished = true;
    bool any_error = false;
    bool any_canceled = false;
    int curr_progress = 0;
    int max_progress = 0;

    for (auto const& job : job_progress) {
        curr_progress += (int) ((double) job.time_multiplier * (double) job.curr);

        // Treat errored jobs as completed (max := curr).
        int job_max = job.error ? job.curr : job.max;
        max_progress += (int) ((double) job.time_multiplier * (double) job_max);

        if (job.error) {
            any_error = true;
        }
        if (job.canceled) {
            any_canceled = true;
        }
        if (!job.finished) {
            all_finished = false;
        }
    }

    // Set the progress bar.
    if (max_progress == 0) {
        curr_progress = max_progress = 1;
    }
    _progress->setMaximum(max_progress);
    _progress->setValue(curr_progress);

    // Update the job list.
    _model->set_progress(job_progress);

    // Only use values calculated from job_progress. Don't perform more queries to
    // _backend, since you'll get an inconsistent view of rendering state.

    /*
    Currently RenderDialogImpl() calls update_status(), which updates _model. This
    ensures that when RenderDialog is later shown, it displays the correct channel
    durations from _model.

    If the render fails immediately (eg. when saving to a read-only path), then when
    RenderDialogImpl() calls update_status(), we cannot immediately create and show
    _maybe_done_dialog. On Xfce, showing the dialog with a hidden parent causes the
    dialog to appear at the wrong location.

    Instead, skip creating _maybe_done_dialog until RenderDialogImpl() returns and the
    caller shows RenderDialog.
    */
    if (all_finished && !isVisible()) {
        // This technically results in a CPU-burning loop if you never show the render
        // dialog, or hide it. I'll change this once it becomes a problem.
        _status_timer.setInterval(0);
        return;
    }

    if (all_finished) {
        _is_done = true;
        _has_errors = any_error;
        _status_timer.stop();

        // If a render job isn't canceled (runs to completion or hits an error), set
        // the progress bar to 100%. If it runs to completion, warn if there's a time
        // mismatch.
        if (!any_canceled) {
            int max_progress = _progress->maximum();
            if (!any_error && curr_progress != max_progress) {
                _error_log->appendPlainText(
                    tr("Warning: total rendered time of %1 seconds != calculated duration of %2 seconds!")
                        .arg(curr_progress)
                        .arg(max_progress)
                );
                _has_errors = true;
            }
            _progress->setValue(max_progress);
        }

        if (_close_on_end) {
            close();
        } else {
            setWindowTitle(any_canceled
                ? tr("Render Canceled")
                : tr("Render Complete"));
            _cancel_close->setText(tr("Close"));

            if (_maybe_cancel_dialog) {
                _maybe_cancel_dialog->close();
            }

            // TODO make done dialog optional
            if (!any_canceled) {
                if (_has_errors) {
                    _maybe_done_dialog = new QMessageBox(
                        QMessageBox::Warning,
                        tr("Render Completed With Errors"),
                        tr("Render complete! Errors were encountered."),
                        QMessageBox::Ok,
                        this);
                } else {
                    _maybe_done_dialog = new QMessageBox(
                        QMessageBox::Information,
                        tr("Render Complete"),
                        tr("Render complete!"),
                        QMessageBox::Ok,
                        this);
                }

                _maybe_done_dialog->setAttribute(Qt::WA_DeleteOnClose);
                connect(
                    _maybe_done_dialog, &QDialog::finished,
                    this, &RenderDialogImpl::done_dialog_closed);

                _maybe_done_dialog->show();
            }
        }
    }
}

void RenderDialogImpl::done_dialog_closed() {
    // When done dialog is closed, close render dialog if no errors occurred.
    if (!_has_errors) {
        close();
    }
}

/*
Currently:

- Clicking X or pressing Esc when rendering is incomplete prompts the user to cancel
  rendering. If the user accepts, all render jobs are cancelled and the render dialog
  closes when all jobs finish.
- Clicking Cancel cancels rendering without prompting, but leaves the render dialog
  open. This is subject to change (I may make it behave like clicking X).
- When all render jobs finish, the Cancel button changes to Close, and the "cancel
  rendering" popup is closed if open. If no jobs were cancelled, a "render complete"
  popup appears. If there were no errors, closing the popup closes the render dialog.
*/

void RenderDialogImpl::cancel_close_clicked() {
    if (_is_done) {
        close();
    } else {
        _backend->cancel_render();
        // Don't close the dialog when rendering finishes.
    }
}

void RenderDialogImpl::prompt_for_cancel() {
    _maybe_cancel_dialog = new QMessageBox(
        QMessageBox::Question,
        tr("Cancel Render"),
        tr("Cancel current render?"),
        QMessageBox::Yes | QMessageBox::No,
        this);

    _maybe_cancel_dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(
        _maybe_cancel_dialog, &QMessageBox::accepted,
        this, &RenderDialogImpl::cancel_dialog_accepted);

    _maybe_cancel_dialog->show();
}

void RenderDialogImpl::cancel_dialog_accepted() {
    _backend->cancel_render();
    _close_on_end = true;
}

void RenderDialogImpl::keyPressEvent(QKeyEvent * e) {
    // Based on https://invent.kde.org/qt/qt/qtbase/-/blob/kde/5.15/src/widgets/dialogs/qdialog.cpp#L703.

    // When the user presses Esc, QDialog::keyPressEvent() calls reject(), which
    // bypasses our closeEvent() hook and closes the dialog immediately. To prevent
    // this, we need to manually handle Esc keypresses with a replica of our
    // closeEvent() logic.

    if (e->matches(QKeySequence::Cancel)) {
        if (!_is_done) {
            prompt_for_cancel();
        } else {
            close();
        }
    } else {
        QDialog::keyPressEvent(e);
    }
}

void RenderDialogImpl::closeEvent(QCloseEvent * event) {
    if (!_is_done) {
        event->ignore();
        prompt_for_cancel();
    }
}

RenderDialog * RenderDialog::make(Backend * backend, MainWindow * parent_win) {
    return new RenderDialogImpl(backend, parent_win);
}
