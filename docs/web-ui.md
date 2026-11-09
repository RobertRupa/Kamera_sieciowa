# WebGUI architecture, i18n and theme system

## Full framework, static firmware assets
Use **Vue 3 + Vite + PrimeVue + Pinia + Vue I18n**, plus Vue Router for multiple screens. Do not recreate a component library, client router or translator. Build the frontend on the host/CI using pinned package lockfile, then embed or mount only prepared, compressed HTML/CSS/JS/assets on the ESP32 via `esp_http_server` (or `esp_https_server`). **No Node.js runtime on microcontroller.** Measure firmware partition/Flash budget, Gzip response headers, content types and caching. Hashed assets can be immutable; revalidate `index.html`. Do not ship source maps, test data or credentials in firmware.

## Baseline screens
Dashboard (device, runtime, Wi-Fi, IP/mDNS, memory, uptime, firmware), Wi-Fi manager and provisioning wizard, Settings (device name, locale/theme), Security (password and sessions), Diagnostics (reason for reset, logs/metrics with no secrets), About (versions/licenses). Hide unsupported features based on capabilities returned by backend; never show inert controls.

## UI MVC/clean boundaries
- **Model**: typed API DTOs and Pinia view state; backend remains source of truth for device settings.
- **View**: Vue SFC/PrimeVue components, no NVS, Wi-Fi or backend logic.
- **Controller/presenter**: composables/stores orchestrate user actions and map API results. One API client handles auth/error parsing.
- **Firmware API controllers**: parse and validate request → application service → DTO serialization. They must not directly mutate LVGL, Wi-Fi driver or NVS.
- Use stable versioned `/api/v1` contract from [Service contracts](service-contracts.md). For scans/connect use 202 + operation ID, not long synchronous blocking handlers.

## Theme
Implement `system`, `light` and `dark` with PrimeVue theme presets/design tokens. Save `ui.theme` in versioned device settings (optional browser-local override has documented precedence); update immediately without reboot. System preference must respond to OS appearance when supported. Responsive UI from 320px upwards, keyboard navigation, labels, focus and suitable contrast.

## Localization
English (`en`) is the default and Polish (`pl`) is bundled on every project. Use semantic translation keys in `web/src/locales/en.json` and `pl.json`, with parity test, localized plural/number/date formatting, `pl → en` fallback and language selector in normal UI and captive wizard. Show Polish letters (ą ć ę ł ń ó ś ź ż). HTTP error codes remain language-neutral; client translates user-facing error messages. No embedded English UI copy in Vue components.

## Suggested structure
```text
web/src/
├── api/{client.ts,device.ts,network.ts,settings.ts,types.ts}
├── components/
├── views/{DashboardView.vue,WifiView.vue,WizardView.vue,SettingsView.vue}
├── stores/{device.ts,network.ts,preferences.ts}
├── composables/
├── locales/{en.json,pl.json}
├── themes/
├── router/index.ts
└── App.vue
```

## Security and failures
Authenticated session for state changes; bounded JSON, method whitelist, CSRF for cookie auth, conservative CORS, sane cookie lifetime, no passwords in GET/status. HTTPS/secure transport for release according to threat model; local lab HTTP should show insecure development warning. Report network disconnect, async scan/retry and AP→STA switch clearly, without permanent loading spinner. Use ordinary frontend unit/component tests and browser/mobile-width smoke checks.

## Wi-Fi Provisioning Client Status

`WifiView.vue` reads readiness from `/api/v1/provisioning/status`, obtains one
short-lived peer-bound session, starts and polls a bounded scan, allows selecting
supported secured networks or entering an SSID manually, and polls the async
connection operation. Unsupported scan security modes are disabled rather than
advertised as connectable. Credentials and the token are not written to browser
storage; the password field is cleared after the request lifecycle.

The client requires same-origin hosting because the firmware rejects foreign
origins and does not enable CORS. The production Vite assets are embedded as
gzip data and served from both AP and STA addresses. After station connection,
the UI shows a link to the assigned STA IP and continues in read-only mode there;
session, scan and credential-write controls remain AP-only. Captive DNS/probe
integration remains pending. The inspected captive component
does not list ESP32-P4 as a supported target; do not enable it or substitute
broad handwritten DNS interception without a verified P4-compatible
implementation. Live browser/API behavior on an associated phone has not yet
been tested.