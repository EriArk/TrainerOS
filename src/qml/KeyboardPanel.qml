import QtQuick
import QtQuick.Shapes

Item {
    id: root
    required property var shell
    signal backRequested()
    onBackRequested: shell.pressButton("B")
    readonly property var keyboard: shell.keyboard
    property bool takesFocus: keyboard.open && !shell.menuOpen && shell.notice.length === 0
    readonly property real occupiedHeight: tray.height * tray.scale
    visible: keyboard.open
    Rectangle {
        visible: !root.keyboard.live
        anchors.fill: parent
        anchors.topMargin: -Theme.screenBounds.y
        anchors.leftMargin: -Theme.screenBounds.x
        anchors.rightMargin: -Theme.screenBounds.x
        color: Theme.surfaceColor("#65102927")
    }
    MouseArea { anchors.fill: parent; visible: !root.keyboard.live }
    Item {
        id: tray
        width: Math.max(960,parent.width)
        height: (root.keyboard.functionRow ? 274 : 241) + (root.keyboard.live ? 0 : 64)
        anchors.bottom: parent.bottom
        scale: Math.min(1,parent.width/960)
        transformOrigin: Item.BottomLeft
        MouseArea { anchors.fill: parent }
        // The tray remains joined to the lower frame throughout its short motion.
        property real reveal: root.visible ? 1 : 0
        transform: Translate { y: (1 - tray.reveal) * 38 }
        Behavior on reveal { NumberAnimation { duration: Theme.motion(130); easing.type: Easing.OutCubic } }
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: Theme.rim; strokeWidth: 2; fillColor: Theme.chassis
                startX: 0; startY: tray.height
                PathLine { x: 0; y: 12 }
                PathQuad { x: 12; y: 0; controlX: 0; controlY: 0 }
                PathLine { x: tray.width - 39; y: 0 }
                PathQuad { x: tray.width - 32; y: 3; controlX: tray.width - 36; controlY: 0 }
                PathLine { x: tray.width - 3; y: 29 }
                PathQuad { x: tray.width; y: 36; controlX: tray.width; controlY: 32 }
                PathLine { x: tray.width; y: tray.height }
            }
        }
        Text {
            x: 22; y: 12; width: 150; elide: Text.ElideRight; text: root.keyboard.title.toUpperCase()
            color: Theme.textColor("#e5eee1"); font.pixelSize: 13; font.bold: true; font.letterSpacing: 1
        }
        Text {
            anchors { right: parent.right; rightMargin: 115 }
            y: 12
            visible: !root.keyboard.live
            text: root.keyboard.count + " / " + root.keyboard.maximumLength
            color: Theme.textColor("#e5eee1"); font.pixelSize: 13
        }
        CapButton {
            claimsFocus: false;
            x: 180; y: 6; width: 144; height: 25
            label: "L2   " + (root.keyboard.language === "ru" ? "РУС · EN" : "EN · РУС") + "   R2"
            textSize: 12; centered: true; tint: Theme.blue
            onActivated: root.keyboard.cycleLanguage()
        }
        CapButton {
            claimsFocus: false;
            objectName: "keyboard-back"
            anchors.right: parent.right; anchors.rightMargin: 16; y: 6; width: 82; height: 25
            label: root.keyboard.live ? "Hide" : "Back"; textSize: 12; centered: true; tint: Theme.pink
            onActivated: root.backRequested()
        }
        Row {
            x: 334; y: 6; spacing: 8
            CapButton { claimsFocus: false; width: 34; height: 25; label: "←"; textSize: 16; centered: true; onActivated: root.keyboard.moveCursor(-1) }
            CapButton { claimsFocus: false; width: 34; height: 25; label: "→"; textSize: 16; centered: true; onActivated: root.keyboard.moveCursor(1) }
        }
        Panel {
            visible: !root.keyboard.live
            x: 12; y: 39; width: parent.width - 24; height: 62
            TextInput {
                id: editor; objectName: "keyboard-text"
                anchors { left: parent.left; right: parent.right; margins: 21; verticalCenter: parent.verticalCenter }
                property bool syncing: false
                function sync() {
                    syncing=true
                    text=root.keyboard.editableText
                    select(root.keyboard.anchor,root.keyboard.cursor)
                    syncing=false
                }
                readOnly: true; selectByMouse: true; clip: true
                echoMode: root.keyboard.secret ? TextInput.Password : TextInput.Normal
                cursorVisible: root.takesFocus && !root.keyboard.live
                color: Theme.ink; selectionColor: Theme.blue; selectedTextColor: Theme.ink
                font.pixelSize: 24
                onCursorPositionChanged: if(!syncing)root.keyboard.select(selectionStart===cursorPosition?selectionEnd:selectionStart,cursorPosition)
                Component.onCompleted: sync()
                Connections { target: root.keyboard; function onChanged(){editor.sync()} }
                MouseArea {
                    anchors.fill: parent; property int start: 0
                    onPressed: function(mouse){start=editor.positionAt(mouse.x,mouse.y);root.keyboard.select(start,start)}
                    onPositionChanged: function(mouse){if(pressed)root.keyboard.select(start,editor.positionAt(mouse.x,mouse.y))}
                    onDoubleClicked: {root.keyboard.select(0,root.keyboard.editableText.length)}
                }
            }
        }
        Repeater {
            model: root.keyboard.keys
            delegate: Item {
                id: keyItem
                required property int index
                required property var modelData
                objectName: "key-" + modelData.id
                visible: modelData.visible
                readonly property real pitch: (tray.width-40)/root.keyboard.layoutUnits
                readonly property bool toolbar: modelData.row < 0
                x: toolbar ? (modelData.id==="layout" ? 430 : modelData.id==="paste" ? 510 : 590) : 20 + modelData.column * pitch
                y: toolbar ? 6 : (root.keyboard.live ? 0 : 64) + (modelData.row===0 ? 43 : (root.keyboard.functionRow ? 76 : 43) + (modelData.row-1)*36)
                width: toolbar ? 72 : modelData.span * pitch - 4
                height: toolbar ? 25 : modelData.row===0 ? 24 : modelData.height*36-6
                readonly property bool selected: root.takesFocus && !root.keyboard.optionsOpen && root.keyboard.focusIndex === index
                enabled: root.takesFocus
                Accessible.role: Accessible.Button
                Accessible.name: modelData.label
                Rectangle {
                    anchors.fill: parent; radius: 4
                    color: keyItem.toolbar ? "transparent" : keyPointer.pressed ? Qt.darker(Theme.paper,1.12) : Theme.paper
                    border.width: 1; border.color: keyItem.selected ? Theme.focus : keyItem.toolbar ? "transparent" : Qt.alpha(Theme.ink,.22)
                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        color: keyItem.toolbar ? Theme.textColor("#e5eee1") : Theme.ink
                        font.pixelSize: keyItem.toolbar ? 12 : modelData.letter ? 17 : text.length>3 ? 10 : 13
                    }
                    Rectangle { visible: modelData.latched; width: 12; height: 2; radius: 1; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 3; color: Theme.focus }
                }
                MouseArea {
                    id: keyPointer
                    anchors.fill: parent
                    property bool repeated: false
                    property bool moved: false
                    property real lastX: 0
                    onPressed: function(mouse){repeated=false;moved=false;lastX=mouse.x;if(modelData.id==="delete")repeatDelay.start()}
                    onReleased: {repeatDelay.stop();repeatDelete.stop()}
                    onCanceled: {repeatDelay.stop();repeatDelete.stop()}
                    onClicked: if(!repeated&&!moved)root.keyboard.activate(index)
                    // Repeated letters stay two taps; the controller times Caps Lock.
                    onDoubleClicked: function(mouse){mouse.accepted=false}
                    onPositionChanged: function(mouse){
                        if(pressed&&modelData.id==="space"&&Math.abs(mouse.x-lastX)>16){
                            moved=true;root.keyboard.moveCursor(mouse.x>lastX?1:-1);lastX=mouse.x
                        }
                    }
                    Timer {id:repeatDelay;interval:400;onTriggered:{parent.repeated=true;root.keyboard.backspace();repeatDelete.start()}}
                    Timer {id:repeatDelete;interval:65;repeat:true;onTriggered:root.keyboard.backspace()}
                }
            }
        }
        Text {
            x: 25; y: tray.height - 19; width: 865
            text: root.keyboard.hint || root.keyboard.layoutHint
            color: root.keyboard.hint.length ? Theme.textColor("#ffe08c") : Theme.textColor("#d4e4d8")
            font.pixelSize: 11; elide: Text.ElideRight
        }
        MouseArea { anchors.fill: parent; visible: root.keyboard.optionsOpen; onClicked: root.keyboard.toggleOptions() }
        Panel {
            visible: root.keyboard.optionsOpen
            x: 430; y: 34; width: 320; height: 202
            MouseArea { anchors.fill: parent }
            Text { x: 16; y: 12; text: "KEYBOARD LAYOUT"; color: Theme.ink; font.pixelSize: 13; font.bold: true }
            Repeater {
                model: ["number-pad","function-row","navigation-block","system-keys"]
                delegate: CapButton {
                    required property string modelData
                    required property int index
                    objectName: "keyboard-"+modelData
                    x: 12; y: 34+index*32; width: 296; height: 30; flat: true
                    claimsFocus: false; selected: root.keyboard.optionIndex===index
                    readonly property bool groupVisible: [root.keyboard.numberPad,root.keyboard.functionRow,root.keyboard.navigationBlock,root.keyboard.systemKeys][index]
                    label: ["Numpad","Function row · Esc, F1–F12","Navigation + arrows","System · Win, Menu, PrtSc"][index]
                    Accessible.name: label + (groupVisible ? ", On" : ", Off")
                    textSize: 12
                    Text {
                        anchors.right: parent.right; anchors.rightMargin: 13; anchors.verticalCenter: parent.verticalCenter
                        text: parent.groupVisible ? "On" : "Off"; color: Theme.muted; font.pixelSize: 12
                    }
                    onActivated: root.keyboard.toggleGroup(modelData)
                }
            }
            CapButton {
                objectName: "keyboard-layout-back"
                x: 12; y: 169; width: parent.width-24; height: 25; flat: true
                claimsFocus: false; label: "B · Done"; textSize: 13
                onActivated: root.keyboard.toggleOptions()
            }
        }
    }
}
