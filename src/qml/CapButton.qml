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
    signal activated()
    implicitWidth: 200
    implicitHeight: 56
    activeFocusOnTab: false
    onSelectedChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    onVisibleChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    Component.onCompleted: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    Rectangle {
        x: -4; y: -4; width: parent.width + 8; height: parent.height + 8
        radius: 13
        color: "transparent"
        border.width: root.selected ? 3 : 0
        border.color: root.focusColor
    }
    Rectangle {
        y: 4; width: parent.width; height: parent.height
        radius: 9; color: "#703d5149"
    }
    Rectangle {
        anchors.fill: parent
        radius: 9
        border.color: Qt.darker(root.tint, 1.6)
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.lighter(root.tint, 1.15) }
            GradientStop { position: 1; color: root.tint }
        }
        Rectangle {
            x: 5; y: 1; width: parent.width - 10; height: 1
            color: "#c0ffffff"
        }
    }
    Column {
        anchors { left: parent.left; right: parent.right; margins: 13; verticalCenter: parent.verticalCenter }
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
    MouseArea { anchors.fill: parent; onClicked: root.activated() }
}
