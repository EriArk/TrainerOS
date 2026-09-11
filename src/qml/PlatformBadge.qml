import QtQuick

// Original hardware silhouettes, seated in a recessed badge on the card.
Rectangle {
    id: root
    property string label: ""
    property string shape: "console"
    width: 72; height: 44; radius: 7
    color: "#d7ded7"; border.color: "#8c9f97"
    Rectangle { x: 2; y: 2; width: parent.width - 4; height: 1; color: "#71877d" }
    Canvas {
        id: silhouette
        x: 20; y: 5; width: 32; height: 20
        onPaint: {
            var c = getContext("2d"); c.reset(); c.strokeStyle = "#405b51"; c.fillStyle = "#405b51"; c.lineWidth = 1.5
            if (root.shape === "dual") {
                c.strokeRect(6, 0, 20, 9); c.strokeRect(4, 11, 24, 8)
                c.strokeRect(10, 2, 12, 5); c.strokeRect(11, 13, 10, 4)
            } else if (root.shape === "handheld") {
                c.strokeRect(9, 0, 14, 19); c.strokeRect(11, 2, 10, 7)
                c.fillRect(11, 13, 5, 1.5); c.fillRect(13, 11, 1.5, 5)
                c.beginPath(); c.arc(20, 13, 1.3, 0, 7); c.fill()
            } else if (root.shape === "wide") {
                c.strokeRect(1, 3, 30, 15); c.strokeRect(7, 5, 18, 11)
                c.fillRect(3, 9, 3, 1.5); c.fillRect(4, 8, 1, 4)
                c.fillRect(27, 9, 2, 2)
            } else if (root.shape === "computer") {
                c.strokeRect(4, 0, 24, 14); c.fillRect(14, 15, 4, 3); c.fillRect(8, 18, 16, 1.5)
            } else if (root.shape === "cube") {
                c.beginPath(); c.moveTo(4, 5); c.lineTo(16, 0); c.lineTo(28, 5); c.lineTo(28, 15); c.lineTo(16, 20); c.lineTo(4, 15); c.closePath(); c.stroke()
                c.moveTo(4, 5); c.lineTo(16, 10); c.lineTo(28, 5); c.moveTo(16, 10); c.lineTo(16, 20); c.stroke()
            } else {
                c.strokeRect(1, 4, 30, 13); c.fillRect(5, 7, 16, 2); c.fillRect(24, 12, 3, 2)
            }
        }
        Connections { target: root; function onShapeChanged() { silhouette.requestPaint() } }
    }
    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 3 } text: root.label; font.pixelSize: 10; font.weight: Font.DemiBold; color: "#405b51" }
}
