#pragma once

#include <memory>

namespace v1::crawler {

class [[nodiscard]] BrowserPageSessionLifecycle final {
public:
    BrowserPageSessionLifecycle();
    ~BrowserPageSessionLifecycle();

    BrowserPageSessionLifecycle(const BrowserPageSessionLifecycle &) = delete;
    BrowserPageSessionLifecycle(BrowserPageSessionLifecycle &&) = delete;
    BrowserPageSessionLifecycle &operator=(const BrowserPageSessionLifecycle &) = delete;
    BrowserPageSessionLifecycle &operator=(BrowserPageSessionLifecycle &&) = delete;

    [[nodiscard]] bool markBrowserContextCreated();
    [[nodiscard]] bool markTargetCreated();
    [[nodiscard]] bool markAttached();
    [[nodiscard]] bool markBaseDomainsEnabled();
    [[nodiscard]] bool markDetached();
    [[nodiscard]] bool markDisposed();
    [[nodiscard]] bool markClosed();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace v1::crawler
