#include "render_dialog.h"
#include "mainwindow.h"
#include "backend.h"
#include "lib/layout_macros.h"
#include "lib/release_assert.h"

#include <QBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>

#include <QTreeView>
#include <QAbstractTableModel>

#include <QDebug>
#include <QFutureWatcher>

static QString format_duration(int seconds) {
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 0, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

struct ProgressState {
    int curr;
    int max;
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
            _backend->render_jobs().size(), ProgressState{0, 1, false, false, false}
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
            .finished = job.future.isFinished(),
            .error = job.future.isResultReadyAt(0),
            .canceled = job.future.isCanceled(),
        });
    }

    return progress;
}

RenderDialog::RenderDialog(Backend *backend, MainWindow *parent_win)
    : QDialog(parent_win)
    , _backend(backend)
    , _win(parent_win)
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
        this, &RenderDialog::cancel_or_close);

    // Setup status timer.
    _status_timer.setInterval(200);
    connect(
        &_status_timer, &QTimer::timeout,
        this, &RenderDialog::update_status);

    _status_timer.start();
    update_status();
}

RenderDialog::~RenderDialog() = default;

/// Called on a timer. Updates the progress table and checks if all render jobs are
/// complete. If so, closes the dialog or switches the Cancel button to Close.
///
/// Does not check for errors and append them to the text area. That's handled by
/// QFutureWatcher.
void RenderDialog::update_status() {
    auto job_progress = get_progress(_backend);

    bool all_finished = true;
    bool any_error = false;
    bool any_canceled = false;
    int curr_progress = 0;
    int max_progress = 0;

    for (auto const& job : job_progress) {
        curr_progress += job.curr;
        // Treat errored jobs as completed (max := curr).
        max_progress += job.error ? job.curr : job.max;

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
    if (all_finished) {
        _done = true;
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
            }
            _progress->setValue(max_progress);
        }

        if (_close_on_end) {
            close();
        } else {
            setWindowTitle(tr("Render Complete"));
            _cancel_close->setText(tr("Close"));
        }
    }
}

void RenderDialog::cancel_or_close() {
    if (_done) {
        close();
    } else {
        _backend->cancel_render();
    }
}

void RenderDialog::closeEvent(QCloseEvent * event) {
    if (!_done) {
        event->ignore();
        _backend->cancel_render();
        _close_on_end = true;
    } else {
        // Technically unnecessary.
        event->accept();
    }
}
