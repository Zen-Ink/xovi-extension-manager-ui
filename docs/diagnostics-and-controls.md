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

Both firmware adapters observe `LanguageAndKeyboard.languageSettings.languageCode`
and pass its initial value and subsequent changes to `ManagerNavigation.setNativeLanguage`.
This updates the session's `xoviNativeUiLanguage` property; the SDK language service
reloads registered catalogs and retranslates attached engines. No settings page
recreation, config-file write or xochitl restart is required. The last observed
native value remains authoritative after the native settings page closes.

The configuration file is the startup fallback, not the live authority. Newly
opened plugin engines cannot reset the session language to their default English.
Consumers of the header-only helper must be rebuilt with the updated SDK; this
does not translate plugins that use their own unrelated localization mechanism.
