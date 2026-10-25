import QtQuick

Item {
    id: root
    property var slots: []
    property var presentation: ({})
    function icon(name){return ({star:"★",bag:"▣",sword:"⚔",heart:"♥",bolt:"ϟ",target:"◎"})[name] || ""}
    property int selected: -1
    property string caption: "Radial menu"
    property bool interactive: true
    signal picked(int slot)
    readonly property real diameter: Math.min(width,height)
    Rectangle { anchors.centerIn: parent; width: root.diameter*.69; height: width; radius: width/2; color: "transparent"; border.color: Theme.divider; border.width: 1 }
    Text { anchors.centerIn: parent; width: root.diameter*.32; text: root.caption; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: Theme.textColor("#799084"); font.pixelSize: root.diameter<200?10:12 }
    Repeater { model: root.slots
        Item { required property int index; required property string modelData
            x: root.width/2+Math.sin(index*2*Math.PI/root.slots.length)*root.diameter*.34-width/2
            y: root.height/2-Math.cos(index*2*Math.PI/root.slots.length)*root.diameter*.34-height/2
            width: Math.max(32,root.diameter*.26); height: Math.min(38,root.diameter*.18)
            Rectangle { anchors.fill: parent; radius: 7; color: root.selected===index ? Theme.selection : Theme.surface; border.width: root.selected===index?1:0; border.color: Theme.accent }
            Text { anchors.centerIn: parent; width: parent.width-8; text: (root.icon(root.presentation.slots?.[index]?.icon || "")+" "+(root.presentation.slots?.[index]?.label || modelData)).trim(); elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; color: Theme.textColor("#375d4e"); font.pixelSize: root.diameter<200?10:12; font.weight: root.selected===index?Font.DemiBold:Font.Normal }
            TapHandler { enabled: root.interactive; onTapped: root.picked(index) }
        }
    }
}
