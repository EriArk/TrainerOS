import QtQuick

Item {
    id: root
    required property var social
    readonly property var picker: social.lobbyPicker
    visible: picker.open
    objectName: "lobby-game-picker"
    function modeName(mode) {
        if (mode.modeLabel) return mode.modeLabel
        if ((mode.mode || "").indexOf("remoteplay") >= 0) return "Remote Play"
        if ((mode.mode || "").indexOf("lan") >= 0) return "LAN · in-game"
        return "NetPlay"
    }
    Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#90152728") }
    MouseArea { anchors.fill: parent; onClicked: root.social.lobbyPickerAction(root.picker.modeOpen?"mode-cancel":"cancel"); onWheel: wheel => wheel.accepted=true }
    Panel {
        id: frame
        anchors.centerIn: parent; width: Math.min(720,parent.width-28); height: Math.min(440,parent.height-12)
        MouseArea { anchors.fill: parent; onWheel: wheel => wheel.accepted=true }
        Text { x: 24; y: 18; text: "Choose a game"; font.family: Theme.displayFamily; font.pixelSize: 25; color: Theme.ink }
        Text { x: 24; y: 49; text: "Choose how to play · up to four proposals"; font.pixelSize: 12; color: Theme.muted }
        Text { anchors.right: parent.right; anchors.rightMargin: 20; y: 14; text: "×"; font.pixelSize: 28; color: Theme.muted; MouseArea { anchors.fill: parent; anchors.margins: -4; onClicked: root.social.lobbyPickerAction("cancel") } }
        CapButton { x: 24; y: 78; width: 145; height: 30; label: root.picker.search || "Search…"; textSize: 12; tint: Theme.blue; claimsFocus: false; onActivated: root.social.lobbyPickerAction("search") }
        ListView {
            x: 24; y: 120; width: 145; height: parent.height-y-68; clip: true; spacing: 4
            model: root.social.lobbyPickerCategories; currentIndex: root.picker.rail
            onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Item {
                required property var modelData; required property int index
                width: ListView.view.width; height: 29
                Rectangle { anchors.fill: parent; radius: 7; color: Theme.green; opacity: root.picker.rail===index?.45:.08; border.width: root.picker.railFocused&&root.picker.rail===index?2:0; border.color: Theme.focusGlow }
                Text { x: 9; anchors.verticalCenter: parent.verticalCenter; width: parent.width-18; text: modelData.title; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 13 }
                MouseArea { anchors.fill: parent; onClicked: root.social.lobbyPickerAction("category:"+modelData.id) }
            }
        }
        Rectangle { x: 183; y: 78; width: 1; height: parent.height-y-68; color: Theme.rim; opacity: .4 }
        Text { x: 202; y: 75; text: "Selected / Proposed"; color: Theme.muted; font.pixelSize: 11 }
        Flickable {
            id: pinned
            x: 198; y: 94; width: parent.width-x-24; height: 58
            contentWidth: pinRow.width; contentHeight: height; clip: true; boundsBehavior: Flickable.StopAtBounds
            Row {
                id: pinRow; spacing: 7
                Repeater {
                    model: root.picker.pins
                    delegate: Rectangle {
                        required property var modelData; required property int index
                        width: 154; height: 54; radius: 10; color: Qt.rgba(Theme.green.r,Theme.green.g,Theme.green.b,.2)
                        border.width: root.picker.pinsFocused&&root.picker.pinFocus===index?2:1
                        border.color: root.picker.pinsFocused&&root.picker.pinFocus===index?Theme.focusGlow:Qt.rgba(Theme.rim.r,Theme.rim.g,Theme.rim.b,.35)
                        Text { x: 8; y: 5; width: parent.width-16; text: modelData.title; elide: Text.ElideRight; font.family: Theme.displayFamily; font.pixelSize: 13; color: Theme.ink }
                        Flow {
                            x: 8; y: 25; width: parent.width-16; spacing: 3
                            Repeater {
                                model: modelData.modes
                                delegate: Text {
                                    required property var modelData
                                    text: (modelData.current?"• ":"")+root.modeName(modelData)
                                    font.pixelSize: 9; color: modelData.proposed?Theme.ink:Theme.muted
                                }
                            }
                        }
                        MouseArea { anchors.fill: parent; onClicked: root.social.lobbyPickerAction("game:"+modelData.gameKey) }
                    }
                }
            }
            Text { x: 5; y: 13; visible: !root.picker.pins.length; text: "Pick a game below to get started"; color: Theme.muted; font.pixelSize: 12 }
        }
        Rectangle { x: 202; y: 159; width: parent.width-x-28; height: 1; color: Theme.rim; opacity: .25 }
        ListView {
            id: games; objectName: "lobby-picker-games"
            x: 198; y: 166; width: parent.width-x-24; height: parent.height-y-71
            clip: true; spacing: 2; boundsBehavior: Flickable.StopAtBounds
            model: root.social.lobbyPickerModel
            // Only deliberate navigation scrolls to focus. A heartbeat never does.
            property string anchorKey: ""
            property real anchorOffset: 0
            property int anchorIndex: 0
            function captureAnchor() {
                // The leading pixel may lie in the inter-row spacing. Query the
                // first actual delegate rather than treating a gap as row zero.
                let i = indexAt(1, contentY+1)
                if (i < 0) i = indexAt(1, contentY+spacing+2)
                if (i < 0) return
                anchorIndex = i
                anchorKey = root.social.lobbyPickerModel.get(anchorIndex).gameKey || ""
                const item = itemAtIndex(anchorIndex)
                if (item) anchorOffset = contentY-item.y
            }
            function restoreAnchor() {
                if (!count) return
                forceLayout()
                let i = root.social.lobbyPickerModel.indexOf(anchorKey)
                if (i<0) i = Math.min(anchorIndex,count-1)
                // ListView's origin can shift after moves/inserts; index*height
                // is not a delegate coordinate. Materialize the anchor and use
                // its real position after layout, preserving the pixel offset.
                positionViewAtIndex(i,ListView.Beginning)
                forceLayout()
                const item = itemAtIndex(i)
                if (item) contentY = Math.max(originY,Math.min(originY+Math.max(0,contentHeight-height),item.y+anchorOffset))
            }
            onMovementEnded: {
                let i = indexAt(1,contentY+Math.min(height/2,24))
                if (i>=0) root.social.lobbyPickerAction("focus:"+root.social.lobbyPickerModel.get(i).gameKey)
            }
            Connections {
                target: root.social.lobbyPickerModel
                function onAboutToRefresh() { games.captureAnchor() }
                function onRefreshed() { Qt.callLater(games.restoreAnchor) }
            }
            Connections {
                target: root.social
                function onLobbyPickerNavigate(index) { if(root.visible) games.positionViewAtIndex(index,ListView.Contain) }
            }
            delegate: Item {
                required property var modelData; required property int index
                objectName: "lobby-picker-game-"+modelData.gameKey
                width: games.width; height: 49
                Rectangle { anchors.fill: parent; radius: 8; color: modelData.selected?Qt.rgba(Theme.yellow.r,Theme.yellow.g,Theme.yellow.b,.18):"transparent"; border.width: !root.picker.railFocused&&!root.picker.pinsFocused&&root.picker.gameKey===modelData.gameKey?2:0; border.color: Theme.focusGlow }
                Image { x: 7; y: 4; width: 29; height: 40; source: modelData.cover || ""; fillMode: Image.PreserveAspectFit; asynchronous: true }
                Column {
                    x: 46; y: 5; width: parent.width-x-32; spacing: 3
                    Text { width: parent.width; text: modelData.title || "Untitled game"; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: 16; color: Theme.ink; elide: Text.ElideRight }
                    Text { width: parent.width; text: (modelData.platform || "").toUpperCase(); font.pixelSize: 11; color: Theme.muted; elide: Text.ElideRight }
                }
                Text { anchors.right: parent.right; anchors.rightMargin: 9; anchors.verticalCenter: parent.verticalCenter; text: modelData.selected?"✓":"›"; color: Theme.ink; font.pixelSize: 22 }
                Rectangle { x: 46; anchors.bottom: parent.bottom; width: parent.width-x-10; height: 1; color: Theme.rim; opacity: .28 }
                MouseArea { anchors.fill: parent; onClicked: { root.social.lobbyPickerAction("focus:"+modelData.gameKey); root.social.lobbyPickerAction("game:"+modelData.gameKey) } }
            }
            Text { anchors.centerIn: parent; width: parent.width-35; text: "No available games in this category"; visible: !games.count; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 16 }
        }
        Text { x: 24; y: parent.height-65; width: parent.width-48; text: root.picker.error || ""; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
        Text { x: 24; y: parent.height-39; text: "Clear selection"; color: Theme.muted; font.pixelSize: 13; MouseArea { anchors.fill: parent; anchors.margins: -5; onClicked: root.social.lobbyPickerAction("clear") } }
        CapButton { anchors.right: parent.right; anchors.rightMargin: 24; y: parent.height-48; width: 182; height: 33; label: root.picker.choices.length>1?"Y · Use for a vote":"Y · Use selection"; textSize: 13; centered: true; claimsFocus: false; tint: Theme.green; onActivated: root.social.lobbyPickerAction("apply") }
        Item {
            anchors.fill: parent; visible: root.picker.modeOpen
            Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#30152728"); radius: 16 }
            MouseArea { anchors.fill: parent; onClicked: root.social.lobbyPickerAction("mode-cancel"); onWheel: wheel=>wheel.accepted=true }
            Panel {
                x: Math.min(games.x+24,parent.width-width-22)
                y: {
                    const row = games.itemAtIndex(root.picker.focus)
                    return Math.max(80,Math.min(frame.height-height-24,games.y+(row?row.y-games.contentY:0)))
                }
                width: 302; height: Math.min(frame.height-100,94+root.picker.modes.length*49)
                MouseArea { anchors.fill: parent; onWheel: wheel=>wheel.accepted=true }
                Text { x: 18; y: 12; text: "How to play?"; font.family: Theme.displayFamily; font.pixelSize: 23; color: Theme.ink }
                Text { x: 18; y: 43; width: parent.width-36; text: root.picker.modeTitle; elide: Text.ElideRight; font.pixelSize: 12; color: Theme.muted }
                Text { anchors.right: parent.right; anchors.rightMargin: 14; y: 9; text: "×"; font.pixelSize: 23; color: Theme.muted; MouseArea { anchors.fill: parent; anchors.margins: -5; onClicked: root.social.lobbyPickerAction("mode-cancel") } }
                ListView {
                    x: 12; y: 66; width: parent.width-24; height: parent.height-y-12; clip: true
                    model: root.picker.modes; currentIndex: root.picker.modeFocus
                    onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                    delegate: Item {
                        required property var modelData; required property int index
                        objectName: "lobby-picker-mode-"+index
                        width: ListView.view.width; height: 49
                        Rectangle { anchors.fill: parent; anchors.margins: 2; radius: 9; color: index===root.picker.modeFocus?Qt.rgba(Theme.green.r,Theme.green.g,Theme.green.b,.35):"transparent"; border.color: Theme.focusGlow; border.width: index===root.picker.modeFocus?1:0 }
                        Text { x: 10; y: 6; text: (modelData.selected?"✓ ":"")+root.modeName(modelData); font.family: Theme.displayFamily; font.pixelSize: 17; color: Theme.ink; opacity: modelData.enabled===false?.5:1 }
                        Text { x: 10; y: 28; width: parent.width-20; text: modelData.unavailable || modelData.modeDetail || ((modelData.mode || "").indexOf("remoteplay")>=0?"Host screen + guest controller":(modelData.mode || "").indexOf("lan")>=0?"Connect using the game's own menus":"Compatible game and runtime on each device"); elide: Text.ElideRight; font.pixelSize: 10; color: Theme.muted }
                        MouseArea { anchors.fill: parent; enabled: modelData.enabled!==false; onClicked: root.social.lobbyPickerAction("mode:"+modelData.key) }
                    }
                }
            }
        }
    }
    Connections {
        target: root.social
        function onChanged() {
            if(root.picker.pinsFocused)
                pinned.contentX=Math.max(0,Math.min(pinned.contentWidth-pinned.width,root.picker.pinFocus*161))
        }
    }
}
