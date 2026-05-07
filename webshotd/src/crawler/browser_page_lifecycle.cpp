#include "crawler/browser_page_lifecycle.hpp"

#include <boost/sml.hpp>

#include <memory>

namespace ws::crawler {
namespace sml = boost::sml;

namespace {

struct IdleState {};
struct BrowserContextCreatedState {};
struct TargetCreatedState {};
struct AttachedState {};
struct BaseDomainsEnabledState {};
struct DetachedState {};
struct DisposedState {};
struct StoppedState {};

struct BrowserContextCreated {};
struct TargetCreated {};
struct Attached {};
struct BaseDomainsEnabled {};
struct Detached {};
struct Disposed {};
struct Stopped {};

struct BrowserPageSessionLifecycleDefinition {
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
            state<StoppedState> + event<Detached> = state<StoppedState>,
            state<BrowserContextCreatedState> + event<Disposed> = state<DisposedState>,
            state<TargetCreatedState> + event<Disposed> = state<DisposedState>,
            state<DetachedState> + event<Disposed> = state<DisposedState>,
            state<DisposedState> + event<Disposed> = state<DisposedState>,
            state<StoppedState> + event<Disposed> = state<StoppedState>,
            state<IdleState> + event<Stopped> = state<StoppedState>,
            state<DisposedState> + event<Stopped> = state<StoppedState>,
            state<StoppedState> + event<Stopped> = state<StoppedState>
        );
    }
};

} // namespace

struct BrowserPageSessionLifecycle::Impl final {
    template <typename Event> [[nodiscard]] bool Process(const Event &event)
    {
        return sm.process_event(event);
    }

    sml::sm<BrowserPageSessionLifecycleDefinition> sm;
};

BrowserPageSessionLifecycle::BrowserPageSessionLifecycle() : impl_(std::make_unique<Impl>()) {}

BrowserPageSessionLifecycle::~BrowserPageSessionLifecycle() = default;

bool BrowserPageSessionLifecycle::MarkBrowserContextCreated()
{
    return impl_->Process(BrowserContextCreated{});
}

bool BrowserPageSessionLifecycle::MarkTargetCreated() { return impl_->Process(TargetCreated{}); }

bool BrowserPageSessionLifecycle::MarkAttached() { return impl_->Process(Attached{}); }

bool BrowserPageSessionLifecycle::MarkBaseDomainsEnabled()
{
    return impl_->Process(BaseDomainsEnabled{});
}

bool BrowserPageSessionLifecycle::MarkDetached() { return impl_->Process(Detached{}); }

bool BrowserPageSessionLifecycle::MarkDisposed() { return impl_->Process(Disposed{}); }

bool BrowserPageSessionLifecycle::MarkStopped() { return impl_->Process(Stopped{}); }

} // namespace ws::crawler
