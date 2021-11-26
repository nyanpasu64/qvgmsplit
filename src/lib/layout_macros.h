#pragma once

#define VA_COMMA(...)  __VA_OPT__(,) __VA_ARGS__

#define require_semicolon //do {} while (0)

// https://doc.qt.io/qt-5/layout.html#laying-out-widg(ets-in-code
// Apparently setting the parent of a layout
// recursively reparents all widgets the layout is managing (not owning),
// and causes the layout to set the parent of future widgets.


#define HIDE(var)  void * var = nullptr; Q_UNUSED(var)


/// Add menubar to QMainWidget.
#define main__m(...) \
    auto * m = new QMenuBar(__VA_ARGS__); \
    main->setMenuBar(m); \
    HIDE(main) \
    \
    require_semicolon


/// Add menu to QMenuBar or QMenu.
#define m__m(...) \
    auto * m2_ = m->addMenu(__VA_ARGS__); \
    auto * m = m2_;
    \
    require_semicolon


#define m__action(...) \
    auto * a = m->addAction(__VA_ARGS__); \
    HIDE(m); \
    \
    require_semicolon


#define m__check(...) \
    auto * a = m->addAction(__VA_ARGS__); \
    a->setCheckable(true); \
    HIDE(m); \
    \
    require_semicolon


/// Add toolbar to QMainWidget.
#define main__tb(qtoolbar) \
    auto * tb = new qtoolbar; \
    main->addToolBar(tb); \
    HIDE(main) \
    \
    require_semicolon


/// Add central leaf widget to QMainWidget.
#define main__central_w(qwidget_main) \
    auto * w = new qwidget_main; \
    parent->setCentralWidget(w); \
    HIDE(main) \
    \
    require_semicolon


/// Add central container and QBoxLayout to QMainWidget.
#define main__central_c_l(qwidget, qlayout) \
    auto * c = new qwidget; \
    main->setCentralWidget(c); \
    HIDE(main) \
    \
    auto * l = new qlayout; \
    c->setLayout(l); \
    \
    require_semicolon


/// Add container and QBoxLayout to QBoxLayout.
#define l__c_l(qwidget, qlayout, ...) \
    auto * c = new qwidget; \
    l->addWidget(c VA_COMMA(__VA_ARGS__)); \
    \
    auto * l = new qlayout; \
    c->setLayout(l); \
    \
    require_semicolon
// l->addWidget(c) sets c.parent.


/// Add container and QFormLayout to QBoxLayout.
#define l__c_form(qwidget, qlayout, ...) \
    auto * c = new qwidget; \
    l->addWidget(c VA_COMMA(__VA_ARGS__)); \
    HIDE(l) \
    \
    auto * form = new qlayout; \
    c->setLayout(form); \
    \
    require_semicolon


/// Add leaf widget to QBoxLayout.
#define l__wptr(WIDGET_PTR, ...) \
    auto * w = WIDGET_PTR; \
    l->addWidget(w VA_COMMA(__VA_ARGS__)); \
    HIDE(l) \
    \
    require_semicolon
// l->addWidget(w) sets w.parent.


/// Add leaf widget to QBoxLayout.
#define l__w(QWIDGET, ...) \
    l__wptr(new QWIDGET __VA_OPT__(,) __VA_ARGS__)


/// Add QBoxLayout to QBoxLayout.
#define l__l(qlayout, ...) \
    auto * parentL = l; \
    auto * l = new qlayout; \
    parentL->addLayout(l VA_COMMA(__VA_ARGS__)); \
    \
    require_semicolon


/// Add QFormLayout to QBoxLayout.
#define l__form(qformlayout, ...) \
    auto * parentL = l; \
    HIDE(l) \
    auto * form = new qformlayout; \
    parentL->addLayout(form VA_COMMA(__VA_ARGS__)); \
    \
    require_semicolon


/// Add left/right to QFormLayout.
#define form__left_right(_left, _right) \
    auto * left = new _left; \
    auto * right = new _right; \
    form->addRow(left, right); \
    HIDE(form) \
    \
    require_semicolon


/// Add wide leaf widget to QFormLayout.
#define form__w(qwidget) \
    auto * w = new qwidget; \
    form->addRow(w); \
    HIDE(form) \
    \
    require_semicolon


/// Add wide layout to QFormLayout.
#define form__l(qlayout) \
    auto * l = new qlayout; \
    form->addRow(l); \
    HIDE(form) \
    \
    require_semicolon


/// Add label and leaf widget to QFormLayout.
#define form__label_wptr(LEFT_TEXT, RIGHT_PTR) \
    auto * w = RIGHT_PTR; \
    \
    form->addRow(LEFT_TEXT, w); \
    HIDE(form) \
    \
    require_semicolon


#define form__label_w(LEFT_TEXT, RIGHT) \
    form__label_wptr(LEFT_TEXT, new RIGHT)


#define append_stretch(...) \
    l->addStretch(__VA_ARGS__)
