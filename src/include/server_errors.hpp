#pragma once
#include <stdexcept>

namespace v1::errors {

struct InvalidPageTokenException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

} // namespace v1::errors
