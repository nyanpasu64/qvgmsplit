#include "mainwindow.h"
#include "backend.h"
#include "lib/layout_macros.h"
#include "lib/release_assert.h"

// Layout
#include <QBoxLayout>
#include <QMenuBar>
#include <QToolBar>

// Model-view
#include <QAbstractTableModel>
#include <QMimeData>
#include <QListView>

// Qt
#include <QDebug>
#include <QErrorMessage>
#include <QFileDialog>
#include <QFileInfo>

#include <algorithm>  // std::rotate
#include <set>
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
//            if (index.row() != MASTER_AUDIO_ROW) {
                flags |= Qt::ItemIsDragEnabled;
//            }
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

    /// Called by QAbstractItemView::dropEvent().
    bool dropMimeData(
        QMimeData const* data,
        Qt::DropAction action,
        int insert_row,
        int insert_column,
        QModelIndex const& replace_index)
    override {
        // Based off QAbstractListModel::dropMimeData().
        if (!data || action != Qt::MoveAction)
            return false;

        QStringList types = mimeTypes();
        if (types.isEmpty())
            return false;
        QString format = types.at(0);
        if (!data->hasFormat(format))
            return false;

        QByteArray encoded = data->data(format);
        QDataStream stream(&encoded, QIODevice::ReadOnly);

        // Only allow dropping between rows.
        if (!replace_index.isValid() && insert_row != -1) {
            std::set<int> dragged_rows;
            QMap<int, QVariant> _v;

            while (!stream.atEnd()) {
                // stream contains one element per cell, not per row.
                int drag_row, _c;
                stream >> drag_row >> _c >> _v;
                dragged_rows.insert(drag_row);
            }

            auto & old_metadata = metadata_mut();
            auto n = old_metadata.size();

            QModelIndexList from, to;
            from.reserve((int) n);
            to.reserve((int) n);

            std::vector<FlatChannelMetadata> new_metadata;
            new_metadata.reserve(n);

            auto skip_row =
                [&dragged_rows, row = dragged_rows.begin()](int i) mutable -> bool
            {
                if (row != dragged_rows.end() && i == *row) {
                    row++;
                    return true;
                }
                return false;
            };
            auto push_idx = [this, &from, &to, &old_metadata, &new_metadata](
                int old_i
            ) {
                auto new_i = (int) new_metadata.size();
                for (int col = 0; col < ColumnCount; col++) {
                    from.push_back(index(old_i, col));
                    to.push_back(index(new_i, col));
                }
                new_metadata.push_back(std::move(old_metadata[(size_t) old_i]));
            };

            for (int old_i = 0; (size_t) old_i < n; old_i++) {
                if (old_i == insert_row) {
                    for (int dragged_row : dragged_rows) {
                        push_idx(dragged_row);
                    }
                }
                if (!skip_row(old_i)) {
                    push_idx(old_i);
                }
            }
            if ((size_t) insert_row == n) {
                for (int dragged_row : dragged_rows) {
                    push_idx(dragged_row);
                }
            }

            release_assert_equal(new_metadata.size(), n);
            changePersistentIndexList(from, to);
            old_metadata = std::move(new_metadata);

            return true;
        }

        return false;
    }

    /// removeRows() is called by QAbstractItemView::startDrag() when the user drags
    /// two items to swap them. But we want to swap items, not overwrite one with
    /// another. So ignore the call.
    bool removeRows(int row, int count, const QModelIndex & parent) override {
        return false;
    }
};

class ChannelsView final : public QListView {
    // TODO
public:
    // ChannelsView()
    explicit ChannelsView(QWidget *parent = nullptr)
        : QListView(parent)
    {
        setSelectionMode(QAbstractItemView::ExtendedSelection);

        setDragEnabled(true);
        setAcceptDrops(true);

        setDragDropMode(QAbstractItemView::InternalMove);
        setDragDropOverwriteMode(true);
        setDropIndicatorShown(true);
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
        if (!_file_path.isEmpty()) {
            load_path();
        }
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
