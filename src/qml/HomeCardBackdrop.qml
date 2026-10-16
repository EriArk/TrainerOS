import QtQuick

// Five original generated landscapes, limited to the game card. Canvas provides
// rounded clipping on the software renderer as well as the GPU renderer.
Rectangle {
    id: root
    readonly property string themeId: Theme.themeId
    readonly property string artwork: "qrc:/home-backdrops/" + (Theme.palettes[themeId] ? themeId : "turquoise") + ".png"
    property string renderedArtwork: ""
    readonly property bool artworkReady: renderedArtwork === artwork
    radius: 15; color: Theme.chassis
    Canvas {
        id: art
        width: parent.width*Theme.uiScale; height: parent.height*Theme.uiScale
        scale: 1/Theme.uiScale; transformOrigin: Item.TopLeft
        antialiasing: true
        property string loaded: ""
        function refresh() { root.renderedArtwork="";if(loaded)unloadImage(loaded);loaded=root.artwork;loadImage(loaded);requestPaint() }
        Component.onCompleted: refresh()
        Connections { target: root; function onArtworkChanged() { art.refresh() } }
        onImageLoaded: requestPaint()
        onCanvasSizeChanged: requestPaint()
        onPaint: {
            const c=getContext("2d");c.reset();c.clearRect(0,0,canvasSize.width,canvasSize.height)
            if(!isImageLoaded(loaded))return
            const w=canvasSize.width,h=canvasSize.height,r=root.radius*Theme.uiScale
            c.save();c.beginPath();c.moveTo(r,0);c.lineTo(w-r,0);c.quadraticCurveTo(w,0,w,r);c.lineTo(w,h-r);c.quadraticCurveTo(w,h,w-r,h);c.lineTo(r,h);c.quadraticCurveTo(0,h,0,h-r);c.lineTo(0,r);c.quadraticCurveTo(0,0,r,0);c.closePath();c.clip()
            const ratio=Math.max(w/1536,h/1024),dw=1536*ratio,dh=1024*ratio
            c.drawImage(loaded,(w-dw)/2,(h-dh)/2,dw,dh)
            const shade=c.createLinearGradient(0,0,0,h);shade.addColorStop(0,"#10102020");shade.addColorStop(.48,"#08102020");shade.addColorStop(1,"#40102020")
            c.fillStyle=shade;c.fillRect(0,0,w,h);c.restore();root.renderedArtwork=loaded
        }
    }
}
