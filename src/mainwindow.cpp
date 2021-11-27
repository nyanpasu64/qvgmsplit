#include "mainwindow.h"
#include "backend.h"
#include "lib/layout_macros.h"

// Layout
#include <QBoxLayout>
#include <QMenuBar>
#include <QToolBar>

// Model-view
#include <QAbstractTableModel>
#include <QMimeData>
#include <QTableView>

// Qt
#include <QErrorMessage>
#include <QFileDialog>
#include <QFileInfo>

#include <algorithm>  // std::rotate
#include <utility>  // std::move

static void setup_error_dialog(QErrorMessage & dialog) {
    static constexpr int W = 640;
    static constexpr int H = 360;
    dialog.resize(W, H);
    dialog.setModal(true);
}

class ChannelsModel final : public QAbstractTableModel {
public:
    enum Column {
        NameColumn,
        ProgressColumn,
        ColumnCount,
    };

    static constexpr int MASTER_AUDIO_ROW = 0;
    static constexpr int CHANNEL_0_ROW = 1;

private:
    Backend * _backend;
    bool _number_channels = true;

public:
    explicit ChannelsModel(Backend * backend, QObject *parent = nullptr)
        : _backend(backend)
    {}

    void set_number_channels(bool number_channels) {
        if (number_channels != _number_channels) {
            _number_channels = number_channels;
            emit dataChanged(
                index(CHANNEL_0_ROW, NameColumn),
                index((int) metadata().size() - 1, NameColumn));
        }
    }

    void begin_reset_model() {
        beginResetModel();
    }

    void end_reset_model() {
        endResetModel();
    }

private:
    std::vector<FlatChannelMetadata> const& metadata() const {
        return _backend->metadata();
    }

    std::vector<FlatChannelMetadata> & metadata_mut() {
        return _backend->metadata_mut();
    }

// impl QAbstractItemModel
public:
    int rowCount(QModelIndex const & parent) const override {
        if (parent.isValid()) {
            // Rows do not have children.
            return 0;
        } else {
            // The root has items.
            return int(metadata().size());
        }
    }

    int columnCount(QModelIndex const & parent) const override {
        if (parent.isValid()) {
            // Rows do not have children.
            return 0;
        } else {
            // The root has items.
            return ColumnCount;
        }
    }

    QVariant data(QModelIndex const & index, int role) const override {
        auto & metadata = this->metadata();

        if (!index.isValid() || index.parent().isValid()) {
            return {};
        }

        auto column = (size_t) index.column();
        if (column >= ColumnCount) {
            return {};
        }

        auto row = (size_t) index.row();
        if (row >= metadata.size()) {
            return {};
        }

        switch (column) {
        case NameColumn:
            switch (role) {
            case Qt::DisplayRole:
                if (_number_channels && row >= CHANNEL_0_ROW) {
                    return QStringLiteral("%1 - %2")
                        .arg(row)
                        .arg(QString::fromStdString(metadata[row].name));
                } else {
                    return QString::fromStdString(metadata[row].name);
                }

            case Qt::CheckStateRole:
                return metadata[row].enabled ? Qt::Checked : Qt::Unchecked;

            default: return {};
            }

        case ProgressColumn:
            switch (role) {
            case Qt::DisplayRole:
                // TODO add custom ProgressPercentRole, ProgressTimeRole, ProgressDurationRole etc.
                return (double) 0.;

            default: return {};
            }

        default: return {};
        }
    }

    bool setData(QModelIndex const& index, QVariant const& value, int role) override {
        if(!index.isValid()) {
            return false;
        }

        if (role == Qt::CheckStateRole) {
            auto & metadata = metadata_mut();

            auto row = (size_t) index.row();
            if (row >= metadata.size()) {
                return false;
            }
            metadata[row].enabled = value == Qt::Checked;
            emit dataChanged(index, index, {Qt::CheckStateRole});
            return true;
        }

        return false;
    }

    Qt::ItemFlags flags(QModelIndex const& index) const override {
        Qt::ItemFlags flags = QAbstractTableModel::flags(index);
        if (index.isValid()) {
            flags |= Qt::ItemIsUserCheckable;
            if (index.row() != MASTER_AUDIO_ROW) {
                flags |= Qt::ItemIsDragEnabled;
            }
        }

        // Only allow dropping *between* items. (This also allows dropping in the
        // background, which acts like dragging past the final row.)
        if (!index.isValid()) {
            flags |= Qt::ItemIsDropEnabled;
        }

        return flags;
    }

    Qt::DropActions supportedDragActions() const override {
        return Qt::MoveAction;
    }

    Qt::DropActions supportedDropActions() const override {
        return Qt::MoveAction;
    }

    /// For leftwards drags, moves [source, source + count) to [dest, dest + count).
    /// For rightwards drags, moves [source, source + count) to [dest - count, dest).
    bool moveRows(
        QModelIndex const& sourceParent,
        int i_source,
        int i_count,
        const QModelIndex &destinationParent,
        int i_dest)
    override {
        // This is not a tree view; reject rows with parents..
        if (sourceParent.isValid() || destinationParent.isValid()) {
            return false;
        }
        // Reject negative inputs.
        if (i_source < 0 || i_count < 0 || i_dest < 0) {
            return false;
        }

        // Reject empty drags.
        if (i_count == 0) {
            return false;
        }

        // Reject moving row 0 (master audio) out of place.
        if (i_source < CHANNEL_0_ROW || i_dest < CHANNEL_0_ROW) {
            return false;
        }

        auto & metadata = metadata_mut();
        auto const n = metadata.size();
        auto const source = (size_t) i_source;
        auto const count = (size_t) i_count;
        auto const dest = (size_t) i_dest;

        // Reject out-of-bounds drags.
        if (source > n || count > n || source + count > n || dest > n) {
            return false;
        }

        // Reject dragging a region into itself ([source, source + count]).
        if (!(dest < source || dest > source + count)) {
            return false;
        }

        auto data = metadata.data();
        if (dest < source) {
            // Leftwards drags.
            std::rotate(
                data + dest,
                data + source,
                data + source + count);
        } else {
            // Rightward drags.
            std::rotate(
                data + source,
                data + source + count,
                data + dest);
        }
        // QAIM emits QAbstractItemModel::rowsMoved for us.
        return true;
    }
};

class ChannelsView final : public QTableView {
    // TODO
public:
    // ChannelsView()
    explicit ChannelsView(QWidget *parent = nullptr)
        : QTableView(parent)
    {
        setSelectionBehavior(QAbstractItemView::SelectRows);
    }
};

class MainWindowImpl : public MainWindow {
    Backend _backend;

    QErrorMessage _error_dialog{this};

    ChannelsModel _model;
    ChannelsView * _view;

    QAction * _open;
    QAction * _render;
    QAction * _exit;

    QString _file_title;
    QString _file_path;

public:
    MainWindowImpl(QWidget *parent, QString path)
        : MainWindow(parent)
        , _model(&_backend)
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
            {l__w(ChannelsView);
                _view = w;
                w->setModel(&_model);
            }
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

        _model.begin_reset_model();
        auto err = _backend.load_path(_file_path);
        _model.end_reset_model();

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
