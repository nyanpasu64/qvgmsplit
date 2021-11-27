#include "mainwindow.h"
#include "lib/defer.h"
#include "lib/layout_macros.h"
#include "lib/release_assert.h"
#include "lib/unwrap.h"

// Widgets
#include <QBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMenuBar>
#include <QToolBar>

// Model-view
#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QMimeData>
#include <QListView>

// Qt
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
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

class ChipsModel final : public QAbstractListModel {
private:
    MainWindow * _win;

public:
    explicit ChipsModel(MainWindow * win, QObject *parent = nullptr)
        : _win(win)
    {}

    void begin_reset_model() {
        beginResetModel();
    }

    void end_reset_model() {
        endResetModel();
    }

private:
    std::vector<ChipMetadata> const& get_chips() const {
        return _win->_backend.chips();
    }

    std::vector<ChipMetadata> & chips_mut() const {
        return _win->_backend.chips_mut();
    }

// impl QAbstractItemModel
public:
    int rowCount(QModelIndex const & parent) const override {
        if (parent.isValid()) {
            // Rows do not have children.
            return 0;
        } else {
            // The root has items.
            return int(get_chips().size());
        }
    }

    QVariant data(QModelIndex const & index, int role) const override {
        auto & chips = this->get_chips();

        if (!index.isValid() || index.parent().isValid()) {
            return {};
        }

        if (index.column() != 0) {
            return {};
        }

        auto row = (size_t) index.row();
        if (row >= chips.size()) {
            return {};
        }

        switch (role) {
        case Qt::DisplayRole:
            return QString::fromStdString(chips[row].name);

        default: return {};
        }
    }

    Qt::ItemFlags flags(QModelIndex const& index) const override {
        Qt::ItemFlags flags = QAbstractListModel::flags(index);
        if (index.isValid()) {
            flags |= Qt::ItemIsDragEnabled;
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

            auto & old_chips = chips_mut();
            auto n = old_chips.size();

            QModelIndexList from, to;
            from.reserve((int) n);
            to.reserve((int) n);

            std::vector<ChipMetadata> new_chips;
            new_chips.reserve(n);

            auto skip_row =
                [&dragged_rows, row = dragged_rows.begin()](int i) mutable -> bool
            {
                if (row != dragged_rows.end() && i == *row) {
                    row++;
                    return true;
                }
                return false;
            };
            auto push_idx = [this, &from, &to, &old_chips, &new_chips](
                int old_i
            ) {
                auto new_i = (int) new_chips.size();
                from.push_back(index(old_i, 0));
                to.push_back(index(new_i, 0));
                new_chips.push_back(std::move(old_chips[(size_t) old_i]));
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

            release_assert_equal(new_chips.size(), n);
            changePersistentIndexList(from, to);
            old_chips = std::move(new_chips);

            auto tx = _win->edit_unwrap();
            tx.chips_changed();
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

class ChipsView final : public QListView {
    // TODO
public:
    // ChannelsView()
    explicit ChipsView(QWidget *parent = nullptr)
        : QListView(parent)
    {
        setSelectionMode(QAbstractItemView::ExtendedSelection);

        setDragEnabled(true);
        setAcceptDrops(true);

        setDragDropMode(QAbstractItemView::InternalMove);
        setDragDropOverwriteMode(true);
        setDropIndicatorShown(true);
    }

// impl QWidget
    QSize sizeHint() const {
        return QSize(128, 128);
    }
};

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
                index((int) get_channels().size() - 1, NameColumn));
        }
    }

    void begin_reset_model() {
        beginResetModel();
    }

    void end_reset_model() {
        endResetModel();
    }

private:
    std::vector<FlatChannelMetadata> const& get_channels() const {
        return _backend->channels();
    }

    std::vector<FlatChannelMetadata> & channels_mut() {
        return _backend->channels_mut();
    }

// impl QAbstractItemModel
public:
    int rowCount(QModelIndex const & parent) const override {
        if (parent.isValid()) {
            // Rows do not have children.
            return 0;
        } else {
            // The root has items.
            return int(get_channels().size());
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
        auto & channels = this->get_channels();

        if (!index.isValid() || index.parent().isValid()) {
            return {};
        }

        auto column = (size_t) index.column();
        if (column >= ColumnCount) {
            return {};
        }

        auto row = (size_t) index.row();
        if (row >= channels.size()) {
            return {};
        }

        switch (column) {
        case NameColumn:
            switch (role) {
            case Qt::DisplayRole:
                if (_number_channels && row >= CHANNEL_0_ROW) {
                    return QStringLiteral("%1 - %2")
                        .arg(row)
                        .arg(QString::fromStdString(channels[row].name));
                } else {
                    return QString::fromStdString(channels[row].name);
                }

            case Qt::CheckStateRole:
                return channels[row].enabled ? Qt::Checked : Qt::Unchecked;

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
            auto & channels = channels_mut();

            auto row = (size_t) index.row();
            if (row >= channels.size()) {
                return false;
            }
            channels[row].enabled = value == Qt::Checked;
            emit dataChanged(index, index, {Qt::CheckStateRole});
            return true;
        }

        return false;
    }

    Qt::ItemFlags flags(QModelIndex const& index) const override {
        Qt::ItemFlags flags = QAbstractTableModel::flags(index);
        if (index.isValid()) {
            flags |= Qt::ItemIsUserCheckable;
        }
        return flags;
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
    }
};

class MainWindowImpl final : public MainWindow {
public:
    QErrorMessage _error_dialog{this};

    ChipsModel _chips_model;
    ChannelsModel _channels_model;

    ChipsView * _chips_view;
    ChannelsView * _channels_view;

    QAction * _open;
    QAction * _render;
    QAction * _exit;

    QString _file_title;
    QString _file_path;

public:
    std::optional<StateTransaction> edit_state() final {
        return StateTransaction::make(this);
    }

    StateTransaction edit_unwrap() final {
        return unwrap(edit_state());
    }

    MainWindowImpl(QWidget *parent, QString path)
        : MainWindow(parent)
        , _chips_model(this)
        , _channels_model(&_backend)
    {
        setup_error_dialog(_error_dialog);
        setAcceptDrops(true);

        // Setup GUI layout

        resize(450, 600);
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

        {main__central_c_l(QWidget, QGridLayout);
            l->setContentsMargins(-1, 8, -1, -1);

            l->addWidget(new QLabel(tr("Chip Order")), 0, 0);
            {auto w = new ChipsView;
                l->addWidget(w, 1, 0);
                _chips_view = w;

                w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
                w->setModel(&_chips_model);
            }

            l->addWidget(new QLabel(tr("Channel Select")), 0, 1);
            {auto w = new ChannelsView;
                l->addWidget(w, 1, 1);
                _channels_view = w;

                w->setModel(&_channels_model);
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
        if (!path.isEmpty()) {
            load_path(std::move(path));
        }
    }

    void load_path(QString file_path) {
        auto tx = edit_unwrap();
        // Should be called before Backend::load_path() overwrites metadata.
        tx.file_replaced();

        auto err = _backend.load_path(file_path);

        if (!err.isEmpty()) {
            _error_dialog.close();
            _error_dialog.showMessage(err);
        } else {
            _file_path = file_path;
        }
    }

    void reload_title() {
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

        load_path(std::move(path));
    }

    void on_render() {

    }

// impl QWidget
    void dragEnterEvent(QDragEnterEvent *event) override {
        QMimeData const* mime = event->mimeData();
        auto urls = mime->urls();
        qDebug() << urls;
        if (!urls.isEmpty() && urls[0].isLocalFile()) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent *event) override {
        QMimeData const* mime = event->mimeData();
        auto urls = mime->urls();
        qDebug() << urls;
        if (!urls.isEmpty() && urls[0].isLocalFile()) {
            load_path(urls[0].toLocalFile());
        }
    }
};

std::unique_ptr<MainWindow> MainWindow::new_with_path(QString path) {
    return std::make_unique<MainWindowImpl>(nullptr, std::move(path));
}

// # GUI state mutation tracking (StateTransaction):

StateTransaction::StateTransaction(MainWindowImpl *win)
    : _win(win)
    , _uncaught_exceptions(std::uncaught_exceptions())
{
    assert(!win->_backend._during_update);
    state_mut()._during_update = true;
}

std::optional<StateTransaction> StateTransaction::make(MainWindowImpl *win) {
    if (win->_backend._during_update) {
        return {};
    }
    return StateTransaction(win);
}

StateTransaction::StateTransaction(StateTransaction && other) noexcept
    : _win(other._win)
    , _uncaught_exceptions(other._uncaught_exceptions)
    , _queued_updates(other._queued_updates)
{
    other._win = nullptr;
}

StateTransaction::~StateTransaction() noexcept(false) {
    if (_win == nullptr) {
        return;
    }
    auto & state = state_mut();
    defer {
        state._during_update = false;
    };

    // If unwinding, skip updating the GUI; we don't want to do work during unwinding.
    if (std::uncaught_exceptions() != _uncaught_exceptions) {
        return;
    }

    auto e = _queued_updates;
    using E = StateUpdateFlag;

    // Window title
    if (e & E::FileReplaced) {
        _win->reload_title();
    }

    // Chips list
    if (e & E::FileReplaced) {
        _win->_chips_model.end_reset_model();
    }

    // Channels list
    if (e & E::ChipsEdited) {
        if (!(e & E::FileReplaced)) {
            _win->_backend.sort_channels();
        }
        _win->_channels_model.end_reset_model();
    }
}

Backend const& StateTransaction::state() const {
    return _win->_backend;
}

Backend & StateTransaction::state_mut() {
    return _win->_backend;
}

using E = StateUpdateFlag;

void StateTransaction::file_replaced() {
    if (!(_queued_updates & E::ChipsEdited)) {
        _win->_channels_model.begin_reset_model();
    }
    if (!(_queued_updates & E::FileReplaced)) {
        _win->_chips_model.begin_reset_model();
    }
    _queued_updates |= E::All;
}

void StateTransaction::chips_changed() {
    if (!(_queued_updates & E::ChipsEdited)) {
        _win->_channels_model.begin_reset_model();
        _queued_updates |= E::ChipsEdited;
    }

}
