/**
 * @file
 * @brief Implementation of time and token helpers for pagination cursors.
 */

#include "cursor.hpp"

namespace chrono = ws::chrono;

namespace ws::crud {

[[nodiscard]] int64_t TimePointToMicros(Clock::time_point tp)
{
    return Raw(chrono::DurationCast<chrono::microseconds>(tp.time_since_epoch()).count());
}

[[nodiscard]] Clock::time_point MicrosToTimePoint(int64_t micros)
{
    return Clock::time_point{chrono::microseconds{micros}};
}

} // namespace ws::crud
