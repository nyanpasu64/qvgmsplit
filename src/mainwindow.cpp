#include "mainwindow.h"
#include "render_dialog.h"
#include "options_dialog.h"
#include "gui_app.h"
#include "lib/defer.h"
#include "lib/layout_macros.h"
#include "lib/release_assert.h"
#include "lib/unwrap.h"

// Widgets
#include <QBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QToolBar>

// Model-view
#include <QAbstractListModel>
#include <QMimeData>
#include <QListView>

// Qt
#include <QDebug>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QErrorMessage>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextDocument>

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

    void move_up(QModelIndex const& idx) {
        if (!idx.isValid()) {
            return;
        }
        int row = idx.row();

        if (!(1 <= row && row < (int) get_chips().size())) {
            return;
        }

        auto tx = _win->edit_unwrap();
        auto & chips = chips_mut(tx);

        beginMoveRows({}, row, row, {}, row - 1);
        std::swap(chips[(size_t) row], chips[(size_t) (row - 1)]);
        endMoveRows();
    }

    void move_down(QModelIndex const& idx) {
        if (!idx.isValid()) {
            return;
        }
        move_up(this->index(idx.row() + 1, 0));
    }

private:
    std::vector<ChipMetadata> const& get_chips() const {
        return _win->_backend.chips();
    }

    std::vector<ChipMetadata> & chips_mut(StateTransaction & tx) const {
        tx.chips_changed();
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
        // Heavily overengineered to support QTableView and ExtendedSelection.
        // This function can be simplified or replaced since we're now using QListView
        // and SingleSelection.

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

        // When dragging past the end of the list, !replace_index.isValid() and
        // insert_row == -1. Set insert_row = n to allow dragging to the end of the
        // list.

        // Only allow dropping between rows or after the last row, not onto a row.
        if (!replace_index.isValid()) {
            auto tx = _win->edit_unwrap();
            auto & old_chips = chips_mut(tx);
            auto n = old_chips.size();

            if (insert_row == -1) {
                insert_row = (int) n;
            }
            release_assert(0 <= insert_row && (size_t) insert_row <= n);

            std::set<int> dragged_rows;
            QMap<int, QVariant> _v;

            while (!stream.atEnd()) {
                // stream contains one element per cell, not per row.
                int drag_row, _c;
                stream >> drag_row >> _c >> _v;
                dragged_rows.insert(drag_row);
            }

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
public:
    // ChannelsView()
    explicit ChipsView(QWidget *parent = nullptr)
        : QListView(parent)
    {
        setSelectionMode(QAbstractItemView::SingleSelection);

        setDragEnabled(true);
        setAcceptDrops(true);

        setDragDropMode(QAbstractItemView::InternalMove);
        setDragDropOverwriteMode(true);
        setDropIndicatorShown(true);
    }

// impl QWidget
public:
    QSize sizeHint() const override {
        return QSize(144, 144);
    }
};

class ChannelsModel final : public QAbstractListModel {
private:
    Backend * _backend;

public:
    explicit ChannelsModel(Backend * backend, QObject *parent = nullptr)
        : _backend(backend)
    {}

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

    QVariant data(QModelIndex const & index, int role) const override {
        auto & channels = this->get_channels();

        if (!index.isValid() || index.parent().isValid()) {
            return {};
        }

        if (index.column() != 0) {
            return {};
        }

        auto row = (size_t) index.row();
        if (row >= channels.size()) {
            return {};
        }

        switch (role) {
        case Qt::DisplayRole:
            return channels[row].numbered_name(row);

        case Qt::CheckStateRole:
            return channels[row].enabled ? Qt::Checked : Qt::Unchecked;

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
        Qt::ItemFlags flags = QAbstractListModel::flags(index);
        if (index.isValid()) {
            flags |= Qt::ItemIsUserCheckable;
        }
        return flags;
    }
};

class ChannelsView final : public QListView {
    bool _space_pressed = false;

public:
    // ChannelsView()
    explicit ChannelsView(QWidget *parent = nullptr)
        : QListView(parent)
    {
        setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

// impl QWidget
protected:
    /// If Space is pressed, toggle the checked state of *every* selected row. By
    /// default, QAbstractItemView only toggles the current row.
    void keyPressEvent(QKeyEvent * event) override {
        switch (event->key()) {
        case Qt::Key_Space:
        // no clue what Key_Select is, but it acts like Space and checks items.
        case Qt::Key_Select:
            _space_pressed = true;
        }

        // If Key_Space or Key_Select pressed, this calls edit() below.
        QListView::keyPressEvent(event);

        _space_pressed = false;
    }

    bool edit(QModelIndex const& index, EditTrigger trigger, QEvent * event) override {
        if (_space_pressed) {
            // If user presses Space when all selected items are checked, uncheck all.
            // Otherwise check all selected items.

            QAbstractItemModel * model = this->model();
            bool all_checked = true;

            auto const sels = selectedIndexes();
            for (auto const& sel : sels) {
                QVariant value = model->data(sel, Qt::CheckStateRole);
                if (!value.isValid()) {
                    return false;
                }

                Qt::CheckState state = static_cast<Qt::CheckState>(value.toInt());
                all_checked &= state == Qt::Checked;
            }

            auto toggled_state = all_checked ? Qt::Unchecked : Qt::Checked;
            for (auto const& sel : sels) {
                model->setData(sel, toggled_state, Qt::CheckStateRole);
            }
            return true;
        } else {
            return QListView::edit(index, trigger, event);
        }
    }
};

class SmallButton : public QPushButton {
public:
    // SmallButton()
    using QPushButton::QPushButton;

// impl QWidget
public:
    QSize sizeHint() const {
        auto out = QPushButton::sizeHint();
        out.setWidth(0);
        return out;
    }
};

static QString errors_to_html(QString const& prefix, std::vector<QString> const& errors) {
    QTextDocument document;
    auto cursor = QTextCursor(&document);
    cursor.beginEditBlock();
    cursor.insertText(prefix);

    // https://stackoverflow.com/a/51864380
    QTextList* bullets = nullptr;
    QTextBlockFormat non_list_format = cursor.blockFormat();
    for (auto const& e : errors) {
        if (!bullets) {
            // create list with 1 item
            bullets = cursor.insertList(QTextListFormat::ListDisc);
        } else {
            // append item to list
            cursor.insertBlock();
        }

        cursor.insertText(e);
    }

    return document.toHtml();
}

class MainWindowImpl final : public MainWindow {
public:
    QErrorMessage _error_dialog{this};

    ChipsModel _chips_model;
    ChannelsModel _channels_model;

    ChipsView * _chips_view;
    QPushButton * _move_up;
    QPushButton * _move_down;
    ChannelsView * _channels_view;

    QAction * _open;
    QAction * _render;
    QAction * _exit;

    QAction * _options;

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
                m->addSeparator();
                {m__action(tr("E&xit"));
                    _exit = a;
                }
            }
            {m__m(tr("&Tools"));
                {m__action(tr("&Options"));
                    _options = a;
                }
            }
        }

        {main__tb(QToolBar);
            tb->setMovable(false);

            tb->addAction(_open);
            tb->addAction(_render);
            tb->addSeparator();
            tb->addAction(_options);
        }

        {main__central_c_l(QWidget, QGridLayout);
            l->setContentsMargins(-1, 6, -1, -1);

            l->addWidget(new QLabel(tr("Chip Order")), 0, 0);
            {
                auto grid = l;
                auto l = new QVBoxLayout;
                grid->addLayout(l, 1, 0);

                {l__w(ChipsView);
                    _chips_view = w;

                    w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
                    w->setModel(&_chips_model);
                }
                {l__l(QHBoxLayout);
                    {l__w(SmallButton(tr("&Up")));
                        _move_up = w;
                    }
                    {l__w(SmallButton(tr("&Down")));
                        _move_down = w;
                    }
                }
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

        connect(_options, &QAction::triggered, this, &MainWindowImpl::open_options);

        connect(_move_up, &QPushButton::clicked, this, &MainWindowImpl::move_up);
        connect(_move_down, &QPushButton::clicked, this, &MainWindowImpl::move_down);

        if (!path.isEmpty()) {
            load_path(std::move(path));
        } else {
            update_file_status();
        }
    }

    void load_path(QString file_path) {
        auto tx = edit_unwrap();

        // StateTransaction::file_replaced() must be called before the chips/channels
        // change. Backend::load_path() overwrites the chips/channels, so we must call
        // file_replaced() first.
        tx.file_replaced();
        // TODO make Backend::load_path() accept StateTransaction &?
        auto err = _backend.load_path(file_path);

        if (!err.isEmpty()) {
            _error_dialog.close();
            _error_dialog.showMessage(
                tr("Error loading file \"%1\":<br>%2").arg(file_path, err)
            );
        } else {
            _file_path = file_path;
        }
    }

    void update_file_status() {
        _file_title = (!_file_path.isEmpty())
            ? QFileInfo(_file_path).fileName()
            : QString();

        if (!_file_title.isEmpty()) {
            setWindowTitle(QStringLiteral("%1[*] - %2").arg(
                _file_title, GuiApp::app_name()
            ));
        } else {
            setWindowTitle(GuiApp::app_name());
        }

        setWindowFilePath(_file_path);
        // setWindowModified(false);

        // Disable render button when no file is open.
        _render->setDisabled(_backend.channels().empty());
    }

    void move_up() {
        _chips_model.move_up(_chips_view->currentIndex());
    }

    void move_down() {
        _chips_model.move_down(_chips_view->currentIndex());
    }

    void on_open() {
        // TODO save recent dirs, using SQLite or QSettings
        QString path = QFileDialog::getOpenFileName(
            this,
            tr("Open File"),
            QString(),
            tr("All supported files (*.vgm *.vgz *.dro *.s98);;All files (*)"));

        if (path.isEmpty()) {
            return;
        }

        load_path(path);
    }

    void on_render() {
        auto orig_path = QFileInfo(_file_path);
        auto wav_name = orig_path.baseName() + QStringLiteral(".wav");

        QString render_path = QFileDialog::getSaveFileName(
            this,
            tr("Render To"),
            orig_path.dir().absoluteFilePath(wav_name),
            tr("WAV files (*.wav);;All files (*)"));

        if (render_path.isEmpty()) {
            return;
        }

        auto err = _backend.start_render(render_path);
        if (!err.empty()) {
            // In practice this never happens; the only errors that are reported
            // immediately are "already rendering" (the Render action is disabled
            // during rendering), OOM errors (unlikely), and .vgm parsing errors (you
            // can't even open a file without parsing a VGM).
            //
            // Entering an invalid file path raises an error *after* each job is
            // created, when it's actually scheduled; these errors show up in
            // Backend::render_jobs()[].results().

            _error_dialog.showMessage(
                errors_to_html(tr("Errors starting render:"), err)
            );

            // _backend.is_rendering() *should* be false, but just in case it's true,
            // start the timer and call update_render_status() anyway (instead of
            // returning).
        }

        if (_backend.is_rendering()) {
            auto render = RenderDialog::make(&_backend, this);
            render->setAttribute(Qt::WA_DeleteOnClose);
            render->show();
        }
    }

    void open_options() {
        auto options = OptionsDialog::make(&_backend, this);
        options->setAttribute(Qt::WA_DeleteOnClose);
        options->show();
    }

// impl QWidget
    void dragEnterEvent(QDragEnterEvent *event) override {
        QMimeData const* mime = event->mimeData();
        auto urls = mime->urls();
        if (!urls.isEmpty() && urls[0].isLocalFile()) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent *event) override {
        QMimeData const* mime = event->mimeData();
        auto urls = mime->urls();
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
        _win->update_file_status();
    }

    // Chips list
    if (e & E::FileReplaced) {
        _win->_chips_model.end_reset_model();

        // Highlight the first chip, which gets moved by the up/down buttons.
        QModelIndex chip_0 = _win->_chips_model.index(0);
        _win->_chips_view->selectionModel()->select(
            chip_0, QItemSelectionModel::ClearAndSelect
        );

        // The Move Down button moves _chips_view->currentIndex(), not the selection.
        // If _chips_view is not focused, selectionModel()->select() doesn't set the
        // current index, so we need to do it manually for the Move Down button to work.
        _win->_chips_view->setCurrentIndex(chip_0);
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
