#pragma once

/**
 * @file
 * @brief Project-wide userver namespace aliases.
 *
 * userver uses versioned namespaces; forward-declaring nested `userver::...`
 * namespaces can introduce ambiguity (e.g. `userver::server` vs
 * `userver::v2_xx::server`). To keep aliasing robust, this header includes a
 * small set of userver headers that ensure the aliased namespaces exist.
 */

// Minimal includes to make the aliased namespaces visible as `userver::...`.
#include <userver/clients/http/client.hpp>
#include <userver/engine/deadline.hpp>
#include <userver/formats/json_fwd.hpp>
#include <userver/server/request/task_inherited_data.hpp>
#include <userver/utils/datetime.hpp>

namespace us = userver;
namespace server = us::server;
namespace eng = us::engine;
namespace json = us::formats::json;
namespace datetime = us::utils::datetime;
namespace httpc = us::clients::http;
