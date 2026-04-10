#pragma once

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <format>
#include <string>
#include <string_view>

namespace std {

template <> struct formatter<boost::uuids::uuid, char> : formatter<std::string_view, char> {
    auto format(const boost::uuids::uuid &value, format_context &ctx) const
    {
        const auto text = boost::uuids::to_string(value);
        return formatter<std::string_view, char>::format(text, ctx);
    }
};

} // namespace std
