import QtQuick
import QtQuick.Window

Window {
    id: root
    required property var flow
    required property var bridge
    objectName: "app-controls-window"
    title: "TrainerOS — Application controls"
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    transientParent: null
    x: Screen.virtualX; y: Screen.virtualY; width: Screen.width; height: Screen.height
    visible: bridge.ready && flow.current.length > 0
    readonly property real unit: Math.max(1,Math.min(width/960,height/540))
    readonly property var hitRegions: flow.controlsOpen ? [0,1,2].map(i => ({id:"control-"+i,rect:Qt.rect(card.x+(12+i*106)*unit,card.y+45*unit,100*unit,48*unit)})) : [{id:"open",rect:Qt.rect(chip.x,chip.y,chip.width,chip.height)}]
    onHitRegionsChanged: bridge.setRegions(hitRegions)
    Component.onCompleted: {bridge.attach(root);bridge.setRegions(hitRegions)}
    onClosing: event => event.accepted=false
    property string pressed: ""
    Connections {
        target: root.bridge
        function onPointer(target,phase,position) {
            if(phase==="press")root.pressed=target
            if(phase==="release") {
                const region=root.hitRegions.find(r=>r.id===target)
                if(root.pressed===target && region && position.x>=region.rect.x && position.x<region.rect.x+region.rect.width && position.y>=region.rect.y && position.y<region.rect.y+region.rect.height) {
                    if(target==="open")root.flow.showControls();else root.flow.control(Number(target.slice(8)))
                }
                root.pressed=""
            }
        }
    }
    Rectangle {
        id: chip; visible: !flow.controlsOpen; width: 52*unit; height: width; radius: width/2
        x: root.width-width-14*unit; y: 14*unit; color: Theme.green; border.color: Theme.paper; border.width: 2*unit
        Text {anchors.centerIn: parent;text:"HOME";font.family:Theme.brandFamily;font.pixelSize:10*unit;color:Theme.ink}
        MouseArea { anchors.fill: parent; onClicked: flow.showControls() }
    }
    Rectangle {
        id: card; visible: flow.controlsOpen; width: 342*unit; height: 105*unit; radius: 16*unit
        x: root.width-width-16*unit; y: 16*unit; color: Theme.paper; border.color: Theme.ink; border.width: 2*unit
        Text { x: 14*unit;y:12*unit;width:parent.width-28*unit;text:flow.currentName;elide:Text.ElideRight;color:Theme.ink;font.family:Theme.displayFamily;font.pixelSize:18*unit }
        Repeater {
            model: ["Return","Minimize","Close"]
            Rectangle {
                required property string modelData;required property int index
                x:(12+index*106)*unit;y:45*unit;width:100*unit;height:48*unit;radius:10*unit
                color:flow.controlIndex===index?Theme.yellow:(index===2?Theme.pink:Theme.blue)
                border.color:flow.controlIndex===index?Theme.ink:"transparent";border.width:2*unit
                Text {anchors.centerIn:parent;text:modelData;color:Theme.ink;font.pixelSize:14*unit}
                MouseArea {anchors.fill:parent;onClicked:flow.control(index)}
            }
        }
    }
}
