// Compile the production hosted hook facade against a finite fake host. This
// checks the adapter's behavior; Host/MinHook internals have their own tests.
#include "host/HostedHooks.hpp"
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void* Token(size_t value) { return reinterpret_cast<void*>(value); }
enum class Result { Reject, EnableFailure, Success };
struct Host {
    Result result = Result::Reject;
    size_t batchCalls = 0, singleCalls = 0, logCalls = 0;
    std::vector<void*> released;
    void* firstBefore = nullptr;
    void* secondBefore = nullptr;
};
GakuStatus Acquire(void* context, void*, void*, void**) {
    ++static_cast<Host*>(context)->singleCalls;
    return GAKU_E_INTERNAL;
}
GakuStatus Batch(void* context, uint32_t count, const GakuHookRequest* requests) {
    auto& host = *static_cast<Host*>(context);
    ++host.batchCalls;
    Check(count == 2 && requests, "facade split or changed a batch");
    Check(requests[0].target == Token(1) && requests[1].target == Token(2),
        "facade changed target order");
    Check(requests[0].detour == Token(11) && requests[1].detour == Token(12),
        "facade changed detours");
    Check(*requests[0].original == host.firstBefore && *requests[1].original == host.secondBefore,
        "facade cleared originals before host validation");
    if (host.result == Result::Reject) return GAKU_E_CONFLICT;
    *requests[0].original = Token(101);
    *requests[1].original = Token(102);
    return host.result == Result::EnableFailure ? GAKU_E_INTERNAL : GAKU_OK;
}
GakuStatus Release(void* context, void* target) {
    static_cast<Host*>(context)->released.push_back(target);
    return GAKU_OK;
}
void Log(void* context, GakuLogLevel level, const char* message) {
    Check(level == GAKU_LOG_ERROR && message && message[0], "missing hook failure diagnostic");
    ++static_cast<Host*>(context)->logCalls;
}
} // namespace

int main() {
    try {
        using namespace GakumasVR::Hooks;
        Host host;
        const GakuLogService log{GAKU_SERVICE_VERSION, sizeof(log), &host, Log};
        const GakuHookService service{GAKU_SERVICE_VERSION, sizeof(service), &host,
            Acquire, Release, Batch};
        void* first = Token(201);
        void* second = Token(202);
        const Request requests[]{{Token(1), Token(11), &first, "first"},
            {Token(2), Token(12), &second, "second"}};
        Check(!Initialize() && !IsInitialized(), "unbound facade initialized a private engine");
        Check(!CreateAndEnableBatch(requests, 2), "unbound facade installed hooks");
        Check(first == Token(201) && second == Token(202), "unbound request changed originals");
        auto invalid = service;
        invalid.abiVersion = 999;
        Check(!BindHost(invalid, log), "facade accepted wrong service ABI");
        invalid = service;
        --invalid.structSize;
        Check(!BindHost(invalid, log), "facade accepted a short service");
        invalid = service;
        invalid.acquireBatch = nullptr;
        Check(!BindHost(invalid, log), "facade accepted missing batch capability");
        invalid = service;
        invalid.release = nullptr;
        Check(!BindHost(invalid, log), "facade accepted missing release capability");
        Check(BindHost(service, log) && Initialize() && IsInitialized(), "valid host binding failed");
        Check(FirstHostFailure() == GAKU_OK, "fresh host binding inherited a failure");
        Check(!BindHost(service, log), "facade replaced a live host binding");
        Check(!CreateAndEnableBatch(nullptr, 2) && LastHostStatus() == GAKU_E_INVALID_ARGUMENT,
            "null batch was accepted");
        Check(!CreateAndEnableBatch(requests, 0) && LastHostStatus() == GAKU_E_INVALID_ARGUMENT,
            "empty batch was accepted");
        Check(!CreateAndEnableBatch(requests, 257) && LastHostStatus() == GAKU_E_INVALID_ARGUMENT,
            "oversized batch was accepted");
        Check(host.batchCalls == 0, "invalid batch reached host");
        Check(FirstHostFailure() == GAKU_OK, "local validation fabricated a host failure");

        host.firstBefore = first;
        host.secondBefore = second;
        Check(!CreateAndEnableBatch(requests, 2) && LastHostStatus() == GAKU_E_CONFLICT,
            "validation conflict was hidden");
        Check(FirstHostFailure() == GAKU_E_CONFLICT, "first host conflict was not latched");
        Check(first == Token(201) && second == Token(202), "conflict changed caller originals");
        Check(host.released.empty(), "facade tried rollback after validation failure");

        host.result = Result::EnableFailure;
        Check(!CreateAndEnableBatch(requests, 2) && LastHostStatus() == GAKU_E_INTERNAL,
            "partial enable failure was hidden");
        Check(FirstHostFailure() == GAKU_E_CONFLICT, "later failure overwrote first host conflict");
        Check(first == Token(101) && second == Token(102),
            "partial enable failure discarded live trampolines");
        Check(host.released.empty(), "partial enable failure rolled back potentially live detours");

        host.result = Result::Success;
        host.firstBefore = first;
        host.secondBefore = second;
        Check(CreateAndEnableBatch(requests, 2) && LastHostStatus() == GAKU_OK,
            "successful batch failed");
        Check(FirstHostFailure() == GAKU_E_CONFLICT, "later success erased first host conflict");
        Check(host.singleCalls == 0 && host.batchCalls == 3, "batch was split into single acquisitions");
        Check(host.logCalls == 2, "host failures were not diagnosed once per batch");
        Shutdown();
        Check(!Initialize() && !IsInitialized(), "shutdown facade remained available");
        Check(FirstHostFailure() == GAKU_E_CONFLICT, "shutdown erased first host failure");
        Check(host.released == std::vector<void*>({Token(2), Token(1)}),
            "shutdown did not release unique owned requests in reverse order");
        Check(first == Token(101) && second == Token(102), "shutdown discarded originals");
        Shutdown();
        Check(host.released.size() == 2, "repeat shutdown released hooks twice");
        Check(!CreateAndEnableBatch(requests, 2) && LastHostStatus() == GAKU_E_SHUTTING_DOWN,
            "stopped facade accepted new work");
        std::puts("PASS: production hosted hook facade; atomic batch forwarding, exact failure status, original lifetime and owner releases");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
