# XOVI Extension Manager UI

Adds an **Extensions** entry to the 3.27/3.28 settings sidebar. The manager page
and third-party settings are created on demand, rather than during xochitl
startup. Native providers and pure QML packages share the same page host.

The UI displays packages, enable/disable controls, custom settings, resource
processing diagnostics and next-start injection switches. Failed custom page
creation leaves a Back button and an error. Configuration saving does not claim
that a plugin has applied its new values.

Requires `xovi-extension-manager >=0.2.0`, `qt-resource-rebuilder >=0.4.0`, and
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

## Checkout dependencies

Clone with `git clone --recurse-submodules`, or run
`git submodule update --init --recursive` after pulling. The `sdk` submodule
pins the shared settings, navigation, notifications and QML controls API.
The `xovi` submodule supplies the binding generator; `XOVI_REPO` can override it.
