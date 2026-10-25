import QtQuick

Item {
    id: root
    property real textInset: 13
    property string title
    property string detail
    property string kind: "action"
    property bool selected: false
    property bool checked: false
    property real level: -1
    property bool muted: false
    property bool compact: false
    signal activated()
    signal adjustmentRequested(int direction)
    signal levelRequested(real value)
    implicitHeight: 62
    onSelectedChanged: if(selected && visible)forceActiveFocus(Qt.OtherFocusReason)
    readonly property bool adjustable: kind === "stepper" || kind === "theme"
    readonly property bool slider: kind === "volume" || kind === "brightness"
    Rectangle { x: 0; y: 10; width: 2; height: parent.height-20; color: "#36847c"; visible: root.selected }
    readonly property int trailingSpace: slider ? 122 : adjustable ? (compact?62:100) : kind==="toggle"?58:30
    Text { x: root.textInset; y: root.compact?3:root.height<48?5:9; width: parent.width-root.textInset-root.trailingSpace; text: root.title; color: root.kind === "unavailable" ? "#7f8986" : "#263d38"; font.pixelSize: root.compact?12:15; font.weight: root.selected ? Font.DemiBold : Font.Normal; elide: Text.ElideRight }
    Text { x: root.textInset; y: root.compact?20:root.height<48?27:31; width: parent.width-root.textInset-root.trailingSpace; text: root.detail; textFormat: Text.PlainText; color: "#78857f"; font.pixelSize: 11; elide: Text.ElideRight }
    Rectangle { x: 13; y: parent.height-1; width: parent.width-13; height: 1; color: "#e1e6e0" }
    MouseArea { anchors.fill: parent; enabled: !["unavailable","status"].includes(root.kind); onClicked: root.activated() }
    Row { anchors.right: parent.right; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter; spacing: root.compact?0:14; visible: root.adjustable
        Text { objectName: "settings-step-previous"; text: "−"; color: "#617b73"; font.pixelSize: 21; width: 28; height: 36; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            MouseArea { anchors.fill: parent; onClicked: root.adjustmentRequested(-1) } }
        Text { objectName: "settings-step-next"; text: "+"; color: "#617b73"; font.pixelSize: 21; width: 28; height: 36; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            MouseArea { anchors.fill: parent; onClicked: root.adjustmentRequested(1) } }
    }
    Rectangle { anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter; width: 33; height: 18; radius: 9; visible: root.kind==="toggle"; color: root.checked ? "#57988b" : "#cdd5cf"
        Rectangle { x: root.checked ? 17 : 2; y: 2; width: 14; height: 14; radius: 7; color: "#ffffff" }
    }
    Text { anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; visible: root.kind==="action"; text: "›"; color: "#8b9a92"; font.pixelSize: 22 }
    Item { anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter; width: 100; height: 38; visible: root.slider
        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 3; radius: 1; color: "#dce3dd"
            Rectangle { width: parent.width*Math.max(0,root.level)/100; height: 3; color: "#57988b" }
        }
        Rectangle { x: (parent.width-10)*Math.max(0,root.level)/100; anchors.verticalCenter: parent.verticalCenter; width: 10; height: 10; radius: 5; color: "#57988b" }
        MouseArea { anchors.fill: parent; onPressed: mouse => root.levelRequested(Math.round(mouse.x/width*100)); onPositionChanged: mouse => { if(pressed) root.levelRequested(Math.max(0,Math.min(100,Math.round(mouse.x/width*100)))) } }
    }
}
