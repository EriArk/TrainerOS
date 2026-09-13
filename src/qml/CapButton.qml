import QtQuick

Item {
    id: root
    property string label: ""
    property string detail: ""
    property color tint: Theme.yellow
    property bool selected: false
    property int textSize: 17
    property bool centered: false
    property color focusColor: Theme.focus
    property string platform: ""
    property string platformShape: "console"
    property bool warning: false
    property bool depressed: false
    readonly property color capTint: enabled ? tint : "#c4cdc7"
    function pressFeedback() {
        if (enabled && visible) { depressed = true; release.restart(); }
    }
    Timer { id: release; interval: 120; onTriggered: root.depressed = false }
    signal activated()
    implicitWidth: 200
    implicitHeight: 56
    activeFocusOnTab: false
    Accessible.role: Accessible.Button
    Accessible.name: label + (detail.length ? ", " + detail : "")
    onSelectedChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    onVisibleChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    Component.onCompleted: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    Rectangle {
        x: -1; y: 2; width: parent.width + 2; height: parent.height + 3
        radius: 10; color: "#50324438"
    }
    Rectangle {
        y: 3; width: parent.width; height: parent.height
        radius: 9; color: Qt.darker(root.capTint, 1.7); border.color: Qt.darker(root.capTint, 1.9)
    }
    Item {
        id: cap
        y: root.depressed || pointer.pressed ? 3 : 0
        width: parent.width; height: parent.height
        Behavior on y { NumberAnimation { duration: Theme.motion(55) } }
        Rectangle {
            anchors.fill: parent; radius: 9; border.color: Qt.darker(root.capTint, 1.6)
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.lighter(root.capTint, 1.18) }
                GradientStop { position: 0.15; color: Qt.lighter(root.capTint, 1.08) }
                GradientStop { position: 1; color: root.capTint }
            }
            Rectangle { anchors.fill: parent; anchors.margins: 2; radius: 7; color: "transparent"; border.color: "#65ffffff" }
            Rectangle { x: 7; y: 2; width: parent.width - 14; height: 1; color: "#b3ffffff" }
        }
        Column {
            anchors { left: parent.left; right: parent.right; margins: 13; verticalCenter: parent.verticalCenter }
            anchors.leftMargin: root.warning ? 39 : 13
            anchors.rightMargin: root.platform.length ? 95 : 13
            spacing: 3
            Text {
                width: parent.width; text: root.label
                textFormat: Text.PlainText
                color: Theme.ink; font.pixelSize: root.textSize; font.weight: Font.DemiBold
                horizontalAlignment: root.centered ? Text.AlignHCenter : Text.AlignLeft
                elide: Text.ElideRight
            }
            Text {
                width: parent.width; text: root.detail; visible: text.length > 0
                textFormat: Text.PlainText
                color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight
            }
        }
        PlatformBadge {
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            visible: root.platform.length > 0; label: root.platform; shape: root.platformShape
        }
        Rectangle {
            x: 12; anchors.verticalCenter: parent.verticalCenter; width: 18; height: 18; radius: 9
            visible: root.warning; color: "#713e37"
            Text { anchors.centerIn: parent; text: "!"; color: "#fff0d8"; font.pixelSize: 13; font.bold: true }
        }
    }
    // A bright core and broad inward bloom remain intact at clipped list edges.
    // The outside footprint stays within the existing four-unit focus gutter.
    Item {
        anchors.fill: parent; z: 1; visible: root.selected
        Repeater {
            model: 12
            delegate: Rectangle {
                required property int index
                readonly property real inset: index - 4
                x: inset; y: inset
                width: root.width - inset * 2; height: root.height - inset * 2
                radius: Math.max(2, 9 - inset); color: "transparent"
                border.width: 2
                border.color: Theme.focusGlow
                opacity: [0.16, 0.32, 0.56, 0.85, 0.92, 0.72, 0.52, 0.36, 0.24, 0.15, 0.08, 0.03][index]
            }
        }
        Rectangle {
            x: -1; y: -1; width: parent.width + 2; height: parent.height + 2
            radius: 10; color: "transparent"
            border.width: 3; border.color: root.focusColor
        }
        Rectangle {
            x: 1; y: 1; width: parent.width - 2; height: parent.height - 2
            radius: 8; color: "transparent"
            border.width: 1; border.color: "#fff9d6"
        }
    }
    MouseArea { id: pointer; anchors.fill: parent; onClicked: { root.pressFeedback(); root.activated(); } }
}
