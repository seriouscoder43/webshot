#include "crawler/browser_page_lifecycle.hpp"

#include <userver/utest/utest.hpp>

namespace v1::crawler {
namespace {

UTEST(BrowserPageSessionLifecycle, HappyPathTransitions)
{
    auto lifecycle = BrowserPageSessionLifecycle{};

    EXPECT_TRUE(lifecycle.markBrowserContextCreated());
    EXPECT_TRUE(lifecycle.markTargetCreated());
    EXPECT_TRUE(lifecycle.markAttached());
    EXPECT_TRUE(lifecycle.markBaseDomainsEnabled());
    EXPECT_TRUE(lifecycle.markDetached());
    EXPECT_TRUE(lifecycle.markDisposed());
    EXPECT_TRUE(lifecycle.markClosed());
}

UTEST(BrowserPageSessionLifecycle, RejectsInvalidOrder)
{
    auto lifecycle = BrowserPageSessionLifecycle{};

    EXPECT_FALSE(lifecycle.markTargetCreated());
    EXPECT_TRUE(lifecycle.markBrowserContextCreated());
    EXPECT_FALSE(lifecycle.markAttached());
    EXPECT_TRUE(lifecycle.markTargetCreated());
    EXPECT_FALSE(lifecycle.markBaseDomainsEnabled());
    EXPECT_TRUE(lifecycle.markAttached());
    EXPECT_FALSE(lifecycle.markClosed());
    EXPECT_TRUE(lifecycle.markBaseDomainsEnabled());
}

UTEST(BrowserPageSessionLifecycle, CleanupTransitionsAreIdempotent)
{
    auto lifecycle = BrowserPageSessionLifecycle{};

    EXPECT_TRUE(lifecycle.markClosed());
    EXPECT_TRUE(lifecycle.markClosed());
    EXPECT_TRUE(lifecycle.markDetached());
    EXPECT_TRUE(lifecycle.markDisposed());
    EXPECT_TRUE(lifecycle.markClosed());

    auto attachedLifecycle = BrowserPageSessionLifecycle{};
    EXPECT_TRUE(attachedLifecycle.markBrowserContextCreated());
    EXPECT_TRUE(attachedLifecycle.markTargetCreated());
    EXPECT_TRUE(attachedLifecycle.markAttached());
    EXPECT_TRUE(attachedLifecycle.markDetached());
    EXPECT_TRUE(attachedLifecycle.markDetached());
    EXPECT_TRUE(attachedLifecycle.markDisposed());
    EXPECT_TRUE(attachedLifecycle.markDisposed());
    EXPECT_TRUE(attachedLifecycle.markDetached());
    EXPECT_TRUE(attachedLifecycle.markClosed());
    EXPECT_TRUE(attachedLifecycle.markClosed());
}

} // namespace
} // namespace v1::crawler
