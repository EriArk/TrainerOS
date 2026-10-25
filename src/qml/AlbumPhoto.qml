import QtQuick

Item {
    id: root
    property url source
    property bool prominent: false
    property bool available: true
    signal activated()
    property real ratio: 16/9
    readonly property real frameWidth: Math.min(width-8,(height-24)*ratio+20)
    readonly property real frameHeight: (frameWidth-20)/ratio+20
    Accessible.role: Accessible.Button
    Accessible.name: prominent ? "Selected screenshot" : "Adjacent screenshot"
    Accessible.onPressAction: activated()
    Item {
        anchors.centerIn: parent; width: root.frameWidth; height: root.frameHeight
        Rectangle { x: 0; y: 7; width: parent.width; height: parent.height; radius: 10; color: Theme.surfaceColor("#17312a25") }
        Rectangle { x: 1; y: 4; width: parent.width; height: parent.height; radius: 9; color: Theme.surfaceColor("#29312a25") }
        Rectangle {
            anchors.fill: parent; radius: 9; border.width: 1; border.color: Theme.edgeShadow
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.edgeLight }
                GradientStop { position: .45; color: Theme.rim }
                GradientStop { position: 1; color: Theme.chassis }
            }
            Rectangle {
                anchors.fill: parent; anchors.margins: 2; radius: 7
                border.color: Qt.alpha(Theme.paper,.8)
                gradient: Gradient {
                    GradientStop { position: 0; color: Theme.surfaceColor("#fff6dd") }
                    GradientStop { position: .45; color: Qt.tint(Theme.surfaceColor("#ece7d5"),Qt.alpha(Theme.rim,.18)) }
                    GradientStop { position: 1; color: Theme.surfaceColor("#c6bca4") }
                }
                Rectangle {
                    anchors.fill: parent; anchors.margins: 5; radius: 3
                    color: Theme.chassisDark; border.width: 2; border.color: Qt.darker(Theme.rim,1.6)
                }
            }
            Image {
                id: photo; objectName: root.prominent ? "album-main-photo" : "album-neighbor-photo"
                anchors.fill: parent; anchors.margins: 10
                source: root.visible ? root.source : ""; asynchronous: true; cache: true
                sourceSize: root.prominent ? Qt.size(1280,960) : Qt.size(480,360)
                fillMode: Image.PreserveAspectFit
            }
            Text {
                anchors.centerIn: parent; visible: photo.status === Image.Error
                text: "Picture unavailable"; color: Theme.accentInk; font.pixelSize: 12
            }
            Rectangle { x: 10; y: 2; width: parent.width-20; height: 1; color: Theme.surfaceColor("#b3fffdf0") }
            MouseArea { anchors.fill: parent; enabled: root.available; onClicked: root.activated() }
        }
    }
}
