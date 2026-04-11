#pragma once

#include "expected.hpp"

#include <optional>
#include <utility>

namespace v1 {

template <typename T> inline T grabValueOf(std::optional<T> &opt)
{
    T x = std::move(*opt);
    opt.reset();
    return x;
}

template <typename T> inline T grabValueOf(std::optional<T> &&opt) { return grabValueOf(opt); }

template <typename T, typename E> inline T grabValueOf(Expected<T, E> &expected)
{
    return std::move(expected).value();
}

template <typename T, typename E> inline T grabValueOf(Expected<T, E> &&expected)
{
    return std::move(expected).value();
}

} // namespace v1
