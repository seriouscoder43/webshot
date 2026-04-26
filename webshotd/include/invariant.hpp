#pragma once

#include "text.hpp"

#include <userver/utils/assert.hpp>

namespace v1 {

[[noreturn]] inline void invariant(const String &message) noexcept
{
    userver::utils::AbortWithStacktrace(message.view());
}

template <typename Condition>
inline void invariant(const Condition &condition, const String &message) noexcept
{
    if (condition)
        return;
    invariant(message);
}

} // namespace v1
