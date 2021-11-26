#pragma once

#include <QFrame>

class HLine : public QFrame {
public:
    explicit HLine(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags())
        : QFrame(parent, f)
    {
        setFrameShape(QFrame::HLine);
        setFrameShadow(QFrame::Sunken);
    }
};

class VLine : public QFrame {
public:
    explicit VLine(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags())
        : QFrame(parent, f)
    {
        setFrameShape(QFrame::VLine);
        setFrameShadow(QFrame::Sunken);
    }
};
