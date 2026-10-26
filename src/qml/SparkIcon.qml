import QtQuick

// Shared currency mark: a golden spark suspended in a clear glass coin.
Canvas {
    id: root
    implicitWidth: 32
    implicitHeight: 32
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const c = getContext("2d")
        c.reset()
        const size = Math.min(width, height)
        c.translate((width-size)/2, (height-size)/2)
        c.scale(size/64, size/64)
        const glass = c.createLinearGradient(12, 4, 48, 60)
        glass.addColorStop(0, "#80f4ffff")
        glass.addColorStop(.42, "#16a9d8e5")
        glass.addColorStop(.72, "#08d4edf4")
        glass.addColorStop(1, "#608dbbce")
        c.beginPath(); c.arc(32,32,29,0,Math.PI*2)
        c.fillStyle = glass; c.fill()
        const rim = c.createLinearGradient(10, 4, 50, 60)
        rim.addColorStop(0, "#f0ffffff")
        rim.addColorStop(.48, "#60809fae")
        rim.addColorStop(1, "#d5e4faff")
        c.strokeStyle = rim; c.lineWidth = 1.8; c.stroke()
        c.beginPath(); c.arc(32,32,25.7,0,Math.PI*2)
        c.strokeStyle = "#4889afbf"; c.lineWidth = .8; c.stroke()
        const gold = c.createLinearGradient(23, 12, 41, 53)
        gold.addColorStop(0, "#fff3b5")
        gold.addColorStop(.5, "#ffdd79")
        gold.addColorStop(1, "#efa738")
        c.beginPath(); c.moveTo(32,12); c.lineTo(37,26)
        c.lineTo(49,32); c.lineTo(37,37); c.lineTo(32,53)
        c.lineTo(27,37); c.lineTo(15,32); c.lineTo(27,26); c.closePath()
        c.fillStyle = gold; c.fill()
        c.strokeStyle = "#b77b2c"; c.lineWidth = 1.4; c.stroke()
        c.beginPath(); c.moveTo(32,18); c.lineTo(35,29)
        c.lineTo(43,32); c.lineTo(35,35); c.lineTo(32,46)
        c.lineTo(29,35); c.lineTo(21,32); c.lineTo(29,29); c.closePath()
        c.fillStyle = "#fff0ae"; c.fill()
        if (size >= 24) {
            c.fillStyle = "#f9d77b"
            c.beginPath(); c.arc(46,20,2,0,Math.PI*2); c.fill()
            c.beginPath(); c.arc(18,44,1.5,0,Math.PI*2); c.fill()
        }
        c.beginPath(); c.arc(32,32,26.8,Math.PI*1.12,Math.PI*1.54)
        c.strokeStyle = "#d9ffffff"; c.lineWidth = 2.4; c.lineCap = "round"; c.stroke()
        c.beginPath(); c.arc(32,32,26.8,Math.PI*.15,Math.PI*.35)
        c.strokeStyle = "#8cffffff"; c.lineWidth = 1.2; c.stroke()
    }
}
