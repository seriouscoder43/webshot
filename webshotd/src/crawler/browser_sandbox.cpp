#include "crawler/browser_sandbox.hpp"
#include "crawler/launch_policy.hpp"

#include <fmt/format.h>

namespace v1::crawler {

std::vector<std::string>
buildChromiumArgs(const std::string &userDataDir, const std::string &netlogPath)
{
    std::vector<std::string> args = {
        "--headless=new",
        "--disable-gpu",
        "--disable-gpu-compositing",
        "--disable-gpu-rasterization",
        "--disable-dev-shm-usage",
        "--disable-background-networking",
        "--disable-breakpad",
        "--disable-crash-reporter",
        "--disable-quic",
        "--no-default-browser-check",
        "--no-first-run",
        "--mute-audio",
        "--hide-scrollbars",
        "--no-sandbox",
        "--no-zygote",
        "--ignore-certificate-errors",
        "--use-gl=swiftshader",
        "--disable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan",
        "--user-data-dir=" + userDataDir,
        "--log-net-log=" + netlogPath,
        "--net-log-capture-mode=IncludeSensitive",
        std::string{"--proxy-server=http://127.0.0.1:"} + fmt::to_string(kProxyListenPort),
        "--proxy-bypass-list=<-loopback>",
        "--remote-debugging-address=127.0.0.1",
        "--remote-debugging-port=" + fmt::to_string(kDevtoolsPort),
        "--window-size=1600,900",
    };
    args.emplace_back("--enable-logging=stderr");
    args.emplace_back("--log-level=0");
    args.emplace_back("about:blank");
    return args;
}

} // namespace v1::crawler
