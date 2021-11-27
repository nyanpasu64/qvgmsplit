#pragma once

// https://stackoverflow.com/a/42060129, not https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/
struct DeferDummy {};

template <class Lambda>
struct Deferrer {
    Lambda f;
    ~Deferrer() { f(); }
};

template <class Lambda>
Deferrer<Lambda> operator<<(DeferDummy, Lambda f) {
    return {f};
}

#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer \
    auto DEFER(__LINE__) = DeferDummy{} << [&]()

// Usage:
// defer {...};
