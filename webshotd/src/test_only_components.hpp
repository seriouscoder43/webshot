#pragma once

#include <userver/components/component_list.hpp>

namespace v1 {

namespace us = userver;
void AppendTestOnlyComponents(us::components::ComponentList &component_list);

} // namespace v1
