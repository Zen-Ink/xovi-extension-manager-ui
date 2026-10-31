# XOVI Extension Manager UI

Adds an **Extensions** entry to the settings sidebar. The manager page
and third-party settings are created on demand, rather than during xochitl
startup. Native providers and pure QML packages share the same page host.

The UI displays packages, enable/disable controls, custom settings, resource
processing diagnostics and next-start injection switches. Failed custom page
creation leaves a Back button and an error. Configuration saving does not claim
that a plugin has applied its new values.

Requires `xovi-extension-manager >=0.3.0`, `qt-resource-rebuilder >=0.4.0`, and
`xovi-message-broker >=0.2.0`. It deliberately uses native broker requests, not
blocking broadcasts back to QML. The settings-entry patch supports the different
3.27 and 3.28 roots; unsupported layouts fail the patch and retain original QML.

## Integrate another plugin

**External plugins do not need the SDK to use manager-ui.** Native plugins may
also register/open their existing pages through QML, optionally injected by QMD.
Alternatively, a native plugin can expose a settings provider using the SDK's C
header, independently of a QML registration object's lifetime. Shared controls
and translation helpers are optional in either case.

See [two integration paths with minimal examples](docs/integration.md) for the
tradeoffs, direct page opening, PIN registration and native exports.

## Build manager-ui itself

The SDK submodule below is a dependency of **this implementation**, not a
requirement imposed on plugins that open/register their own QML pages.

Build both supported architectures using the plugin's own script:

```sh
git submodule update --init --recursive
TOOLCHAIN_ROOT=/path/to/toolchian ./build.sh all
# Or ./build.sh aarch64 / ./build.sh arm32
```

Libraries are written to `build/<arch>/`. The toolchain root contains `rmpp`
and `rm2` SDK environment scripts. The rm-xovi-extensions workspace can also
build and package this plugin through its top-level `build.sh`.

Host and firmware tests under `tests/` are integration tests: run them from an
rm-xovi-extensions workspace containing the manager, plugin translation catalogs
and extracted firmware resources. Firmware resources are not distributed here.

## Official update preference (3.27 / 3.29)

Broker signal `xovi-extension-manager-ui$officialUpdateControl` accepts
`{"action":"read"}` or `{"action":"set","enabled":false}` (also `true`).
Use the existing serialized native broker client and allow 90 seconds for the
native systemd calls. The C export is `char *xem_official_update_control(const char *)`;
the caller frees its JSON response with `free` as for other broker exports.

On RM2 3.27.1.0 and 3.29.1.0, startup captures the shared `UpdateEnabled`
instance created by the resident softwareupdate module. Exact ELF build IDs
and constructor call instructions identify the two supported call sites. Only
xochitl's QObject C2 GOT entry is intercepted; other callers are passed through.
After the original QObject constructor returns, a guarded queued callback waits
for derived initialization and verifies the public metaobject class name.
The ordinary MainView startup binds the existing `RetailDemo` singleton.
No Software page creation or visit is required, and closing that page does not
release the module-owned object. No private object layout is read.

References are borrowed and cleared on actual QObject destruction. Before the
startup callback has bound the instance, requests return unavailable immediately
with null native fields rather than waiting for the GUI event loop to start.
Unsupported executable identities also remain unavailable. All live object
reads/writes execute on the GUI thread; calls already on that thread run directly.
Native updates-off/on behavior and forced/retail constraints are retained.

Replies contain `ok`, nullable booleans `enabled`, `forced`, `retail`,
`persistentEnabled`, nullable string `unitFileState`, and nullable boolean
`readbackMatches`. Missing state is `null`, never an inferred `false`.
On set, `ok=true` requires the native readback and persistent systemd state to
equal the request. `forced=true` prevents disabling; retail mode prevents
changing the preference, matching the native UI. Errors include
`official-update-object-unavailable`, `official-updates-forced`,
`official-retail-control-disabled`, `official-update-readback-mismatch`, and
`official-update-persistence-unverified`. Read success only means the native
enabled property was readable; inspect nullable fields independently.

The native setter controls `update-engine.service`. Persistence is read via
systemd `GetUnitFileState`; no configuration key is fabricated. Turning updates
off also hides the official manual-check button. This API neither checks for an
official target nor starts an update. The observed official DBus interface
exposes combined `AttemptUpdate()`, observational `GetStatus()`, and
`ResetStatus()`; it does not expose separate check-only/download/install calls.
Preference control alone does not prove compatibility-first update ordering.

## Checkout dependencies

Clone with `git clone --recurse-submodules`, or run
`git submodule update --init --recursive` after pulling. The `sdk` submodule
pins the shared settings, navigation, notifications and QML controls API.
The `xovi` submodule supplies the binding generator; `XOVI_REPO` can override it.

See [error presentation and component ownership](docs/diagnostics-and-controls.md).

## Notification events (breaking migration)

Notifications use event subscriptions plus state queries; the old polling API
has been removed. Button delivery and business completion are separate states.
See [native and QML examples](https://github.com/Zen-Ink/xovi-extension-manager-sdk/blob/master/docs/notification-events.md).
Update manager, manager-ui and native notification producers together.
Socket services live in manager and do not require this UI; see the SDK's
[Socket service guide](https://github.com/Zen-Ink/xovi-extension-manager-sdk/blob/master/docs/managed-sockets.md).

`python3 tests/check-notification-events.py` runs the actual QML notification
store with a signal-driven backend stub, without compiling the plugin. Native
callback/lifetime tests remain in `tests/run-host-tests.sh`.
