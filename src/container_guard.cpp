#include "container_guard.hpp"
/**
 * @file
 * @brief RAII wrapper that creates and then removes a Docker container.
 */

#include <exception>

#include <fmt/core.h>

#include <userver/engine/subprocess/process_starter.hpp>
#include <userver/logging/log.hpp>

namespace engine = userver::engine;

using engine::subprocess::ExecOptions;
/** Make ExecOptions that resolve the executable from PATH. */
static inline ExecOptions makeExecOpts()
{
    ExecOptions o;
    o.use_path = true;
    return o;
}

ContainerGuard::ContainerGuard(
    engine::subprocess::ProcessStarter &starterRef, std::string containerName,
    const std::vector<std::string> &createArgs
)
    : starter(&starterRef), name(std::move(containerName)), removed(false)
{
    auto proc = starter->Exec("docker", createArgs, makeExecOpts());
    auto status = proc.Get();
    if (!status.IsExited() || status.GetExitCode() != 0) {
        removed = true;
        throw std::runtime_error(fmt::format("docker create failed for {}", name));
    }
}

ContainerGuard::~ContainerGuard() { remove(); }

void ContainerGuard::remove() noexcept
{
    if (removed || name.empty() || starter == nullptr)
        return;
    try {
        auto proc = starter->Exec(
            "docker", std::vector<std::string>{"rm", "-f", name}, makeExecOpts()
        );
        static_cast<void>(proc.Get());
    } catch (std::exception &) {
        LOG_INFO() << fmt::format("docker rm for {} failed:", name);
    }
    removed = true;
}
