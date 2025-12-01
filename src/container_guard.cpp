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
    engine::subprocess::ProcessStarter &starterRef, String containerName,
    const std::vector<String> &createArgs
)
    : starter(&starterRef), name(std::move(containerName)), removed(false)
{
    std::vector<std::string> byteArgs;
    for (auto &&arg : createArgs)
        byteArgs.push_back(std::string(arg.view()));
    auto proc = starter->Exec("docker", byteArgs, makeExecOpts());
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
            "docker", std::vector<std::string>{"rm", "-f", std::string(name.view())}, makeExecOpts()
        );
        auto status = proc.Get();
        if (!status.IsExited()) {
            LOG_ERROR() << fmt::format("docker rm for {} did not exit cleanly", name);
        } else if (status.GetExitCode() != 0) {
            LOG_ERROR() << fmt::format(
                "docker rm for {} exited with code {}", name, status.GetExitCode()
            );
        }
    } catch (std::exception &) {
        LOG_ERROR() << fmt::format("docker rm for {} failed:", name);
    }
    removed = true;
}
