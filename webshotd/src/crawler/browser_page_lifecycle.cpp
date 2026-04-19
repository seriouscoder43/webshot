#include "crawler/browser_page_lifecycle.hpp"

#include <boost/sml.hpp>

#include <memory>

namespace v1::crawler {
namespace sml = boost::sml;

namespace {

struct IdleState {};
struct BrowserContextCreatedState {};
struct TargetCreatedState {};
struct AttachedState {};
struct BaseDomainsEnabledState {};
struct DetachedState {};
struct DisposedState {};
struct ClosedState {};

struct BrowserContextCreated {};
struct TargetCreated {};
struct Attached {};
struct BaseDomainsEnabled {};
struct Detached {};
struct Disposed {};
struct Closed {};

struct BrowserPageLifecycleDefinition {
    [[nodiscard]] auto operator()() const
    {
        using namespace sml;

        return make_transition_table(
            *state<IdleState> + event<BrowserContextCreated> = state<BrowserContextCreatedState>,
            state<BrowserContextCreatedState> + event<TargetCreated> = state<TargetCreatedState>,
            state<TargetCreatedState> + event<Attached> = state<AttachedState>,
            state<AttachedState> + event<BaseDomainsEnabled> = state<BaseDomainsEnabledState>,
            state<AttachedState> + event<Detached> = state<DetachedState>,
            state<BaseDomainsEnabledState> + event<Detached> = state<DetachedState>,
            state<DetachedState> + event<Detached> = state<DetachedState>,
            state<DisposedState> + event<Detached> = state<DisposedState>,
            state<ClosedState> + event<Detached> = state<ClosedState>,
            state<BrowserContextCreatedState> + event<Disposed> = state<DisposedState>,
            state<TargetCreatedState> + event<Disposed> = state<DisposedState>,
            state<DetachedState> + event<Disposed> = state<DisposedState>,
            state<DisposedState> + event<Disposed> = state<DisposedState>,
            state<ClosedState> + event<Disposed> = state<ClosedState>,
            state<IdleState> + event<Closed> = state<ClosedState>,
            state<DisposedState> + event<Closed> = state<ClosedState>,
            state<ClosedState> + event<Closed> = state<ClosedState>
        );
    }
};

} // namespace

struct BrowserPageSessionLifecycle::Impl final {
    template <typename Event> [[nodiscard]] bool process(const Event &event)
    {
        return sm.process_event(event);
    }

    sml::sm<BrowserPageLifecycleDefinition> sm;
};

BrowserPageSessionLifecycle::BrowserPageSessionLifecycle() : impl(std::make_unique<Impl>()) {}

BrowserPageSessionLifecycle::~BrowserPageSessionLifecycle() = default;

bool BrowserPageSessionLifecycle::markBrowserContextCreated()
{
    return impl->process(BrowserContextCreated{});
}

bool BrowserPageSessionLifecycle::markTargetCreated() { return impl->process(TargetCreated{}); }

bool BrowserPageSessionLifecycle::markAttached() { return impl->process(Attached{}); }

bool BrowserPageSessionLifecycle::markBaseDomainsEnabled()
{
    return impl->process(BaseDomainsEnabled{});
}

bool BrowserPageSessionLifecycle::markDetached() { return impl->process(Detached{}); }

bool BrowserPageSessionLifecycle::markDisposed() { return impl->process(Disposed{}); }

bool BrowserPageSessionLifecycle::markClosed() { return impl->process(Closed{}); }

} // namespace v1::crawler
