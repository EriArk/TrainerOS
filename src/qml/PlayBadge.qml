import QtQuick

Rectangle {
    id: root
    property string label: ""
    property bool online: false
    property bool solo: false
    width: caption.implicitWidth + 41; height: 28; radius: 6
    color: online ? "#bedfe9" : solo ? "#dedfc9" : "#c5e2ad"
    border.color: online ? "#6c9ca7" : solo ? "#9caa8f" : "#829e67"
    Rectangle { x: 3; y: 2; width: parent.width - 6; height: 1; color: "#65ffffff" }
    Canvas {
        id: icon
        x: 7; y: 5; width: 21; height: 18
        onPaint: {
            const c = getContext("2d"); c.reset(); c.strokeStyle = "#284c45"; c.fillStyle = "#284c45"; c.lineWidth = 1.5
            if (root.online) {
                c.beginPath(); c.arc(10, 9, 8, 0, Math.PI * 2); c.stroke()
                c.beginPath(); c.ellipse(6, 1, 8, 16); c.stroke()
                c.beginPath(); c.moveTo(2, 9); c.lineTo(18, 9); c.stroke()
            } else {
                const person = function(x, y) {
                    c.beginPath(); c.arc(x, y + 3, 3, 0, Math.PI * 2); c.fill()
                    c.beginPath(); c.arc(x, y + 12, 5, Math.PI, Math.PI * 2); c.lineTo(x + 5, y + 14); c.lineTo(x - 5, y + 14); c.closePath(); c.fill()
                }
                if (root.solo) person(10, 1)
                else { person(6, 2); person(16, 0) }
            }
        }
        Connections { target: root; function onSoloChanged() { icon.requestPaint() } function onOnlineChanged() { icon.requestPaint() } }
    }
    Text {
        id: caption
        x: 33; anchors.verticalCenter: parent.verticalCenter
        text: root.label; textFormat: Text.PlainText
        font.pixelSize: 12; font.bold: true; color: Theme.ink
    }
}
