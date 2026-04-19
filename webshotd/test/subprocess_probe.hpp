#pragma once

#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace test::subprocess_probe {

struct Result {
    int exitCode = -1;
    int termSignal = 0;
    std::string output;

    [[nodiscard]] bool exited() const noexcept { return exitCode >= 0; }
    [[nodiscard]] bool signaled() const noexcept { return termSignal != 0; }
};

inline std::filesystem::path currentExecutableDir()
{
    std::array<char, 4096> buf{};
    const auto n = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (n < 0) {
        throw std::runtime_error(
            std::string{"readlink(/proc/self/exe) failed: "} + std::strerror(errno)
        );
    }
    buf.at(static_cast<std::size_t>(n)) = '\0';
    return std::filesystem::path{buf.data()}.parent_path();
}

inline Result run(std::string_view helperName, std::initializer_list<std::string_view> args)
{
    const auto helperPath = currentExecutableDir() / helperName;
    if (!std::filesystem::is_regular_file(helperPath)) {
        throw std::runtime_error("missing helper binary: " + helperPath.string());
    }

    int pipeFds[2];
    if (::pipe(pipeFds) != 0) {
        throw std::runtime_error(std::string{"pipe failed: "} + std::strerror(errno));
    }

    const auto pid = ::fork();
    if (pid < 0) {
        const auto savedErrno = errno;
        ::close(pipeFds[0]);
        ::close(pipeFds[1]);
        throw std::runtime_error(std::string{"fork failed: "} + std::strerror(savedErrno));
    }

    if (pid == 0) {
        ::close(pipeFds[0]);
        ::dup2(pipeFds[1], STDERR_FILENO);
        ::dup2(pipeFds[1], STDOUT_FILENO);
        ::close(pipeFds[1]);

        auto ownedArgs = std::vector<std::string>{helperPath.string()};
        ownedArgs.reserve(args.size() + 1);
        for (const auto arg : args)
            ownedArgs.emplace_back(arg);

        auto argv = std::vector<char *>{};
        argv.reserve(ownedArgs.size() + 1);
        for (auto &arg : ownedArgs)
            argv.push_back(arg.data());
        argv.push_back(nullptr);

        ::execv(helperPath.c_str(), argv.data());
        std::fprintf(stderr, "execv failed for %s: %s\n", helperPath.c_str(), std::strerror(errno));
        std::_Exit(127);
    }

    ::close(pipeFds[1]);

    auto result = Result{};
    std::array<char, 4096> buf{};
    while (true) {
        const auto n = ::read(pipeFds[0], buf.data(), buf.size());
        if (n == 0)
            break;
        if (n < 0) {
            if (errno == EINTR)
                continue;
            ::close(pipeFds[0]);
            throw std::runtime_error(std::string{"read failed: "} + std::strerror(errno));
        }
        result.output.append(buf.data(), static_cast<std::size_t>(n));
    }
    ::close(pipeFds[0]);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            throw std::runtime_error(std::string{"waitpid failed: "} + std::strerror(errno));
        }
    }

    if (WIFEXITED(status))
        result.exitCode = WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        result.termSignal = WTERMSIG(status);

    return result;
}

} // namespace test::subprocess_probe
