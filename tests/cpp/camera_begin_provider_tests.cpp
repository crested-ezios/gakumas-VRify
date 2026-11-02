#include "host/CameraBeginProvider.hpp"

#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using GakumasVR::Hosted::CameraBeginProvider;

int checks = 0;
int failures = 0;

void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

struct Observation {
    int calls{};
    void* camera{};
};

void Observe(void* user, void* camera) {
    auto& observation = *static_cast<Observation*>(user);
    ++observation.calls;
    observation.camera = camera;
}

void OtherObserver(void* user, void* camera) {
    Observe(user, camera);
}

void ServiceAndTargets() {
    CameraBeginProvider provider;
    const auto api = provider.Service();
    Check(api.abiVersion == GAKU_VRIFY_CAMERA_BEGIN_VERSION, "service ABI version");
    Check(api.structSize == sizeof(GakuVrifyCameraBeginService), "service table size");
    Check(api.context == &provider, "service context identifies this provider");
    Check(api.getTarget && api.subscribe && api.unsubscribe, "complete service table");
    Check(api.getTarget(api.context, 0) == nullptr, "first target initially empty");
    Check(api.getTarget(api.context, 1) == nullptr, "second target initially empty");

    int first{}, second{}, ignored{};
    provider.SetTarget(0, &first);
    provider.SetTarget(1, &second);
    provider.SetTarget(2, &ignored);
    provider.SetTarget(std::numeric_limits<uint32_t>::max(), &ignored);
    Check(api.getTarget(api.context, 0) == &first, "first target retained");
    Check(api.getTarget(api.context, 1) == &second, "second target retained");
    Check(api.getTarget(api.context, 2) == nullptr, "invalid target kind rejected");
    Check(api.getTarget(api.context, std::numeric_limits<uint32_t>::max()) == nullptr,
        "overflow target kind rejected");
    provider.SetTarget(0, nullptr);
    Check(api.getTarget(api.context, 0) == nullptr, "target can be cleared");
    Check(api.getTarget(api.context, 1) == &second, "clearing one target preserves the other");
}

void SubscriptionAndConflicts() {
    CameraBeginProvider provider;
    const auto api = provider.Service();
    Observation owner, stranger;
    int camera{};
    Check(api.subscribe(api.context, nullptr, &owner) == GAKU_E_INVALID_ARGUMENT,
        "null callback rejected");
    Check(api.subscribe(api.context, Observe, &owner) == GAKU_OK, "first observer admitted");
    Check(api.subscribe(api.context, OtherObserver, &owner) == GAKU_E_CONFLICT,
        "different callback conflicts");
    Check(api.subscribe(api.context, Observe, &stranger) == GAKU_E_CONFLICT,
        "different user context conflicts");
    Check(api.subscribe(api.context, nullptr, &owner) == GAKU_E_INVALID_ARGUMENT,
        "invalid replacement does not replace observer");
    Check(api.unsubscribe(api.context, OtherObserver, &owner) == GAKU_E_CONFLICT,
        "foreign callback cannot unsubscribe owner");
    Check(api.unsubscribe(api.context, Observe, &stranger) == GAKU_E_CONFLICT,
        "foreign user context cannot unsubscribe owner");
    provider.Dispatch(&camera, false);
    Check(owner.calls == 1 && owner.camera == &camera, "owner remains after rejected operations");
    Check(stranger.calls == 0, "conflicting observer never runs");
    Check(api.unsubscribe(api.context, Observe, &owner) == GAKU_OK, "owner can unsubscribe");
    provider.Dispatch(&camera, false);
    Check(owner.calls == 1, "unsubscribe closes later callback admission");
    Check(api.unsubscribe(api.context, Observe, &owner) == GAKU_OK, "repeat unsubscribe is harmless");
}

void AliasesAndRepeatedSubscription() {
    CameraBeginProvider provider;
    const auto api = provider.Service();
    Observation owner;
    int target{}, camera{};
    provider.SetTarget(0, &target);
    provider.SetTarget(1, &target);
    Check(api.getTarget(api.context, 0) == api.getTarget(api.context, 1), "aliased targets exposed faithfully");
    Check(api.subscribe(api.context, Observe, &owner) == GAKU_OK, "subscribe with aliased targets");
    Check(api.subscribe(api.context, Observe, &owner) == GAKU_OK, "same subscription is idempotent");
    provider.Dispatch(&camera, false);
    Check(owner.calls == 1, "one dispatch sends one notification after repeated subscribe");
    provider.Dispatch(&camera, false);
    Check(owner.calls == 2, "later source-camera event still notifies once");
}

struct Reentry {
    GakuVrifyCameraBeginService api;
    int calls{};
    bool targetMatched{};
    GakuStatus repeated{GAKU_E_INVALID_ARGUMENT};
    GakuStatus removed{GAKU_E_INVALID_ARGUMENT};
    void* target{};
};

void Reenter(void* user, void*) {
    auto& state = *static_cast<Reentry*>(user);
    ++state.calls;
    state.targetMatched = state.api.getTarget(state.api.context, 0) == state.target;
    state.repeated = state.api.subscribe(state.api.context, Reenter, user);
    state.removed = state.api.unsubscribe(state.api.context, Reenter, user);
}

void ReentrantUnsubscribe() {
    CameraBeginProvider provider;
    int target{}, camera{};
    provider.SetTarget(0, &target);
    Reentry state{provider.Service()};
    state.target = &target;
    Check(state.api.subscribe(state.api.context, Reenter, &state) == GAKU_OK, "reentrant observer admitted");
    provider.Dispatch(&camera, false);
    Check(state.calls == 1 && state.targetMatched, "callback can query provider without deadlock");
    Check(state.repeated == GAKU_OK, "callback can repeat its subscription");
    Check(state.removed == GAKU_OK, "callback can unsubscribe itself without deadlock");
    provider.Dispatch(&camera, false);
    Check(state.calls == 1, "self-unsubscribe prevents the next notification");
}

void ThrowingObserver(void* user, void*) {
    ++static_cast<Observation*>(user)->calls;
    throw std::runtime_error("synthetic observer failure");
}

void ExceptionIsolation() {
    CameraBeginProvider provider;
    Observation throwing, replacement;
    int camera{};
    Check(provider.Subscribe(ThrowingObserver, &throwing) == GAKU_OK, "throwing observer admitted");
    provider.Dispatch(&camera, false);
    provider.Dispatch(&camera, false);
    Check(throwing.calls == 2, "callback exceptions do not escape or silently remove observer");
    Check(provider.Unsubscribe(ThrowingObserver, &throwing) == GAKU_OK, "throwing observer can unsubscribe");
    Check(provider.Subscribe(Observe, &replacement) == GAKU_OK, "provider remains usable after exception");
    provider.Dispatch(&camera, false);
    Check(replacement.calls == 1, "replacement receives source camera");
}

int nullUserCalls = 0;
void ObserveWithoutUser(void* user, void*) {
    if (user == nullptr) ++nullUserCalls;
}

void CameraFiltering() {
    CameraBeginProvider provider;
    Observation owner;
    int camera{};
    Check(provider.Subscribe(Observe, &owner) == GAKU_OK, "filter observer admitted");
    provider.Dispatch(nullptr, false);
    provider.Dispatch(&camera, true);
    provider.Dispatch(nullptr, true);
    Check(owner.calls == 0, "null and eye cameras are filtered");
    provider.Dispatch(&camera, false);
    Check(owner.calls == 1 && owner.camera == &camera, "source camera passes filter unchanged");
    Check(provider.Unsubscribe(Observe, &owner) == GAKU_OK, "filter observer removed");
    nullUserCalls = 0;
    Check(provider.Subscribe(ObserveWithoutUser, nullptr) == GAKU_OK, "null user context is supported");
    provider.Dispatch(&camera, false);
    Check(nullUserCalls == 1, "valid callback receives its null context");
}

void ClosedAdmission() {
    CameraBeginProvider provider;
    const auto retained = provider.Service();
    Observation owner;
    int target{}, camera{};
    provider.SetTarget(0, &target);
    provider.SetTarget(1, &target);
    Check(retained.subscribe(retained.context, Observe, &owner) == GAKU_OK, "observer admitted before close");
    provider.Close();
    provider.Close();
    Check(retained.getTarget(retained.context, 0) == nullptr, "retained service hides closed first target");
    Check(retained.getTarget(retained.context, 1) == nullptr, "retained service hides closed second target");
    provider.SetTarget(0, &target);
    Check(retained.getTarget(retained.context, 0) == nullptr, "target cannot reopen closed provider");
    Check(retained.subscribe(retained.context, Observe, &owner) == GAKU_E_SHUTTING_DOWN,
        "retained service rejects new subscription after close");
    provider.Dispatch(&camera, false);
    Check(owner.calls == 0, "close prevents new dispatch admission");
    Check(retained.unsubscribe(retained.context, Observe, &owner) == GAKU_OK,
        "resident unsubscribe remains safe after close");
}

struct CloseDuringCallback {
    CameraBeginProvider* provider;
    int entered{};
    int finished{};
};

void CloseFromCallback(void* user, void*) {
    auto& state = *static_cast<CloseDuringCallback*>(user);
    ++state.entered;
    state.provider->Close();
    ++state.finished;
}

void ReentrantClose() {
    CameraBeginProvider provider;
    CloseDuringCallback state{&provider};
    int camera{};
    Check(provider.Subscribe(CloseFromCallback, &state) == GAKU_OK, "closing observer admitted");
    provider.Dispatch(&camera, false);
    Check(state.entered == 1 && state.finished == 1, "captured callback can close provider and finish");
    provider.Dispatch(&camera, false);
    Check(state.entered == 1, "reentrant close prevents future notification");
    Check(provider.Subscribe(CloseFromCallback, &state) == GAKU_E_SHUTTING_DOWN,
        "reentrant close is permanent");
}

void Run(const char* name, void (*test)()) {
    try {
        test();
        std::cout << "PASS: " << name << '\n';
    } catch (const std::exception& error) {
        ++failures;
        std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
    } catch (...) {
        ++failures;
        std::cerr << "FAIL: " << name << ": unexpected exception\n";
    }
}
} // namespace

int main() {
    Run("service table and target boundaries", ServiceAndTargets);
    Run("observer validation and owner conflicts", SubscriptionAndConflicts);
    Run("aliased targets and repeat subscription", AliasesAndRepeatedSubscription);
    Run("reentrant query and unsubscribe", ReentrantUnsubscribe);
    Run("callback exception isolation", ExceptionIsolation);
    Run("source, eye and null camera filtering", CameraFiltering);
    Run("closed admission through retained service", ClosedAdmission);
    Run("close from an in-flight callback", ReentrantClose);
    std::cout << "Camera-begin provider: " << checks << " checks, " << failures << " failed cases\n";
    return failures == 0 ? 0 : 1;
}
