import QtQuick

Item {
    id: root
    required property var flow
    property bool active: false
    Text {
        id: notice
        x: 12; y: 0; width: parent.width - 24
        text: root.flow.status || (root.flow.hiddenView ? "Hidden applications" : "")
        color: root.flow.status ? Theme.textColor("#96502d") : Theme.muted; font.pixelSize: 14
        elide: Text.ElideRight
    }
    GridView {
        id: grid; objectName: "apps-grid"
        anchors { fill: parent; topMargin: notice.text ? 32 : 10; bottomMargin: 8 }
        clip: true; cellWidth: width / 5; cellHeight: 116
        model: root.flow.rows; currentIndex: root.flow.railFocused ? -1 : root.flow.focusIndex
        onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, GridView.Contain)
        delegate: Item {
            required property var modelData
            required property int index
            width: grid.cellWidth; height: grid.cellHeight
            Accessible.role: Accessible.Button
            Accessible.name: modelData.name
            Accessible.onPressAction: root.flow.open(index)
            Image {
                anchors.horizontalCenter: parent.horizontalCenter; y: 7; width: 58; height: 58
                source: "image://desktop-app/" + encodeURIComponent(modelData.icon || "application-x-executable")
                sourceSize.width: 96; sourceSize.height: 96; fillMode: Image.PreserveAspectFit
            }
            Text {
                x: 10; y: 77; width: parent.width - 20; height: 34; text: modelData.name
                horizontalAlignment: Text.AlignHCenter; color: Theme.ink; font.pixelSize: 13
                maximumLineCount: 2; wrapMode: Text.Wrap; elide: Text.ElideRight
                font.bold: grid.currentIndex === index
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter; y: 70; width: 30; height: 3; radius: 1.5
                color: Theme.focusGlow; visible: grid.currentIndex === index
            }
            MouseArea {
                anchors.fill: parent
                onClicked: root.flow.open(index)
                onPressAndHold: root.flow.options(index)
            }
        }
    }
    Text {
        anchors.centerIn: parent; visible: !root.flow.rows.length
        text: root.flow.hiddenView ? "No hidden apps" : (root.flow.ready ? "No applications found" : "Loading applications…")
        color: Theme.muted; font.pixelSize: 18
    }
}
