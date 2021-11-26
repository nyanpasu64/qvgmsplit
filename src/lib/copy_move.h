#pragma once

// based off Q_DISABLE_COPY and the like.

#define DISABLE_COPY(Class) \
    Class(Class const&) = delete;\
    Class & operator=(Class const&) = delete;

#define DISABLE_MOVE(Class) \
    Class(Class &&) = delete; \
    Class & operator=(Class &&) = delete;

#define DISABLE_COPY_MOVE(Class) \
    DISABLE_COPY(Class) \
    DISABLE_MOVE(Class)


#define DEFAULT_COPY(Class) \
    Class(Class const&) = default;\
    Class & operator=(Class const&) = default;

#define CONSTEXPR_COPY(Class) \
    constexpr Class(Class const&) = default;\
    constexpr Class & operator=(Class const&) = default;

#define DEFAULT_MOVE(Class) \
    Class(Class &&) = default; \
    Class & operator=(Class &&) = default;
