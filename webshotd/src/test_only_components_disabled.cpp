#include "test_only_components.hpp"

/**
 * @file
 * @brief Release implementation for test-only component registration.
 */

namespace v1 {

namespace us = userver;
void AppendTestOnlyComponents(us::components::ComponentList &) {}

} // namespace v1
