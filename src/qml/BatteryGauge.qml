import QtQuick
import QtQuick.Shapes

Item {
    id: root
    objectName: "power-gauge"
    required property var status
    readonly property color chargeColor: !status.available ? "#8ea69f" : status.charging ? "#a9ddee" : status.percent <= 10 ? "#f3a6a9" : status.percent <= 20 ? "#f9d274" : "#b7dea0"
    width: 100; height: 27
    Accessible.role: Accessible.StaticText
    Accessible.name: status.available ? "Battery " + status.percent + " percent, " + status.state : "Battery unavailable"
    Rectangle { x: 0; y: 2; width: parent.width; height: 25; radius: 5; color: "#092d31" }
    Rectangle {
        x: 0; y: 0; width: parent.width; height: 25; radius: 5
        color: Qt.lighter(Theme.chassisDark, 1.16); border.color: Theme.rim
        Rectangle { x: 9; y: 5; width: 28; height: 14; radius: 2; color: "#102f32"; border.color: "#c4dace" }
        Rectangle { x: 37; y: 9; width: 3; height: 6; radius: 1; color: "#c4dace" }
        Rectangle { x: 12; y: 8; width: root.status.available ? 22 * root.status.percent / 100 : 0; height: 8; radius: 1; color: root.chargeColor }
        Shape {
            objectName: "power-charging"
            x: 18; y: 3; visible: root.status.available && root.status.charging
            ShapePath {
                fillColor: "#fffbe8"; strokeColor: "#294448"; strokeWidth: 0.6
                startX: 8; startY: 0
                PathLine { x: 1; y: 10 }
                PathLine { x: 6; y: 10 }
                PathLine { x: 3; y: 19 }
                PathLine { x: 12; y: 7 }
                PathLine { x: 7; y: 7 }
                PathLine { x: 8; y: 0 }
            }
        }
        Text { objectName: "power-percent"; x: 48; y: 3; width: 46; text: root.status.available ? root.status.percent + "%" : "—"; color: root.chargeColor; font.pixelSize: 14; font.weight: Font.DemiBold }
        Text { x: 20; y: 3; text: "!"; visible: root.status.available && !root.status.charging && root.status.percent <= 10; color: "#fffbe8"; font.pixelSize: 15; font.bold: true }
    }
}
