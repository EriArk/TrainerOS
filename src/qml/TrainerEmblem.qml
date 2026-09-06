import QtQuick

Rectangle {
    id: root
    property string emblem: "compass"
    radius: width / 2
    color: "#e7f0e3"; border.color: "#a9c5b3"; border.width: 2
    Canvas {
        id: drawing
        anchors.fill: parent
        anchors.margins: 14
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: root; function onEmblemChanged() { drawing.requestPaint(); } }
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.translate(width / 2, height / 2);
            ctx.scale(width / 160, height / 160);
            ctx.fillStyle = "#91c6b3"; ctx.strokeStyle = "#397b68"; ctx.lineWidth = 4;
            ctx.beginPath();
            if (root.emblem === "leaf") {
                ctx.moveTo(-45, 49); ctx.bezierCurveTo(-75, -20, 2, -65, 49, -51);
                ctx.bezierCurveTo(64, 4, 28, 68, -45, 49); ctx.fill(); ctx.stroke();
                ctx.beginPath(); ctx.moveTo(-46, 51); ctx.lineTo(29, -30); ctx.stroke();
            } else if (root.emblem === "spark") {
                for (let i = 0; i < 16; ++i) {
                    const radius = i % 2 === 0 ? 64 : 27;
                    const angle = i * Math.PI / 8 - Math.PI / 2;
                    const x = Math.cos(angle) * radius, y = Math.sin(angle) * radius;
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                }
                ctx.closePath(); ctx.fill(); ctx.stroke();
            } else {
                ctx.moveTo(0, -68); ctx.lineTo(56, 0); ctx.lineTo(0, 68); ctx.lineTo(-56, 0);
                ctx.closePath(); ctx.fill(); ctx.stroke();
            }
            ctx.beginPath(); ctx.arc(0, 0, 16, 0, Math.PI * 2);
            ctx.fillStyle = "#f4cc62"; ctx.fill(); ctx.stroke();
        }
    }
}
