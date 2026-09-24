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

Manager-ui observes Qt's successful `QCoreApplication::installTranslator` call,
then identifies xochitl's native `reMarkable_*.qm` translator and reads its
`QTranslator::language()` metadata in memory. The original Qt operation and its
return value are preserved. Plugin translators cannot select the native locale.

The SDK follows only that in-process native value. It does not read xochitl.conf,
APP_LOCALE, LANG, or a new engine's default locale. `ManagerBridge.uiLanguageReady`
is false and `uiLanguage` is empty until the native translator is observed.
First observation and runtime language changes retranslate all attached engines.
Each plugin continues to own its translation files. Rebuild SDK consumers and
manager-ui together; older installed binaries can retain the old fallback code.

### Device findings, 2026-09-23

The device did contain the updated binary, including `setNativeLanguage`, the
new QMD hooks and the `native language:` logging string. Xochitl logged
`Activated translation: en`, but no manager native-language observation appeared.
The previous QMD hooks depended on language UI components being instantiated;
resource patch acceptance did not prove those hooks ran. They have been removed.
The old SDK then read stale `Language=zh_CN`, which explains Chinese labels and
the translated Extensions entry despite native English. This was an incomplete
startup adapter, not simply a failure to update the plugin.

The native English resource supplied in the firmware contains `language=en_US`.
The Qt translator observer reads that actual metadata, without waiting for the
Language page, changing configuration or inferring language from translated text.
Verify the resulting `native language:` log after a future authorized build and
deployment. This source update has not yet been compiled or tested on-device.

AppLoad still has its own independent environment-first selector; that selector
is not used by manager-ui or the SDK helper.

### Default PINs

NFJ ASK AI and rmfakecloud-control declare zero default locations. On the inspected
device, both already had `settings=true` saved in `launchers.json`; cloud's current
provider/manifest did not itself request a default pin. Saved records do not record
whether their historical origin was a default or a manual choice. Changing defaults
applies to first discovery only and does not silently clear these existing records.

## Pinned entry capacity

Pinned entries use the native layout's remaining extent and native control size.
There is no fixed one-entry page or three-entry bottom-bar limit. Only real
overflow adds a page action, and that action occupies one of the available slots.
If just one slot remains, it opens the Extensions inventory instead of creating
an extra control outside the available space. No available slot means no injected
control; the separate Extensions settings entry remains independently managed.


### 设置页面的返回路径

打开插件设置时保存当前管理器页面（列表、详情、通知或另一个插件页）。页面调用 `settingsContext.close()` 与管理器 Back 使用同一返回历史，优先恢复上一个页面；只有已退到入口根页面时才关闭入口层。首次由 QMD Loader 展示的页面作为根页面，后续打开请求不会覆盖原入口。插件页面返回后会重新创建，插件内部未保存的表单状态不属于当前导航历史的保存范围。

### 3.27 底栏 PIN 更新

3.27 的原生 CreateMenu 使用 `sidePadding`，3.28 使用 `horizontalPadding`。底栏注入必须引用对应版本的属性，否则可用宽度为 NaN，入口容量计算失效。LauncherEntries 也对无效测量值采取与“尚未取得尺寸”相同的处理，避免静默丢失入口。

`tests/check-bottom-pin-runtime.py` 将 QMD 应用于对应固件的 CreateMenu，并运行实际 LauncherEntries，检查主页隐藏期间增删 PIN、恢复显示、清空后重新添加以及原生搜索入口的可见性变化。C++ 管理器和原生 Action 使用测试替身，不代表已部署实机验证。
