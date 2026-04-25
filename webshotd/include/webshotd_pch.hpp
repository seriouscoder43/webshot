#pragma once

// Precompiled header for the webshotd build.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <absl/strings/ascii.h>
#include <boost/uuid/uuid.hpp>
#include <uni_algo/conv.h>
#include <uni_algo/norm.h>

#include <userver/clients/http/client.hpp>
#include <userver/components/component.hpp>
#include <userver/components/component_base.hpp>
#include <userver/engine/deadline.hpp>
#include <userver/engine/exception.hpp>
#include <userver/engine/io/common.hpp>
#include <userver/engine/task/current_task.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/http/content_type.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>
#include <userver/utils/assert.hpp>
#include <userver/utils/datetime.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/yaml_config/schema.hpp>

#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component_list.hpp>
#include <userver/components/fs_cache.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/components/process_starter.hpp>
#include <userver/congestion_control/component.hpp>
#include <userver/engine/task_processors_load_monitor.hpp>
#include <userver/server/handlers/http_handler_static.hpp>
#include <userver/server/handlers/server_monitor.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/utils/daemon_run.hpp>
