#pragma once

#include <QDebug>

#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#define TRACE qDebug() << __PRETTY_FUNCTION__
