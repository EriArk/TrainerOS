import QtQuick

Item {
    id: root
    property string source: ""
    property string name: ""
    property string presence: ""
    width: 42; height: width
    Rectangle { anchors.fill: parent; anchors.topMargin: 3; anchors.bottomMargin: -3; radius: width/2; color: "#50142324" }
    Rectangle { anchors.fill: parent; radius: width/2; color: Theme.chassisFoot; border.color: Theme.edgeLight; border.width: 1 }
    Rectangle { anchors.fill: parent; anchors.margins: 3; radius: width/2; color: Theme.blue
        Text { anchors.centerIn: parent; text: (root.name || "?").substring(0,1).toUpperCase(); color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: parent.width*.5; textFormat: Text.PlainText }
    }
    Canvas {
        id: picture; x: 3; y: 3
        width: (parent.width-6)*Theme.uiScale; height: (parent.height-6)*Theme.uiScale
        scale: 1/Theme.uiScale; transformOrigin: Item.TopLeft
        antialiasing: true
        property string loadedSource: ""
        onCanvasSizeChanged: requestPaint()
        function refreshAvatar() {
            if(loadedSource)unloadImage(loadedSource)
            loadedSource=root.source
            if(loadedSource)loadImage(loadedSource)
            requestPaint()
        }
        Component.onCompleted: refreshAvatar()
        Connections { target: root; function onSourceChanged() { picture.refreshAvatar() } }
        onImageLoaded: requestPaint()
        onPaint: {
            const c=getContext("2d"); c.reset();c.clearRect(0,0,canvasSize.width,canvasSize.height)
            if(!loadedSource || !isImageLoaded(loadedSource))return
            c.save();c.beginPath();c.arc(canvasSize.width/2,canvasSize.height/2,canvasSize.width/2,0,Math.PI*2);c.clip()
            c.drawImage(loadedSource,0,0,canvasSize.width,canvasSize.height);c.restore()
        }
    }
    Rectangle {
        visible: !!root.presence; anchors.right: parent.right; anchors.bottom: parent.bottom
        width: Math.max(10,root.width*.22); height: width; radius: width/2
        color: root.presence === "online" ? "#8dcb83" : root.presence === "idle" ? Theme.yellow : root.presence === "dnd" ? Theme.pink : "#9ca4a0"
        border.color: Theme.chassisDark; border.width: 2
    }
}
