import QtQuick

Panel {
    id: root
    required property var presentation
    required property var shell
    readonly property string selection: (presentation.menuActions[presentation.menuFocus] || {}).id || ""
    readonly property bool settingsRoot: presentation.panel === "display"
    readonly property bool appearance: presentation.panel.indexOf("display") === 0
    radius: 30
    surface: Qt.tint(Theme.paper,"#09c29948")
    patterned: true
    GameActivity { anchors.fill: parent; visible: !root.presentation.panel.length; presentation: root.presentation; shell: root.shell }
    Item {
        anchors.fill: parent; visible: root.presentation.panel.length>0
        Text { id: heading; x: 29; y: 23; width: parent.width-105; text: root.presentation.menuCaption; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 27; elide: Text.ElideRight; textFormat: Text.PlainText }
        SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 25; y: 22; icon: "x"; label: "Back"; onClicked: root.presentation.cancel() }
        Item {
            id: identity; x: 29; y: 76; width: Math.min(220,parent.width*.29); height: parent.height-y-27
            GameFrame { id: picture; presentation: root.presentation; maximumWidth: identity.width; maximumHeight: 145 }
            Text { id: name; y: picture.height+17; width: parent.width; text: root.presentation.gameTitle; font.family: Theme.displayFamily; font.pixelSize: 22; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight; color: Theme.ink; textFormat: Text.PlainText }
            Text { y: name.y+name.height+15; width: parent.width; text: root.presentation.ready ? (root.appearance ? "For this game only\n\nFormat, pixels and selected artwork apply next launch.\nLive previews are marked separately." : "Your game stays open.") : "Release the controls"; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
        }
        Rectangle { x: identity.x+identity.width+21; y: identity.y; width: 1; height: identity.height; color: Theme.surfaceColor("#30477865") }
        Item {
            id: content; x: identity.x+identity.width+43; y: identity.y; width: parent.width-x-29; height: identity.height
            Grid {
                visible: root.settingsRoot; columns: 2; spacing: 13; width: parent.width
                Repeater { model: root.settingsRoot ? root.presentation.menuActions.slice(0,4) : []
                    GameSettingTile { required property var modelData; required property int index
                        width: (content.width-13)/2; height: index<2 ? Math.min(132,content.height*.41) : Math.min(88,content.height*.27)
                        compact: index>=2; label: modelData.label; detail: modelData.detail || ""; glyph: modelData.glyph || "settings"
                        tint: index===0?Theme.blue:index===1?Theme.green:index===2?Theme.tabColors[4]:Theme.yellow
                        selected: root.selection===modelData.id; enabled: root.presentation.ready && !modelData.readOnly
                        onActivated: root.presentation.activateAction(modelData.id)
                    }
                }
            }
            Row { visible: root.settingsRoot; anchors.bottom: parent.bottom; width: parent.width; spacing: 13
                Repeater { model: root.settingsRoot ? root.presentation.menuActions.slice(4) : []
                    ActivityRow { required property var modelData; width: (content.width-13)/2; height: 43; label: modelData.label; glyph: modelData.id==="back"?"chevron":"settings"; accent: Theme.paper; selected: root.selection===modelData.id; enabled: root.presentation.ready; onActivated: root.presentation.activateAction(modelData.id) }
                }
            }
            ListView {
                id: actions; anchors.fill: parent; visible: !root.settingsRoot; clip: true; spacing: root.presentation.panel==="players"?2:10
                model: root.settingsRoot ? [] : root.presentation.menuActions; currentIndex: root.presentation.menuFocus
                onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
                delegate: ActivityRow {
                    required property var modelData; required property int index
                    width: actions.width; height: root.presentation.panel==="players"?40:modelData.icon ? 77 : modelData.detail ? 63 : 49
                    label: modelData.label; detail: modelData.detail || ""; glyph: modelData.glyph || (root.presentation.panel==="achievements"?"trophy":"pictures")
                    artwork: modelData.icon || ""; accent: index%3===0?Theme.blue:index%3===1?Theme.green:Theme.yellow
                    selected: root.selection===modelData.id; enabled: root.presentation.ready && (modelData.readOnly!==true || root.presentation.panel==="achievements")
                    removable: !!modelData.removable; onRemoved: root.shell.dismissNotificationById(modelData.id.substring(7))
                    onActivated: root.presentation.activateAction(modelData.id)
                }
            }
        }
    }
}
