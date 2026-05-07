#include "crawler/browser_page_lifecycle.hpp"

#include <userver/utest/utest.hpp>

namespace ws::crawler {
namespace {

UTEST(BrowserPageSessionLifecycle, HappyPathTransitions)
{
    BrowserPageSessionLifecycle lifecycle{};

    EXPECT_TRUE(lifecycle.MarkBrowserContextCreated());
    EXPECT_TRUE(lifecycle.MarkTargetCreated());
    EXPECT_TRUE(lifecycle.MarkAttached());
    EXPECT_TRUE(lifecycle.MarkBaseDomainsEnabled());
    EXPECT_TRUE(lifecycle.MarkDetached());
    EXPECT_TRUE(lifecycle.MarkDisposed());
    EXPECT_TRUE(lifecycle.MarkStopped());
}

UTEST(BrowserPageSessionLifecycle, RejectsInvalidOrder)
{
    BrowserPageSessionLifecycle lifecycle{};

    EXPECT_FALSE(lifecycle.MarkTargetCreated());
    EXPECT_TRUE(lifecycle.MarkBrowserContextCreated());
    EXPECT_FALSE(lifecycle.MarkAttached());
    EXPECT_TRUE(lifecycle.MarkTargetCreated());
    EXPECT_FALSE(lifecycle.MarkBaseDomainsEnabled());
    EXPECT_TRUE(lifecycle.MarkAttached());
    EXPECT_FALSE(lifecycle.MarkStopped());
    EXPECT_TRUE(lifecycle.MarkBaseDomainsEnabled());
}

UTEST(BrowserPageSessionLifecycle, CleanupTransitionsAreIdempotent)
{
    BrowserPageSessionLifecycle lifecycle{};

    EXPECT_TRUE(lifecycle.MarkStopped());
    EXPECT_TRUE(lifecycle.MarkStopped());
    EXPECT_TRUE(lifecycle.MarkDetached());
    EXPECT_TRUE(lifecycle.MarkDisposed());
    EXPECT_TRUE(lifecycle.MarkStopped());

    BrowserPageSessionLifecycle attached_lifecycle{};
    EXPECT_TRUE(attached_lifecycle.MarkBrowserContextCreated());
    EXPECT_TRUE(attached_lifecycle.MarkTargetCreated());
    EXPECT_TRUE(attached_lifecycle.MarkAttached());
    EXPECT_TRUE(attached_lifecycle.MarkDetached());
    EXPECT_TRUE(attached_lifecycle.MarkDetached());
    EXPECT_TRUE(attached_lifecycle.MarkDisposed());
    EXPECT_TRUE(attached_lifecycle.MarkDisposed());
    EXPECT_TRUE(attached_lifecycle.MarkDetached());
    EXPECT_TRUE(attached_lifecycle.MarkStopped());
    EXPECT_TRUE(attached_lifecycle.MarkStopped());
}

} // namespace
} // namespace ws::crawler
