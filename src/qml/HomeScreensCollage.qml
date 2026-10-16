import QtQuick

Item {
    id: root
    property url source
    property bool selected: false
    property bool balanced: false
    signal activated()
    readonly property real frameWidth: balanced ? Math.min(width/1.48,(height-14)*1.24) : Math.min(width * .68, height * 1.45)
    readonly property real frameHeight: balanced ? frameWidth*.75 : height*.82
    readonly property real rearTop: (frameHeight-frameWidth*Math.sin(Math.PI/20)-frameHeight*Math.cos(Math.PI/20))/2
    readonly property real frontTop: (frameHeight-frameWidth*Math.sin(Math.PI/30)-frameHeight*Math.cos(Math.PI/30))/2
    readonly property real artTop: Math.min(rearTop,11+frontTop)
    readonly property real artBottom: Math.max(frameHeight-rearTop,11+frameHeight-frontTop)+4
    readonly property real artOffset: balanced ? (height-artBottom+artTop)/2-artTop : 0
    readonly property real visualTop: artOffset+artTop
    readonly property real visualBottom: artOffset+artBottom
    Accessible.role: Accessible.Button
    Accessible.name: "Game screenshots"
    Accessible.onPressAction: activated()
    function claimFocus() { if(selected && visible)forceActiveFocus(Qt.OtherFocusReason) }
    onSelectedChanged: Qt.callLater(claimFocus)
    onVisibleChanged: Qt.callLater(claimFocus)
    Component.onCompleted: Qt.callLater(claimFocus)

    Item {
    width: parent.width; height: parent.height; y: root.artOffset
    // The rear mount is stationery, not a second invented capture. The current
    // media repository retains one confirmed exit image per owner/adventure.
    Item {
        x: Math.max(0,(parent.width-root.frameWidth*(root.balanced ? 1.30 : 1.38))/2)
        y: 0; width: root.frameWidth; height: root.frameHeight; rotation: -9
        Rectangle { x: 2; y: 4; width: parent.width; height: parent.height; radius: 5; color: "#40362d24" }
        Rectangle {
            anchors.fill: parent; radius: 4; color: "#d3dbc0"; border.color: "#899b7c"
            Rectangle { anchors.fill: parent; anchors.margins: 6; color: Theme.homeLandscape[5]; radius: 2; opacity: .55 }
            Rectangle { x: 11; y: parent.height-12; width: parent.width*.44; height: 2; color: "#94a181" }
        }
    }
    Item {
        x: Math.max(0,(parent.width-root.frameWidth*(root.balanced ? 1.30 : 1.38))/2)+root.frameWidth*(root.balanced ? .30 : .38)
        y: 11; width: root.frameWidth; height: root.frameHeight; rotation: 6
        Rectangle { x: 2; y: 4; width: parent.width; height: parent.height; radius: 5; color: "#50362d24" }
        Rectangle {
            anchors.fill: parent; radius: 4; color: "#fff1cf"
            border.color: root.selected ? Theme.focus : "#d2b883"; border.width: root.selected ? 3 : 1
            Rectangle {
                x: 6; y: 6; width: parent.width-12; height: parent.height-24
                color: Theme.homeLandscape[1]; radius: 2; clip: true
                Image {
                    id: photo; objectName: "home-screens-photo"
                    anchors.fill: parent; source: root.visible ? root.source : ""
                    sourceSize: Qt.size(360,240); asynchronous: true; cache: true
                    fillMode: Image.PreserveAspectFit
                }
                // An embossed camera keeps a new player's empty album inviting.
                Rectangle {
                    visible: photo.status !== Image.Ready
                    anchors.centerIn: parent; width: 32; height: 23; radius: 5
                    color: "transparent"; border.color: "#c6d4b4"; border.width: 2
                    Rectangle { x: 7; y: -4; width: 12; height: 4; radius: 2; color: "#c6d4b4" }
                    Rectangle { anchors.centerIn: parent; width: 12; height: 12; radius: 6; color: "transparent"; border.color: "#c6d4b4"; border.width: 2 }
                }
            }
            Text {
                x: 6; y: parent.height-17; width: parent.width-12
                text: "Screens"; horizontalAlignment: Text.AlignHCenter
                color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 12
            }
        }
    }
    }
    MouseArea { anchors.fill: parent; onClicked: root.activated() }
}
