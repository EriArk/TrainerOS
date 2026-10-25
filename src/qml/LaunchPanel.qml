import QtQuick

Item {
    id: root
    required property var launch
    readonly property var info: launch.state
    Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#b0102022") }
    MouseArea { anchors.fill: parent }
    Rectangle { anchors.centerIn: popup; anchors.verticalCenterOffset: 6; width: popup.width + 4; height: popup.height + 4; radius: 20; color: Theme.surfaceColor("#55000000") }
    Panel {
        id: popup
        anchors.centerIn: parent
        width: Math.min(620, parent.width - 72); height: 320
        patterned: false
        Item {
            x: 11; y: 11; width: parent.width - 22; height: 186; clip: true
            Rectangle { anchors.fill: parent; color: Theme.chassisTop }
            ShellBackgroundPattern { anchors.fill: parent; opacity: 0.3 }
            Image { anchors.fill: parent; source: root.info.art || ""; fillMode: Image.PreserveAspectCrop; asynchronous: true }
            Rectangle { anchors.fill: parent; gradient: Gradient {
                GradientStop { position: 0; color: Theme.surfaceColor("#30101a20") }
                GradientStop { position: 1; color: Theme.surfaceColor("#ef152c2d") }
            } }
            Text { x: 24; y: 22; text: "LET’S PLAY"; font.pixelSize: 12; font.letterSpacing: 2; color: Theme.yellow }
            Text { x: 24; anchors.bottom: parent.bottom; anchors.bottomMargin: 22; width: parent.width - 48
                text: root.info.title || ""; color: Theme.accentInk; font.family: Theme.displayFamily; font.pixelSize: 30
                maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight; textFormat: Text.PlainText }
        }
        Item {
            x: 30; y: 225; width: 26; height: 26
            RotationAnimator on rotation { from: 0; to: 360; duration: 1100; loops: Animation.Infinite; running: root.visible && !Theme.reducedMotion }
            Repeater { model: 8
                Rectangle { required property int index; width: 5; height: 5; radius: 3; color: Theme.chassisTop; opacity: (index+1)/8
                    x: 11 + 10*Math.cos(index*Math.PI/4); y: 11 + 10*Math.sin(index*Math.PI/4) }
            }
        }
        Text { x: 72; y: 211; width: parent.width - 250; height: 56; verticalAlignment: Text.AlignVCenter
            text: root.info.stage || "Opening game…"; font.pixelSize: 18; color: Theme.ink; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
        CapButton { objectName: "launch-cancel"; anchors.right: parent.right; anchors.rightMargin: 27; y: 219
            width: 132; height: 40; tint: Theme.pink; label: "Cancel"; selected: root.visible
            enabled: root.info.cancellable !== false; onActivated: root.launch.cancel() }
        Text { x: 30; y: 281; text: "B · Cancel opening"; font.pixelSize: 12; color: Theme.muted }
    }
}
