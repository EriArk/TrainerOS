import QtQuick

// A cached, static field drawing. Repaint only on size/palette changes, never
// on page navigation or an animation tick. No input or domain dependencies.
Canvas {
    id: root
    property color lineColor: Theme.rim
    opacity: 0.24
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onLineColorChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.strokeStyle = root.lineColor;
        ctx.lineWidth = 1.5;
        for (let i = 0; i < 6; ++i) {
            ctx.beginPath();
            ctx.ellipse(322 - i * 25, -42 - i * 24, 280 + i * 50, 280 + i * 48);
            ctx.stroke();
        }
        ctx.setLineDash([4, 8]);
        ctx.beginPath();
        ctx.moveTo(220, 420);
        ctx.bezierCurveTo(500, 340, 380, 200, 715, -20);
        ctx.stroke();
        ctx.setLineDash([]);
        for (const point of [[540, 125], [294, 389]]) {
            ctx.beginPath(); ctx.arc(point[0], point[1], 5, 0, 2 * Math.PI); ctx.stroke();
        }
    }
}
