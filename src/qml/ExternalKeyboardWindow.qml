import QtQuick
import QtQuick.Window

Window {
    id: root
    required property var keyboard
    required property var bridge
    title: "TrainerOS Keyboard"
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    transientParent: null
    x: Screen.virtualX; y: Screen.virtualY; width: Screen.width; height: Screen.height
    visible: bridge.ready && keyboard.open
    readonly property real unit: Theme.uiScale
    readonly property var hitRegions: [{id:"keyboard",rect:Qt.rect(0,height-panel.occupiedHeight*unit,width,panel.occupiedHeight*unit)}]
    onHitRegionsChanged: bridge.setRegions(hitRegions)
    Component.onCompleted: {bridge.attach(root);bridge.setRegions(hitRegions)}
    onClosing: event => event.accepted=false
    QtObject {
        id: host
        readonly property var keyboard: root.keyboard
        readonly property bool menuOpen: false
        readonly property string notice: ""
        function pressButton(button) {root.keyboard.cancel()}
    }
    Item {
        width: root.width/root.unit; height: root.height/root.unit
        scale: root.unit; transformOrigin: Item.TopLeft
        KeyboardPanel {id:panel;anchors.fill:parent;shell:host;takesFocus:root.visible}
    }
    Connections {
        target: root.bridge
        function onPointer(target,phase,position) {if(target==="keyboard")root.bridge.forwardPointer(phase,position)}
    }
}
