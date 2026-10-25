import QtQuick

Item {
    id: root
    required property var presentation
    required property var shell
    readonly property string selection: (presentation.menuActions[presentation.menuFocus] || {}).id || ""
    readonly property var gameActions: presentation.menuActions.filter(a => a.column === "game")
    readonly property var otherActions: presentation.menuActions.filter(a => a.column !== "game" && a.id !== "continue" && a.id !== "exit" && a.id !== "minimize")
    Text { x: 26; y: 18; text: "Now playing"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 27 }
    SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 20; y: 17; icon: "x"; label: "Continue"; onClicked: root.presentation.activateAction("continue") }
    Item { id: game; x: 26; y: 65; width: (parent.width-70)*.55; height: parent.height-y-23
        GameFrame { id: frame; presentation: root.presentation; maximumWidth: game.width*.64; maximumHeight: 154 }
        Column { id: identity; x: frame.width+15; y: 8; width: game.width-x; spacing: 10
            Text { width: parent.width; text: "Playing"; color: Theme.muted; font.pixelSize: 12 }
            Text { width: parent.width; text: root.presentation.gameTitle; font.family: Theme.displayFamily; font.pixelSize: 23; color: Theme.ink; wrapMode: Text.WordWrap; maximumLineCount: 4; elide: Text.ElideRight; textFormat: Text.PlainText }
        }
        Row { id: controls; y: frame.height+17; width: parent.width; spacing: 12
            GameSettingTile { width: (parent.width-12)*.57; height: 51; label: "Continue"; compact: true; glyph: "play"; tint: Theme.green; selected: root.selection==="continue"; enabled: root.presentation.ready; onActivated: root.presentation.activateAction("continue") }
            GameSettingTile { width: (parent.width-12)*.43; height: 51; label: "Minimize"; compact: true; glyph: "desktop"; tint: Theme.blue; selected: root.selection==="minimize"; enabled: root.presentation.ready && root.presentation.menuActions.some(a => a.id==="minimize" && !a.readOnly); onActivated: root.presentation.activateAction("minimize") }
        }
        ListView { id: options; y: controls.y+controls.height+16; width: parent.width; height: Math.max(45,quit.y-y-7); clip: true; spacing: 2; model: root.gameActions
            currentIndex: root.gameActions.findIndex(a => a.id===root.selection)
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: ActivityRow { required property var modelData; width: options.width; height: 48; label: modelData.label; glyph: modelData.id==="achievements"?"trophy":"settings"; accent: modelData.id==="achievements"?Theme.yellow:Theme.blue; selected: root.selection===modelData.id; enabled: root.presentation.ready && !modelData.readOnly; onActivated: root.presentation.activateAction(modelData.id) }
        }
        ActivityRow { id: quit; anchors.bottom: parent.bottom; width: Math.min(190,parent.width); height: 39; label: "Exit game"; glyph: "close"; accent: Theme.pink; selected: root.selection==="exit"; enabled: root.presentation.ready; onActivated: root.presentation.activateAction("exit") }
    }
    Rectangle { x: game.x+game.width+13; y: game.y; width: 1; height: game.height; color: Theme.surfaceColor("#3558786a") }
    Item { id: updates; x: game.x+game.width+27; y: game.y; width: parent.width-x-26; height: game.height
        Text { id: heading; width: parent.width; text: root.shell.social.homeLobby.title || "Your updates"; font.family: Theme.displayFamily; font.pixelSize: 23; elide: Text.ElideRight; color: Theme.ink; textFormat: Text.PlainText }
        Row { id: people; y: heading.height+12; height: visible ? 80 : 0; spacing: 9; visible: playerOverlay.players.length>0
            Repeater { model: playerOverlay.players.slice(0,5); LobbyAvatar { required property var modelData; width: 65; height: 80; small: true; person: modelData } }
        }
        ListView { id: actions; y: people.visible ? people.y+people.height+10 : heading.height+14; width: parent.width; height: parent.height-y; clip: true; spacing: 5; model: root.otherActions
            currentIndex: root.otherActions.findIndex(a => a.id===root.selection)
            onCurrentIndexChanged: if(currentIndex>=0)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: ActivityRow { required property var modelData; width: actions.width; label: modelData.label; detail: modelData.detail || ""; avatar: modelData.avatar || ""; person: !!modelData.person; selected: root.selection===modelData.id
                glyph: modelData.id.indexOf("voice")>=0||modelData.id==="call"?"speaker":modelData.id==="controls"?"settings":"chat"; accent: modelData.id==="controls"?Theme.green:Theme.blue
                removable: !!modelData.removable; onRemoved: root.shell.dismissNotificationById(modelData.id.substring(7))
                enabled: root.presentation.ready && !modelData.readOnly; onActivated: root.presentation.activateAction(modelData.id)
            }
        }
    }
}
