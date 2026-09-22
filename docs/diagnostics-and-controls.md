# Error presentation and control ownership

## Error presentation

The package list shows a classified failure before a pending restart. Details
include the original code/cause, translated summary and recovery advice, plus the
raw loader message. A pending saved change is reported separately: restarting
applies that change; it does not promise to fix an error.

For `wrong ELF class`, the primary message says the plugin **or dependency** has
incompatible ELF bitness. It recommends a matching build, not restart. Older
backends without diagnostic objects still show load failure before restart.

Operation failures use backend recovery advice in notifications and messages.
QML missing resources, unsupported declarations and expired contexts have
specific hints. Reopening is advised for expired pages, not a blanket restart.

The UI maintains en/zh_CN/zh_TW translations in the `Diagnostics` context; plugins
still own their own language catalogs. Machine codes and raw details are retained.
See [backend semantics](https://github.com/Zen-Ink/xovi-extension-manager/blob/master/docs/diagnostics.md).

## Where components live

- SDK `qml/org/xovi/Controls`: shared typography, buttons, labels, sidebar items,
  panels and pagination. Embedded by each consumer that chooses to use them.
- UI `resources/controls`: thin wrappers such as EButton/ELabel/Pager/navigation,
  plus manager-specific notification state and progress presentation.
- Individual plugins: their own domain components (for example Advanced Settings
  selectors/export popups), retained alongside their pages.

There is therefore no single all-purpose component library confined to the SDK.
The shared controls mostly build on xochitl's `ark.controls`/`ark.tokens`, but
some currently replace visual/content branches. That is an adaptation burden,
not a guarantee of native appearance across firmware versions.

## Recommended boundary

Prefer native Ark components and a small optional adaptation layer: central
font tokens, accessibility fixes and genuinely shared pagination. Avoid growing
an independent replacement for the native widget/theme system. Keep notifications
and manager-specific navigation in UI, and domain controls in their plugins.

A shared public component should have one implementation; wrappers should only
provide defaults rather than fork layout or behavior. Plugins need neither
shared controls nor translation helpers merely to register/open settings. Future
visual consolidation should be validated separately on supported firmware; this
error-handling change does not redesign existing controls.

## Runtime language synchronization

Both firmware adapters observe the native `languageSettings.languageCode` model.
`LocalizationShortcut` connects it during native localization initialization;
`LanguageAndKeyboard` and `LanguageSelector` also connect their initial value and
changes to `ManagerNavigation.setNativeLanguage`. Opening Language settings is
not required for initial synchronization. These adapters observe the same native
model, rather than independently choosing a language.
This updates the session's `xoviNativeUiLanguage` property; the SDK language service
reloads registered catalogs and retranslates attached engines. No settings page
recreation, config-file write or xochitl restart is required. The last observed
native value remains authoritative after the native settings page closes.

The configuration file is the startup fallback, not the live authority. Newly
opened plugin engines cannot reset the session language to their default English.
Consumers of the header-only helper must be rebuilt with the updated SDK; this
does not translate plugins that use their own unrelated localization mechanism.

### Diagnosing a language mismatch

Use xochitl's `rm.localization.language` / `Activated translation: ...` log to
check the actual native selection. `LANG=en_US.UTF-8` is an OS locale, not proof
that the UI is English. The manager logs `[extension-manager-ui] native language:`
when its native adapter observes a new selection.

There are three independent selectors in the current workspace:

| Owner | Language selection |
| --- | --- |
| xochitl | Native `LanguageSettings.languageCode` |
| SDK consumers (manager-ui and migrated plugins) | Native session value first; config fallback until observed; environment/system fallback only if no config/session value |
| AppLoad | `APP_LOCALE`, then `LANG`, then `/data/xochitl.conf`, then system locale |

AppLoad is an independent loader, not the language authority for manager-ui.
Plugin catalogs are separate resources, not separate language selectors.

On the inspected 3.27.3.0 device, xochitl logged `Activated translation: en`, but
`/home/root/.config/remarkable/xochitl.conf` contained `Language=zh_CN`, and
`/data/xochitl.conf` did not exist (including in xochitl's mount namespace).
The installed manager-ui lacked `setNativeLanguage`, so the legacy config fallback
selected Chinese. Updating only a QML page cannot update that older native binary.
The runtime bridge fixes this discrepancy without modifying the user's config.
Rebuild/deploy SDK consumers together so an older plugin cannot instantiate an
older shared language service first.

## Pinned entry capacity

Pinned entries use the native layout's remaining extent and native control size.
There is no fixed one-entry page or three-entry bottom-bar limit. Only real
overflow adds a page action, and that action occupies one of the available slots.
If just one slot remains, it opens the Extensions inventory instead of creating
an extra control outside the available space. No available slot means no injected
control; the separate Extensions settings entry remains independently managed.
