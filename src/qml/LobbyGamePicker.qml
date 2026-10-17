import QtQuick

Item {
    id: root
    required property var social
    readonly property var picker: social.lobbyPicker
    visible: picker.open
    objectName: "lobby-game-picker"
    Rectangle { anchors.fill: parent; color: "#80152728" }
    MouseArea { anchors.fill: parent; onClicked: root.social.lobbyPickerAction("cancel"); onWheel: wheel => wheel.accepted=true }
    Panel {
        anchors.centerIn: parent; width: Math.min(690,parent.width-28); height: Math.min(400,parent.height-12)
        MouseArea { anchors.fill: parent; onWheel: wheel => wheel.accepted=true }
        Text { x: 24; y: 18; text: "Choose a game"; font.family: Theme.displayFamily; font.pixelSize: 25; color: Theme.ink }
        Text { x: 24; y: 50; text: "One game, or up to four choices for a vote"; font.pixelSize: 12; color: Theme.muted }
        Text { anchors.right: parent.right; anchors.rightMargin: 20; y: 14; text: "×"; font.pixelSize: 28; color: Theme.muted; MouseArea { anchors.fill: parent; anchors.margins: -4; onClicked: root.social.lobbyPickerAction("cancel") } }
        CapButton { x: 24; y: 78; width: 145; height: 30; label: root.picker.search || "Search…"; textSize: 12; tint: Theme.blue; claimsFocus: false; onActivated: root.social.lobbyPickerAction("search") }
        ListView { x: 24; y: 120; width: 145; height: parent.height-y-83; clip: true; spacing: 4; model: root.social.lobbyPickerCategories; currentIndex: root.picker.rail
            onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Item { required property var modelData; required property int index; width: ListView.view.width; height: 29
                Rectangle { anchors.fill: parent; radius: 7; color: Theme.green; opacity: root.picker.rail===index?.45:.08; border.width: root.picker.railFocused&&root.picker.rail===index?2:0; border.color: Theme.focusGlow }
                Text { x: 9; anchors.verticalCenter: parent.verticalCenter; width: parent.width-18; text: modelData.title || "Untitled game"; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 13 }
                MouseArea { anchors.fill: parent; onClicked: root.social.lobbyPickerAction("category:"+modelData.id) }
            }
        }
        Rectangle { x: 183; y: 80; width: 1; height: parent.height-y-83; color: Theme.rim; opacity: .4 }
        ListView { id: games; x: 198; y: 78; width: parent.width-x-24; height: parent.height-y-92; clip: true; spacing: 2; model: root.social.lobbyPickerRows; currentIndex: root.picker.focus
            onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Item { required property var modelData; required property int index; width: games.width; height: 49
                Rectangle { anchors.fill: parent; radius: 8; color: modelData.selected?Qt.rgba(Theme.yellow.r,Theme.yellow.g,Theme.yellow.b,.24):"transparent"; border.width: !root.picker.railFocused&&root.picker.focus===index?2:0; border.color: Theme.focusGlow }
                Image { x: 7; y: 4; width: 29; height: 40; source: modelData.cover || ""; fillMode: Image.PreserveAspectFit; asynchronous: true }
                Column { x: 46; y: 5; width: parent.width-x-32; spacing: 3
                    Text { width: parent.width; text: modelData.title || "Untitled game"; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: 16; color: Theme.ink; elide: Text.ElideRight }
                    Text { width: parent.width; text: (modelData.platform || "").toUpperCase()+" · "+(modelData.modeLabel || "Multiplayer"); font.pixelSize: 11; color: Theme.muted; elide: Text.ElideRight }
                }
                Text { anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: modelData.selected?"✓":"+"; color: Theme.ink; font.pixelSize: 22 }
                Rectangle { x: 46; anchors.bottom: parent.bottom; width: parent.width-x-10; height: 1; color: Theme.rim; opacity: .28 }
                MouseArea { anchors.fill: parent; onClicked: root.social.lobbyPickerAction("pick:"+modelData.key) }
            }
            Text { anchors.centerIn: parent; width: parent.width-35; text: "No available multiplayer games here yet"; visible: !games.count; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 16 }
        }
        Text { x: 24; y: parent.height-79; width: parent.width-48; text: root.picker.error || root.picker.choices.map(c=>c.title).join(" · ") || "Choosing Together"; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
        Text { x: 24; y: parent.height-44; text: "Clear"; color: Theme.muted; font.pixelSize: 13; MouseArea { anchors.fill: parent; anchors.margins: -5; onClicked: root.social.lobbyPickerAction("clear") } }
        CapButton { anchors.right: parent.right; anchors.rightMargin: 24; y: parent.height-54; width: 182; height: 33; label: root.picker.choices.length>1?"Y · Use for a vote":"Y · Use selection"; textSize: 13; centered: true; claimsFocus: false; tint: Theme.green; onActivated: root.social.lobbyPickerAction("apply") }
    }
}
