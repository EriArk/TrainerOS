import QtQuick

Rectangle {
    id: root
    objectName: "start-dashboard"
    required property var shell
    readonly property var board: shell.controls
    readonly property int selectedRadio: ["wifi","bluetooth","airplane"].indexOf(board.selected)
    readonly property var selectedRadioData: selectedRadio<0 ? ({}) : (shell.device.radios[selectedRadio] || {})
    readonly property string selectedDescription: board.selectedTitle + (selectedRadio<0 ? "" : " · "+(selectedRadioData.level===1?"On":selectedRadioData.level===0?"Off":"Unavailable"))
    readonly property int displayRows: board.editing ? 4 : Math.max(2, ...board.widgets.map(w => w.y+w.h))
    implicitWidth: 760
    implicitHeight: (board.editing ? 178 : 148) + displayRows*52
    radius: 18
    color: Theme.paper
    border.color: Theme.divider
    CrtTexture { anchors.fill: parent; anchors.margins: 2 }

    component ActionButton: Rectangle {
        property string label
        property string glyph: ""
        property bool selected: false
        signal activated()
        implicitWidth: glyph.length ? 58 : 80
        implicitHeight: 38
        radius: 8
        color: selected ? Theme.selection : "transparent"
        border.width: selected ? 1 : 0
        border.color: Theme.focusGlow
        opacity: enabled ? 1 : .35
        MenuGlyph { anchors.horizontalCenter: parent.horizontalCenter; y: 0; width: 21; height: 21; glyph: parent.glyph; visible: parent.glyph.length>0 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: parent.glyph.length ? 25 : (parent.height-height)/2; text: parent.label; color: Theme.ink; font.pixelSize: parent.glyph.length ? 10 : 13 }
        MouseArea { anchors.fill: parent; onClicked: parent.activated() }
        Accessible.role: Accessible.Button
        Accessible.name: label
        Accessible.onPressAction: activated()
    }
    Text { x: 24; y: 21; text: "Start"; font.family: Theme.displayFamily; font.pixelSize: 25; color: Theme.ink }
    ActionButton { objectName: "start-edit"; x: 112; y: 18; label: root.board.editing ? "Done" : "Edit widgets"; width: 96; selected: root.board.selected==="edit"; onActivated: root.board.edit() }
    Row { anchors.right: closeButton.left; anchors.rightMargin: 14; y: 17; spacing: 8; visible: !root.board.editing
        Repeater { model: [{id:"settings",title:"Settings",icon:"settings"},{id:"power",title:"Power",icon:"power"},{id:"switch",title:"Switch",icon:"desktop"}]
            ActionButton { required property var modelData; label: modelData.title; glyph: modelData.icon; selected: root.board.selected===modelData.id; onActivated: root.board.activate(modelData.id) }
        }
    }
    SocialIconButton { id: closeButton; anchors.right: parent.right; anchors.rightMargin: 18; y: 20; icon: "x"; label: "Close Start"; onClicked: root.shell.closeControls() }
    Rectangle { x: 24; y: 65; width: parent.width-48; height: 1; color: Theme.divider }
    Item {
        id: grid; x: 24; y: 80; width: parent.width-48; height: parent.height-y-(root.board.editing ? 98 : 68)
        readonly property real cellW: (width+8)/12
        readonly property real cellH: (height+8)/root.displayRows
        // Empty space also accepts a horizontal swipe. Widget drags own their input.
        MouseArea { anchors.fill: parent; property real startX
            onPressed: mouse => startX=mouse.x
            onReleased: mouse => { if(Math.abs(mouse.x-startX)>50)root.board.changePage(mouse.x<startX?1:-1) }
        }
        Repeater { model: root.board.editing ? 48 : 0
            Rectangle { required property int index; x: (index%12)*grid.cellW+grid.cellW/2-1; y: Math.floor(index/12)*grid.cellH+grid.cellH/2-1; width: 2; height: 2; radius: 1; color: Theme.divider
                visible: !root.board.widgets.some(w => index%12>=w.x && index%12<w.x+w.w && Math.floor(index/12)>=w.y && Math.floor(index/12)<w.y+w.h)
            }
        }
        Repeater { model: root.board.widgets
            ControlWidget { shell: root.shell
                x: modelData.x*grid.cellW; y: modelData.y*grid.cellH
                width: modelData.w*grid.cellW-8; height: modelData.h*grid.cellH-8
                onMoved: (dx,dy) => root.board.drop(modelData.id,Math.round(modelData.x+dx/grid.cellW),Math.round(modelData.y+dy/grid.cellH))
                onSwiped: delta => root.board.changePage(delta)
            }
        }
        Text { anchors.centerIn: parent; visible: root.board.widgets.length===0; text: "Your space for quick controls\nEdit widgets to add something here"; horizontalAlignment: Text.AlignHCenter; lineHeight: 1.6; color: Theme.muted; font.pixelSize: 15 }
    }
    Text { x: 24; y: grid.y+grid.height+13; width: parent.width-48; color: root.board.status.length || root.shell.device.error.length ? Theme.ink : Theme.muted; font.pixelSize: 12; elide: Text.ElideRight
        text: root.board.status || root.shell.device.error || (root.board.moving ? "Moving " + root.board.selectedTitle + " · choose a space or another page" : root.selectedDescription) }
    Row { x: 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; spacing: 4; visible: root.board.editing
        ActionButton { objectName: "start-add"; label: "+ Add"; enabled: !root.board.moving; onActivated: root.board.pick() }
        ActionButton { objectName: "start-size"; label: Theme.buttonLabel("X")+" Size"; enabled: root.board.widgetSelected&&!root.board.moving; onActivated: root.board.resize() }
        ActionButton { objectName: "start-hide"; label: Theme.buttonLabel("Y")+" Hide"; enabled: root.board.widgetSelected&&!root.board.moving; onActivated: root.board.hide() }
        ActionButton { objectName: "start-move"; label: root.board.moving ? "Place" : "Move"; enabled: root.board.widgetSelected; onActivated: root.board.activate(root.board.selected) }
        ActionButton { objectName: "start-cancel"; label: "Cancel move"; width: 94; visible: root.board.moving; onActivated: root.board.back() }
    }
    ActionButton { objectName: "start-audio-output"; x: parent.width-340; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; width: 140
        visible: !root.board.editing && root.board.selected==="volume"
        label: Theme.buttonLabel("Y")+" Audio output"; onActivated: root.board.audioOutputs()
    }
    Text { x: 24; anchors.bottom: parent.bottom; anchors.bottomMargin: root.board.editing ? 54 : 16; width: parent.width-(!root.board.editing&&root.board.selected==="volume"?370:230); elide: Text.ElideRight; color: Theme.muted; font.pixelSize: 11
        text: root.board.moving ? Theme.buttonLabel("A")+" Place · "+Theme.buttonLabel("B")+" Cancel" : root.board.editing ? Theme.buttonLabel("A")+" Move · "+Theme.buttonLabel("L2")+" Add · "+Theme.buttonLabel("B")+" Done" : root.board.adjusting ? "← → / ↑ ↓ Adjust · "+(root.board.selected==="volume"?Theme.buttonLabel("X")+" Mute · ":"")+Theme.buttonLabel("B")+" Done" : Theme.buttonLabel("A")+" Select · "+Theme.buttonLabel("B")+" Close" }
    Row { anchors.right: parent.right; anchors.rightMargin: 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; spacing: 4; visible: root.board.pages>1 || root.board.editing
        ActionButton { objectName: "start-previous-page"; width: 46; label: "‹ "+Theme.buttonLabel("L1"); enabled: root.board.page>0; onActivated: root.board.changePage(-1) }
        Text { width: 43; y: 12; text: (root.board.page+1)+" / "+root.board.pages; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 12 }
        ActionButton { objectName: "start-next-page"; width: 46; label: Theme.buttonLabel("R1")+" ›"; enabled: root.board.page+1<root.board.pages || root.board.moving&&root.board.page<7; onActivated: root.board.changePage(1) }
    }
    Rectangle { anchors.fill: parent; z: 10; radius: root.radius; color: Qt.alpha("black",.55); visible: root.board.picking
        MouseArea { anchors.fill: parent; onClicked: root.board.back() }
        Rectangle { anchors.centerIn: parent; width: Math.min(420,root.width-40); height: Math.min(root.height-24,100+Math.max(1,root.board.available.length)*44); radius: 14; color: Theme.paper; border.color: Theme.divider
            MouseArea { anchors.fill: parent }
            Text { x: 24; y: 20; text: "Add a widget"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 23 }
            SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 16; y: 17; icon: "x"; label: "Close"; onClicked: root.board.back() }
            Text { anchors.centerIn: parent; visible: !root.board.available.length; text: "All available widgets are on your pages"; font.pixelSize: 13; color: Theme.muted }
            ListView { currentIndex: root.board.available.findIndex(w => w.selected); onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain); x: 18; y: 64; width: parent.width-36; height: parent.height-y-16; clip: true; spacing: 4; model: root.board.available
                delegate: Rectangle { required property var modelData; width: ListView.view.width; height: 40; radius: 7; color: modelData.selected ? Theme.selection : "transparent"; border.color: modelData.selected ? Theme.focusGlow : "transparent"
                    Text { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; width: parent.width-24; text: modelData.title; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 15 }
                    MouseArea { anchors.fill: parent; onClicked: root.board.restore(modelData.id) }
                }
            }
        }
    }
    Rectangle { anchors.fill: parent; z: 10; radius: root.radius; color: Qt.alpha("black",.55); visible: root.board.switching
        MouseArea { anchors.fill: parent; onClicked: root.board.back() }
        Rectangle { anchors.centerIn: parent; width: Math.min(390,root.width-32); height: 220; radius: 14; color: Theme.paper; border.color: Theme.divider
            MouseArea { anchors.fill: parent }
            Text { x: 25; y: 23; text: "Switch environment"; font.family: Theme.displayFamily; font.pixelSize: 24; color: Theme.ink }
            SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 17; y: 17; icon: "x"; label: "Cancel"; onClicked: root.board.back() }
            ActivityRow { x: 24; y: 76; width: parent.width-48; label: "Desktop"; detail: "Maintenance and desktop apps"; glyph: "desktop"; selected: root.board.switchIndex===0; onActivated: root.board.activate("desktop") }
            ActivityRow { x: 24; y: 140; width: parent.width-48; label: "Steam Gaming Mode"; glyph: "game-controller"; selected: root.board.switchIndex===1; onActivated: root.board.activate("steam") }
        }
    }

    Rectangle { anchors.fill: parent; z: 20; radius: root.radius; color: Qt.alpha("black",.55); visible: root.shell.device.audioOpen
        MouseArea { anchors.fill: parent; onClicked: root.shell.device.closeAudio() }
        Rectangle { objectName: "start-audio-popup"; anchors.centerIn: parent; width: Math.min(490,root.width-32); height: Math.min(330,root.parent.height-24); radius: 18; color: Theme.paper; border.color: Theme.divider
            MouseArea { anchors.fill: parent }
            Text { x: 22; y: 18; text: "Audio output"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 22 }
            SocialIconButton { objectName: "start-audio-close"; anchors.right: parent.right; anchors.rightMargin: 15; y: 13; icon: "x"; label: "Close audio outputs"; onClicked: root.shell.device.closeAudio() }
            Text { x: 22; y: 53; text: "Choose where system audio plays"; color: Theme.muted; font.pixelSize: 13 }
            ListView { id: outputs; x: 22; y: 83; width: parent.width-44; height: parent.height-y-58; clip: true; spacing: 4
                model: root.shell.device.audioOutputs; currentIndex: root.shell.device.audioFocus
                onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                onCountChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                delegate: ActivityRow { required property var modelData; required property int index
                    objectName: "audio-output-"+index; width: outputs.width; height: 54; glyph: "speaker"
                    label: modelData.title; detail: !modelData.available ? "Not connected" : modelData.id===root.shell.device.audioOutput ? "Current output" : ""
                    selected: index===root.shell.device.audioFocus; accent: modelData.id===root.shell.device.audioOutput ? Theme.green : Theme.paper
                    enabled: modelData.available && !root.shell.device.busy
                    onActivated: root.shell.device.selectOutput(modelData.id)
                }
                Text { anchors.centerIn: parent; width: parent.width-24; visible: !outputs.count; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
                    text: root.shell.device.busy ? "Looking for audio devices…" : "No audio outputs available.\nConnect headphones or check your audio device."; color: Theme.muted; font.pixelSize: 15 }
            }
            Text { x: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 14; width: parent.width-44; height: 30; wrapMode: Text.Wrap; elide: Text.ElideRight; textFormat: Text.PlainText
                text: root.shell.device.error || (root.shell.device.busy ? "Updating audio…" : Theme.buttonLabel("A")+" Choose · "+Theme.buttonLabel("B")+" Back"); color: Theme.muted; font.pixelSize: 12 }
        }
    }

    Rectangle { anchors.fill: parent; z: 20; radius: root.radius; color: Theme.surfaceColor("#68182b29"); visible: root.shell.controlsNetworkOpen
        MouseArea { anchors.fill: parent; onClicked: root.shell.closeControlsNetwork() }
        Rectangle { objectName: "start-network-popup"; anchors.centerIn: parent; width: Math.min(490,root.width-32); height: Math.min(Math.max(380,root.height-16),root.parent.height-24); radius: 18; color: Theme.paper; border.color: Theme.divider
            MouseArea { anchors.fill: parent }
            Text { x: 22; y: 18; text: root.shell.network.bluetooth ? "Bluetooth" : "Wi-Fi"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 22 }
            SocialIconButton { objectName: "start-network-close"; anchors.right: parent.right; anchors.rightMargin: 15; y: 13; icon: "x"; label: "Close connections"; onClicked: root.shell.closeControlsNetwork() }
            ConnectionsPane { x: 22; y: 47; width: parent.width-44; height: parent.height-y-12; shell: root.shell; quick: true; flat: true }
        }
    }
}
