import QtQuick

Item {
    id: root
    property string selectedButton: ""
    property bool interactive: true
    signal picked(string button)
    readonly property var positions: [
        {id:"leftShoulder",x:.22,y:.20,label:"L1"}, {id:"rightShoulder",x:.78,y:.20,label:"R1"},
        {id:"leftTrigger",x:.22,y:.04,label:"L2"}, {id:"rightTrigger",x:.78,y:.04,label:"R2"},
        {id:"north",x:.79,y:.30,label:"north"}, {id:"south",x:.79,y:.52,label:"south"},
        {id:"west",x:.68,y:.41,label:"west"}, {id:"east",x:.90,y:.41,label:"east"},
        {id:"up",x:.22,y:.30,label:"↑"}, {id:"down",x:.22,y:.52,label:"↓"},
        {id:"left",x:.11,y:.41,label:"←"}, {id:"right",x:.33,y:.41,label:"→"},
        {id:"leftStick",x:.37,y:.70,label:"L3"}, {id:"rightStick",x:.65,y:.70,label:"R3"},
        {id:"select",x:.43,y:.39,label:"Select"}, {id:"start",x:.58,y:.39,label:"Start"}]
    Canvas {
        id: outline
        Connections { target: Theme; function onDarkChanged(){outline.requestPaint()} }
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const c=getContext("2d"); c.reset(); c.scale(width/400,height/280)
            c.strokeStyle=Theme.divider; c.lineWidth=1.5; c.fillStyle=Theme.surface
            c.beginPath(); c.moveTo(105,38);c.bezierCurveTo(55,25,37,60,24,115);c.bezierCurveTo(-4,238,25,276,76,229)
            c.lineTo(125,189);c.bezierCurveTo(175,180,225,180,275,189);c.lineTo(324,229)
            c.bezierCurveTo(375,276,404,238,376,115);c.bezierCurveTo(363,60,345,25,295,38)
            c.bezierCurveTo(245,52,155,52,105,38);c.closePath();c.fill();c.stroke()
        }
    }
    Repeater { model: root.positions
        Item { required property var modelData
            width: Math.max(20,root.width*.088); height: Math.max(18,root.height*.115)
            x: modelData.x*root.width-width/2; y: modelData.y*root.height-height/2
            readonly property bool active: root.selectedButton===modelData.id
            Rectangle { anchors.centerIn: parent; width: parent.width; height: parent.height; radius: 16; color: parent.active ? Theme.surfaceColor("#e2eee6") : Theme.surfaceColor("#fbfcf9"); border.width: parent.active ? 2 : 1; border.color: parent.active ? Theme.surfaceColor("#4b8a7b") : Theme.surfaceColor("#cdd7cf") }
            Text { anchors.centerIn: parent; text: {const f=Theme.controller?.family; return Theme.buttonLabel(modelData.label)} color: Theme.textColor("#48645a"); font.pixelSize: ["select","start"].includes(modelData.id)?7:Math.min(12,root.width/24) }
            TapHandler { enabled: root.interactive; onTapped: root.picked(modelData.id) }
        }
    }
}
