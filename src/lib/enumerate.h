#pragma once

/// Taken from http://www.reedbeta.com/blog/python-like-enumerate-in-cpp17/

#include <tuple>  // std::tie
#include <iterator>  // std::begin
#include <utility>  // std::declval
#include <cstdint>  // size_t

template <
    typename IndexT,
    typename T,
    typename TIter = decltype(std::begin(std::declval<T>())),
    typename = decltype(std::end(std::declval<T>()))
>
constexpr auto enumerate(T && iterable)
{
    struct iterator
    {
        IndexT i;
        TIter iter;
        bool operator != (const iterator & other) const { return iter != other.iter; }
        void operator ++ () { ++i; ++iter; }
        auto operator * () const { return std::tie(i, *iter); }
    };
    struct iterable_wrapper
    {
        T iterable;
        auto begin() { return iterator{ 0, std::begin(iterable) }; }
        auto end() { return iterator{ 0, std::end(iterable) }; }
    };
    return iterable_wrapper{ std::forward<T>(iterable) };
}
