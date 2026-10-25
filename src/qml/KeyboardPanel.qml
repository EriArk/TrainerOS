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
        height: (root.keyboard.touchLayout ? 270 : root.keyboard.functionRow ? 274 : 241) + (root.keyboard.live ? 0 : 64)
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
            id: keyRepeater
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
                y: toolbar ? 6 : (root.keyboard.live ? 0 : 64) + (root.keyboard.touchLayout ? 46+modelData.row*50 : modelData.row===0 ? 43 : (root.keyboard.functionRow ? 76 : 43) + (modelData.row-1)*36)
                width: toolbar ? 72 : modelData.span * pitch - 4
                height: toolbar ? 25 : root.keyboard.touchLayout ? 43 : modelData.row===0 ? 24 : modelData.height*36-6
                readonly property bool selected: root.takesFocus && !root.keyboard.optionsOpen && root.keyboard.focusIndex === index
                enabled: root.takesFocus
                Accessible.role: Accessible.Button
                Accessible.name: modelData.label
                Rectangle {
                    anchors.fill: parent; radius: root.keyboard.touchLayout ? 7 : 4
                    color: keyItem.toolbar ? "transparent" : keyPointer.pressed || (touchPointer.pressed&&touchPointer.hoverIndex===index) ? Qt.darker(Theme.paper,1.12) : Theme.paper
                    border.width: 1; border.color: keyItem.selected ? Theme.focus : keyItem.toolbar ? "transparent" : Qt.alpha(Theme.ink,.22)
                    Text {
                        anchors.centerIn: parent
                        text: touchPointer.trackpad&&!keyItem.toolbar ? "" : modelData.label
                        color: keyItem.toolbar ? Theme.textColor("#e5eee1") : Theme.ink
                        font.pixelSize: keyItem.toolbar ? 12 : root.keyboard.touchLayout ? (modelData.label.length>3?15:22) : modelData.letter ? 17 : text.length>3 ? 10 : 13
                    }
                    Rectangle { visible: modelData.latched; width: 12; height: 2; radius: 1; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 3; color: Theme.focus }
                }
                MouseArea {
                    id: keyPointer
                    anchors.fill: parent
                    enabled: !root.keyboard.touchLayout || keyItem.toolbar
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
        MouseArea {
            id: touchPointer
            objectName: "keyboard-touch-surface"
            x: 16; y: (root.keyboard.live?0:64)+44; width: tray.width-32; height: 202
            visible: root.keyboard.touchLayout
            enabled: root.takesFocus && !root.keyboard.optionsOpen
            preventStealing: true
            property int hoverIndex: -1
            property string startId: ""
            property real startX: 0
            property real startY: 0
            property real lastX: 0
            property real lastY: 0
            property bool trackpad: false
            property bool repeated: false
            property bool temporarySymbols: false
            property var accents: []
            property int accentIndex: -1
            property real accentLeft: 0
            property real accentTop: 0
            function keyAt(px,py) {
                if(px<0||px>width||py<0||py>height)return -1
                let nearest=-1, distance=1e9
                for(let i=0;i<keyRepeater.count;++i){
                    const key=keyRepeater.itemAt(i)
                    if(!key||!key.visible||key.modelData.row<0)continue
                    const dy=Math.abs(py+y-(key.y+key.height/2))
                    if(dy>26)continue
                    const dx=Math.max(key.x-(px+x),0,(px+x)-(key.x+key.width))
                    if(dx+dy<distance){distance=dx+dy;nearest=i}
                }
                return nearest
            }
            function clearGesture(){hold.stop();erase.stop();hoverIndex=-1;trackpad=false;accents=[];accentIndex=-1}
            onPressed: function(mouse){
                clearGesture();repeated=false;temporarySymbols=false
                startX=lastX=mouse.x;startY=lastY=mouse.y;hoverIndex=keyAt(mouse.x,mouse.y)
                startId=hoverIndex<0?"":keyRepeater.itemAt(hoverIndex).modelData.id
                if(startId.length)hold.start()
            }
            onPositionChanged: function(mouse){
                if(!pressed)return
                if(trackpad){
                    let steps=Math.trunc((mouse.x-lastX)/12)
                    for(let i=0;i<Math.min(20,Math.abs(steps));++i)root.keyboard.moveCursor(steps<0?-1:1)
                    if(steps)lastX+=steps*12
                    if(root.keyboard.live&&Math.abs(mouse.y-lastY)>22){root.keyboard.pressKey(mouse.y<lastY?Qt.Key_Up:Qt.Key_Down);lastY=mouse.y}
                    return
                }
                if(accents.length){accentIndex=Math.max(0,Math.min(accents.length-1,Math.floor((mouse.x+x-accentLeft)/48)));return}
                if(startId==="symbols"&&root.keyboard.symbolPage===0&&Math.hypot(mouse.x-startX,mouse.y-startY)>20){root.keyboard.setSymbolPage(1);temporarySymbols=true}
                const next=keyAt(mouse.x,mouse.y)
                if(next!==hoverIndex){hold.stop();erase.stop();hoverIndex=next}
            }
            onReleased: function(mouse){
                const selected=hoverIndex, alternate=accentIndex>=0?accents[accentIndex]:""
                const cursorMode=trackpad, didRepeat=repeated, from=startId, temporary=temporarySymbols
                clearGesture()
                if(cursorMode||didRepeat)return
                if(alternate.length)root.keyboard.typeAlternative(alternate)
                else if(selected>=0){
                    if(from==="shift"&&keyRepeater.itemAt(selected).modelData.letter)root.keyboard.activateShifted(selected)
                    else root.keyboard.activate(selected)
                }
                if(temporary)root.keyboard.setSymbolPage(0)
            }
            onCanceled: {const temporary=temporarySymbols;clearGesture();if(temporary)root.keyboard.setSymbolPage(0)}
            onDoubleClicked: function(mouse){mouse.accepted=false}
            onEnabledChanged: if(!enabled)clearGesture()
            Timer {
                id: hold; interval: 400
                onTriggered: {
                    if(!touchPointer.pressed||touchPointer.hoverIndex<0)return
                    if(touchPointer.startId==="space"){touchPointer.trackpad=true;return}
                    if(touchPointer.startId==="delete"){touchPointer.repeated=true;root.keyboard.backspace();erase.start();return}
                    const choices=root.keyboard.alternatives(touchPointer.hoverIndex)
                    if(choices.length){
                        const key=keyRepeater.itemAt(touchPointer.hoverIndex)
                        touchPointer.accentLeft=Math.max(16,Math.min(tray.width-16-choices.length*48,key.x))
                        touchPointer.accentTop=key.y-58;touchPointer.accents=choices;touchPointer.accentIndex=0
                    }
                }
            }
            Timer {id:erase;interval:65;repeat:true;onTriggered:root.keyboard.backspace()}
        }
        Rectangle {
            visible: touchPointer.pressed&&!touchPointer.trackpad&&!touchPointer.accents.length&&!root.keyboard.secret&&touchPointer.hoverIndex>=0&&keyRepeater.itemAt(touchPointer.hoverIndex)&&!keyRepeater.itemAt(touchPointer.hoverIndex).modelData.utility&&keyRepeater.itemAt(touchPointer.hoverIndex).modelData.id!=="space"
            readonly property var key: touchPointer.hoverIndex>=0?keyRepeater.itemAt(touchPointer.hoverIndex):null
            x: key?Math.max(16,Math.min(tray.width-width-16,key.x+(key.width-width)/2)):0
            y: key?key.y-51:0;width:72;height:48;radius:9;color:Theme.paper;border.color:Qt.alpha(Theme.ink,.4);z:2
            Text {anchors.centerIn:parent;text:parent.key?parent.key.modelData.label:"";color:Theme.ink;font.pixelSize:28}
        }
        Rectangle {
            visible: touchPointer.accents.length>0;x:touchPointer.accentLeft;y:touchPointer.accentTop
            width:touchPointer.accents.length*48;height:50;radius:7;color:Theme.paper;border.color:Qt.alpha(Theme.ink,.4);z:3
            Row {Repeater {model:touchPointer.accents;delegate:Rectangle {
                required property int index;required property string modelData
                width:48;height:50;radius:7;color:index===touchPointer.accentIndex?Theme.blue:"transparent"
                Text {anchors.centerIn:parent;text:modelData;color:Theme.ink;font.pixelSize:24}
            }}}
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
            x: 430; y: 34; width: 320; height: root.keyboard.touchLayout?106:234
            MouseArea { anchors.fill: parent }
            Text { x: 16; y: 12; text: "KEYBOARD LAYOUT"; color: Theme.ink; font.pixelSize: 13; font.bold: true }
            CapButton {
                objectName: "keyboard-layout-mode"
                x:12;y:34;width:296;height:30;flat:true;claimsFocus:false;selected:root.keyboard.optionIndex===0
                label: root.keyboard.touchLayout ? "Touch · iPhone / iPad" : "Full PC keyboard";textSize:13
                onActivated:root.keyboard.setLayout(root.keyboard.touchLayout?"pc":"touch")
            }
            Repeater {
                model: root.keyboard.touchLayout ? [] : ["number-pad","function-row","navigation-block","system-keys"]
                delegate: CapButton {
                    required property string modelData
                    required property int index
                    objectName: "keyboard-"+modelData
                    x: 12; y: 66+index*32; width: 296; height: 30; flat: true
                    claimsFocus: false; selected: root.keyboard.optionIndex===index+1
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
                x: 12; y: root.keyboard.touchLayout?73:201; width: parent.width-24; height: 25; flat: true
                claimsFocus: false; label: "B · Done"; textSize: 13
                onActivated: root.keyboard.toggleOptions()
            }
        }
    }
}
