import QtQuick
import ark.controls as ArkControls
import ark.tokens as ArkTokens

ButtonBase {
    iconBase: "qrc:/xovi/manager/icons/"
    type: checked ? ArkTokens.Button.primary : ArkTokens.Button.secondary
    states: ArkControls.Button.ButtonState.Idle | ArkControls.Button.ButtonState.Disabled
}
