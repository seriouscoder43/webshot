#pragma once

#include <string>
#include <vector>

#include <userver/engine/subprocess/process_starter.hpp>

namespace us = userver;
namespace engine = us::engine;

class [[nodiscard]] ContainerGuard {
public:
    ContainerGuard(
        engine::subprocess::ProcessStarter &starter, std::string name,
        const std::vector<std::string> &createArgs
    );
    ~ContainerGuard();

    ContainerGuard(const ContainerGuard &) = delete;
    ContainerGuard &operator=(const ContainerGuard &) = delete;
    ContainerGuard(ContainerGuard &&) noexcept;
    ContainerGuard &operator=(ContainerGuard &&) noexcept;

    const std::string &name() const noexcept { return name_; }

    // Explicitly remove container now. Safe to call multiple times.
    void remove() noexcept;

private:
    engine::subprocess::ProcessStarter *starter_ = nullptr;
    std::string name_;
    bool removed_;
};
