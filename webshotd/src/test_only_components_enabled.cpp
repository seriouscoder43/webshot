#include "test_only_components.hpp"

/**
 * @file
 * @brief Testsuite implementation for test-only component registration.
 */

#include "browser_probe_handler.hpp"

#include <userver/testsuite/testsuite_support.hpp>

namespace v1 {

void appendTestOnlyComponents(us::components::ComponentList &componentList)
{
    componentList.Append<us::components::TestsuiteSupport>().Append<BrowserProbeHandler>();
}

} // namespace v1
