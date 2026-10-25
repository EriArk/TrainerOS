import QtQuick
import QtQuick.Window

Window {
    id: root
    required property var flow
    required property var bridge
    required property bool permitted
    color: "transparent"
    title: "TrainerOS — Radial input"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    transientParent: null
    x: Screen.virtualX; y: Screen.virtualY; width: Screen.width; height: Screen.height
    visible: permitted && bridge.ready && !!flow.radial.slots?.length
    readonly property var regions: {
        const result=[], count=flow.radial.slots?.length || 0
        if(!visible)return result
        const d=panel.width-16, w=Math.max(32,d*.26), h=Math.min(38,d*.18)
        for(let i=0;i<count;i++)result.push({id:"slot:"+i,rect:Qt.rect(panel.x+panel.width/2+Math.sin(i*2*Math.PI/count)*d*.34-w/2,panel.y+panel.height/2-Math.cos(i*2*Math.PI/count)*d*.34-h/2,w,h)})
        // The bridge uses these regions for both painting and pointer targeting.
        // Keep the full ring visible; slot regions precede this inert backdrop.
        result.push({id:"ring-surface",rect:Qt.rect(panel.x,panel.y,panel.width,panel.height)})
        return result
    }
    onRegionsChanged: bridge.setRegions(regions)
    Component.onCompleted: {bridge.attach(root);bridge.setRegions(regions)}
    Connections { target: root.bridge; function onPointer(target,phase,position) { if(phase==="release" && target.startsWith("slot:"))root.flow.selectRadialSlot(Number(target.slice(5))) } }
    Rectangle { id: panel; anchors.centerIn: parent; width: Math.min(370*Theme.uiScale,parent.width*.6); height: width; radius: width/2; color: Theme.surfaceColor("#f5fafbf7")
        RingDiagram { anchors.fill: parent; anchors.margins: 8; slots: flow.radial.slots || []; presentation: flow.radial.presentation || ({}); selected: flow.radial.selected ?? -1; caption: flow.radial.presentation?.name || "Release or tap to choose"; onPicked: slot => root.flow.selectRadialSlot(slot) }
    }
}
