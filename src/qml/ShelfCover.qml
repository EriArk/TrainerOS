import QtQuick

Item {
    id: root
    required property var entry
    property bool selected: false
    property bool active: false
    property real coverHeight: 130
    readonly property bool realCover: art.status === Image.Ready
    readonly property real aspect: realCover && art.implicitHeight > 0 ? art.implicitWidth / art.implicitHeight : 2 / 3
    // Reserve room for the selected case's 3.5% lift and halo at every size.
    readonly property real availableCoverHeight: coverHeight - Math.max(0,coverHeight*0.035-1)
    readonly property real bookWidth: Math.min(width - 26, availableCoverHeight * aspect)
    readonly property real bookHeight: Math.min(availableCoverHeight, bookWidth / aspect)
    signal activated()
    signal favoriteRequested(string gameId)
    Accessible.role: Accessible.ListItem
    Accessible.name: entry.title || ""
    Accessible.selected: selected
    onSelectedChanged: if (selected && active) forceActiveFocus(Qt.OtherFocusReason)
    onActiveChanged: if (selected && active) forceActiveFocus(Qt.OtherFocusReason)
    Component.onCompleted: if (selected && active) forceActiveFocus(Qt.OtherFocusReason)

    Item {
        id: book
        z: 1
        x: (root.width-width)/2; y: root.coverHeight-height + (root.selected ? -3 : 0)
        width: root.bookWidth; height: root.bookHeight
        scale: root.selected ? 1.035 : 1
        transformOrigin: Item.Bottom
        Behavior on scale { NumberAnimation { duration: Theme.motion(140) } }
        Behavior on y { NumberAnimation { duration: Theme.motion(140) } }
        Rectangle { x: 3; y: 5; width: parent.width+2; height: parent.height; radius: 3; color: "#21152b27" }
        Rectangle { x: 2; y: 3; width: parent.width+1; height: parent.height; radius: 3; color: "#25152b27" }
        Rectangle { anchors.fill: parent; anchors.margins: -4; radius: 6; color: "transparent"; border.width: 6; border.color: Qt.alpha(Theme.focusGlow,0.12); visible: root.selected }
        Rectangle { anchors.fill: parent; anchors.margins: -2; radius: 4; color: "transparent"; border.width: 1.5; border.color: Theme.focus; visible: root.selected }
        Rectangle { anchors.fill: parent; radius: 2; color: Theme.chassisDark }
        Image {
            id: fallback; anchors.fill: parent
            visible: !root.realCover
            source: root.visible && !root.realCover ? root.entry.fallbackCover || "qrc:/cover-fallbacks/neutral-0.png" : ""
            sourceSize.width: 280; sourceSize.height: 420
            fillMode: Image.PreserveAspectCrop; asynchronous: true; mipmap: true
        }
        Image {
            id: art; objectName: "shelf-cover-art"
            anchors.fill: parent
            source: root.visible ? root.entry.cover || "" : ""
            sourceSize.width: 420; sourceSize.height: 420
            fillMode: Image.PreserveAspectFit; asynchronous: true; mipmap: true
        }
        Rectangle {
            anchors.fill: parent; visible: !root.realCover
            gradient: Gradient {
                GradientStop { position: 0.4; color: "transparent" }
                GradientStop { position: 1; color: "#bf182d35" }
            }
        }
        Text {
            visible: !root.realCover
            x: 8; y: parent.height*0.56; width: parent.width-16; height: parent.height*0.31
            text: root.entry.title || ""; textFormat: Text.PlainText
            color: "#fff6dc"; font.family: Theme.displayFamily; font.pixelSize: 17
            fontSizeMode: Text.Fit; minimumPixelSize: 10; font.bold: true
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight
        }
        Item {
            visible: !root.realCover; anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom; anchors.bottomMargin: 3; width: 26; height: 16
            PlatformBadge {
                scale: 0.36; transformOrigin: Item.TopLeft; frameless: true; glyphColor: "#f5e9c8"
                label: root.entry.platformShort || ""; shape: root.entry.platformShape || "console"
            }
        }
        // The cover is a small physical case, not a button tile.
        Rectangle { x: 0; y: 0; width: 4; height: parent.height; color: "#40172c29" }
        Rectangle { x: 4; y: 1; width: 1; height: parent.height-2; color: "#40ffffff" }
        Rectangle { x: 1; y: 0; width: parent.width-2; height: 1; color: "#85ffffff" }
        Rectangle { anchors.fill: parent; radius: 2; color: "transparent"; border.width: 0.6; border.color: "#60182c2a" }
        Text { visible: root.entry.linked === false; anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 4; text: "!"; font.bold: true; color: Theme.yellow; font.pixelSize: 20 }
        FavoriteStar {
            objectName: "shelf-favorite-"+(root.entry.id || "")
            anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 2
            marked: root.entry.favorite === true; active: root.active
            onActivated: root.favoriteRequested(root.entry.id)
        }
    }
    Item {
        id: caption; x: 7; y: root.coverHeight+16; width: root.width-14; height: 22; clip: true
        readonly property real travel: Math.max(0, label.implicitWidth-width)
        readonly property bool moving: root.selected && root.active && root.visible && !Theme.reducedMotion && Qt.application.state === Qt.ApplicationActive && travel > 1
        function restart() { ticker.stop();label.x=0;if(moving)ticker.start() }
        onMovingChanged: restart()
        onTravelChanged: restart()
        Text {
            id: label; width: caption.moving ? implicitWidth : caption.width; height: parent.height
            text: root.entry.title || ""; textFormat: Text.PlainText
            font.family: Theme.displayFamily; font.pixelSize: 14; font.weight: root.selected ? Font.DemiBold : Font.Normal
            color: root.selected ? Theme.ink : Theme.muted
            horizontalAlignment: caption.moving ? Text.AlignLeft : Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter; elide: caption.moving ? Text.ElideNone : Text.ElideRight
        }
        SequentialAnimation {
            id: ticker; loops: Animation.Infinite
            PauseAnimation { duration: 3500 }
            NumberAnimation { target: label; property: "x"; to: -caption.travel; duration: Math.max(800,caption.travel/30*1000) }
            PauseAnimation { duration: 1500 }
            PropertyAction { target: label; property: "x"; value: 0 }
        }
    }
    MouseArea {
        anchors.fill: parent; enabled: root.active
        // ListView steals a vertical drag; only a real tap activates a game.
        onClicked: root.activated()
    }
}
