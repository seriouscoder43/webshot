#pragma once

#include <boost/safe_numerics/exception_policies.hpp>
#include <boost/safe_numerics/native.hpp>
#include <boost/safe_numerics/safe_integer.hpp>

#include <cstdint>
#include <cstdlib>

namespace integers_detail {

struct Abort {
    [[noreturn]] void
    operator()(const boost::safe_numerics::safe_numerics_error &, const char *) const noexcept
    {
        std::abort();
    }
};

using AbortPolicy = boost::safe_numerics::exception_policy<Abort, Abort, Abort, Abort>;

} // namespace integers_detail

using u32 = boost::safe_numerics::safe<
    uint32_t, boost::safe_numerics::native, integers_detail::AbortPolicy>;
using i32 =
    boost::safe_numerics::safe<int32_t, boost::safe_numerics::native, integers_detail::AbortPolicy>;
using u64 = boost::safe_numerics::safe<
    uint64_t, boost::safe_numerics::native, integers_detail::AbortPolicy>;
using i64 =
    boost::safe_numerics::safe<int64_t, boost::safe_numerics::native, integers_detail::AbortPolicy>;
