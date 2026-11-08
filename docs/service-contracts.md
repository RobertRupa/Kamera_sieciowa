# Service contracts, DTOs and controller boundary

A **service contract** is a stable public interface, its pre/postconditions, errors, ownership semantics, threading model and observable events. It is NOT merely a class named `Service`.

## Example C++ ports (illustrative, adapt to toolchain)
```cpp
// components/app_contracts/include/app_contracts/NetworkDto.hpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace app {
enum class NetworkMode : uint8_t { Unconfigured, ProvisioningAp, Connecting, Connected, Error };
enum class AppError : uint8_t { None, InvalidArgument, Unauthorized, Busy, Timeout,
                                StorageFailure, NetworkFailure, Unsupported };
struct Status {
    AppError error{AppError::None};
    [[nodiscard]] bool ok() const noexcept { return error == AppError::None; }
};
struct WifiCredentials { std::string ssid; std::string password; };
struct NetworkSnapshot {
    NetworkMode mode{NetworkMode::Unconfigured};
    std::string ipv4;
    int32_t rssiDbm{0};
    bool hasIp{false};
};
// Events carry snapshots or value data, never borrowed pointers with uncertain lifetime.
enum class NetworkEventType : uint8_t { Connecting, Connected, Disconnected, Provisioning };
struct NetworkEvent { NetworkEventType type; NetworkSnapshot snapshot; };
}
```

```cpp
// components/app_contracts/include/app_contracts/IWifiService.hpp
#pragma once
#include "app_contracts/NetworkDto.hpp"

namespace app {
class IWifiService {
public:
    virtual ~IWifiService() = default;
    // Nonblocking: schedules work. No raw ESP-IDF handles escape the interface.
    [[nodiscard]] virtual Status connect(const WifiCredentials& credentials) = 0;
    [[nodiscard]] virtual Status disconnect() = 0;
    [[nodiscard]] virtual NetworkSnapshot snapshot() const = 0;
};
class IConfigStore {
public:
    virtual ~IConfigStore() = default;
    [[nodiscard]] virtual Status saveWifi(const WifiCredentials& value) = 0;
    [[nodiscard]] virtual Status loadWifi(WifiCredentials& out) const = 0;
    [[nodiscard]] virtual Status clearWifi() = 0;
};
class IProvisioningService {
public:
    virtual ~IProvisioningService() = default;
    [[nodiscard]] virtual Status startApWizard() = 0;
    [[nodiscard]] virtual Status stopApWizard() = 0;
};
}
```

These are interface examples, not drop-in firmware: a real implementation must address secure string handling, heap budgets, NVS failure modes and platform-specific asynchronous event lifetimes. Adjust `std::string` to bounded buffers where required. Never store passwords in event DTOs or logs.

## Service implementation rules
- Interfaces belong in `app_contracts` (or smallest component owning the domain policy).
- Implementers reside in distinct `connectivity` / `persistence` / `provisioning` components.
- Application orchestration e.g. `NetworkCoordinator` depends on `IWifiService` and `IConfigStore` by constructor injection.
- Every public method documents whether it is thread-safe, can block, modifies persistent state, and emits events. Do not retain references to temporaries.
- Make ownership of `esp_event_handler_instance_t`, `httpd_handle_t`, mutexes and storage handles explicit.
- Return structured error state with reason/retry hints. Use transaction boundaries for credentials: validate → persist securely → connect → confirm; define whether rollback occurs on failure.

## HTTP API contract (baseline `/api/v1`)
| Method/path | Auth | Purpose | Expected result |
| --- | --- | --- | --- |
| `GET /api/v1/health` | optional, minimal | liveness/version | 200 + health JSON |
| `GET /api/v1/device` | session | capabilities, firmware, uptime, target | 200 |
| `GET /api/v1/network` | session | STA/AP/IP/mDNS status, **no passwords** | 200 |
| `POST /api/v1/network/scan` | session or limited provisioning context | enqueue scan | 202 + operation ID |
| `GET /api/v1/network/scan/{id}` | matching session | scan result/status | 200 / 404 |
| `PUT /api/v1/network/credentials` | session or bounded provisioning token | validate + secure save + reconnect | 202 |
| `POST /api/v1/network/reprovision` | admin | start AP wizard | 202 |
| `GET /api/v1/settings` | session | nonsecret settings and theme/locale | 200 |
| `PATCH /api/v1/settings` | session | validated preference changes | 200 |
| `POST /api/v1/auth/login` | public with throttling | establish session | 200/401 |
| `POST /api/v1/auth/logout` | session | expire session | 204 |

Use consistent JSON envelope: success `{"data": {...}}`, failure `{"error": {"code":"invalid_ssid","message":"...","retryable":false}}`. IDs may identify asynchronous operations; no long synchronous scans in HTTP request tasks. Content type `application/json`, UTF-8, max body length, bounded JSON depth, request timeouts. Keep HTTP codes meaningful: 400 invalid input, 401 unauthenticated, 403 forbidden, 404 missing, 409 conflict/busy, 429 rate limit, 500 internal failure, 503 unavailable. Define schema/field ranges in OpenAPI (`docs/api/openapi.yaml` in actual projects); generate typed frontend client where useful.

## Event contracts
Prefer a single event category/topic naming convention (e.g. `network.state.changed`, `settings.updated`, `wifi.scan.completed`) with fixed payload and task context. Producers enqueue, consumers own their state and choose whether to drop/coalesce/retry. LVGL receives state snapshots via GUI-owned queue; web can poll, SSE or WebSocket only where justified by load.

## Compatibility
Version REST endpoints and persistent NVS schema independently. Never silently change units/enum values, locale keys or API field names. Validate schema at the boundary and translate ESP-IDF errors only in adapters.