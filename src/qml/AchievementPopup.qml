import QtQuick

Item {
    id: root
    required property var shell
    readonly property var detail: shell.hall.detail
    readonly property bool earned: detail.earnedState === "earned"
    readonly property bool unknown: detail.earnedState === "unknown"
    readonly property color metal: earned ? Theme.yellow : Theme.rim
    objectName: "achievement-popup"
    onVisibleChanged: if(visible)description.contentY=0
    function close() { shell.pressButton("B") }
    Rectangle { anchors.fill: parent; color: Qt.alpha(Theme.ink,.32) }
    MouseArea { anchors.fill: parent; onClicked: root.close() }
    Item {
        id: card
        objectName: "achievement-popup-card"
        width: Math.min(650,parent.width*.82); height: Math.min(300,parent.height*.85)
        anchors.centerIn: parent
        Rectangle { x: 3; y: 7; width: parent.width; height: parent.height; radius: 21; color: Theme.surfaceColor("#40302232") }
        Rectangle {
            anchors.fill: parent; radius: 20; border.color: Theme.rim; border.width: 2
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.lighter(Theme.paper,1.04) }
                GradientStop { position: 1; color: Qt.tint(Theme.paper,Qt.alpha(Theme.rim,.12)) }
            }
        }
        Rectangle { anchors.fill: parent; anchors.margins: 4; radius: 16; color: "transparent"; border.color: Theme.surfaceColor("#b3ffffff") }
        MouseArea { anchors.fill: parent }
        Text {
            x: 22; y: 15; width: parent.width-76; height: 29
            text: root.detail.title || "Achievement"; textFormat: Text.PlainText
            font.family: Theme.displayFamily; font.pixelSize: 23; fontSizeMode: Text.Fit; minimumPixelSize: 15
            elide: Text.ElideRight; color: Theme.ink
        }
        Text {
            x: 23; y: 45; width: parent.width-70; height: 19
            text: root.detail.world || "RetroAchievements"; textFormat: Text.PlainText; elide: Text.ElideRight
            color: Theme.muted; font.pixelSize: 12
        }
        Text {
            anchors.right: parent.right; anchors.rightMargin: 15; y: 13; width: 30; height: 32
            text: "×"; color: Theme.muted; font.pixelSize: 28; horizontalAlignment: Text.AlignHCenter
            MouseArea { anchors.fill: parent; onClicked: root.close() }
        }
        Item {
            id: photo
            x: 24; y: 82; width: card.width*.38; height: card.height-111; rotation: -2
            Rectangle { x: 2; y: 4; width: parent.width; height: parent.height; radius: 8; color: Theme.surfaceColor("#30263830") }
            Rectangle { anchors.fill: parent; radius: 7; color: Theme.paper; border.color: Qt.alpha(Theme.rim,.6) }
            Rectangle {
                anchors.fill: parent; anchors.margins: 7; anchors.bottomMargin: 27; radius: 3
                color: Qt.tint(Theme.paper,Qt.alpha(Theme.rim,.18))
                // No exit photo or unrelated game art may masquerade as an award capture.
                Item {
                    visible: !root.detail.moment
                    anchors.centerIn: parent; width: 46; height: 33; opacity: .45
                    Rectangle { x: 8; y: -4; width: 14; height: 6; radius: 2; color: Theme.muted }
                    Rectangle { anchors.fill: parent; radius: 6; color: "transparent"; border.width: 2; border.color: Theme.muted }
                    Rectangle { anchors.centerIn: parent; width: 18; height: 18; radius: 9; color: "transparent"; border.width: 2; border.color: Theme.muted }
                    Rectangle { x: 36; y: 7; width: 3; height: 3; radius: 1.5; color: Theme.muted }
                }
                Image {
                    anchors.fill: parent;source: root.visible ? root.detail.moment || "" : ""
                    asynchronous: true;fillMode: Image.PreserveAspectFit
                }
            }
            Text {
                anchors.bottom: parent.bottom; anchors.bottomMargin: 7; width: parent.width
                text: root.detail.moment ? "A · View moment" : root.detail.expired ? "Photo left the recent buffer" : "No captured moment"; horizontalAlignment: Text.AlignHCenter
                color: Theme.muted; font.pixelSize: 11
            }
            MouseArea { anchors.fill: parent; enabled: !!root.detail.moment; onClicked: root.shell.hall.openMoment() }
        }
        Item {
            id: info
            x: photo.x+photo.width+25; y: 80; width: card.width-x-24; height: card.height-y-20
            Item {
                id: medal
                width: Math.min(90,info.height*.51); height: width
                Rectangle { x: 2; y: 4; width: parent.width; height: parent.height; radius: 19; color: Theme.surfaceColor("#35263830") }
                Rectangle {
                    anchors.fill: parent; radius: 18; border.width: 1; border.color: Qt.darker(root.metal,1.3)
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.lighter(root.metal,1.35) }
                        GradientStop { position: .55; color: root.metal }
                        GradientStop { position: 1; color: Qt.darker(root.metal,1.25) }
                    }
                }
                Rectangle { anchors.fill: parent; anchors.margins: 3; radius: 15; color: "transparent"; border.color: Theme.surfaceColor("#cffff8df") }
                Image {
                    id: badge; anchors.fill: parent; anchors.margins: 8
                    source: root.visible ? root.detail.icon || "" : ""; sourceSize: Qt.size(192,192)
                    fillMode: Image.PreserveAspectFit; asynchronous: true
                }
                Text {
                    anchors.centerIn: parent; visible: badge.status!==Image.Ready
                    text: root.earned ? "★" : root.unknown ? "?" : "◇"; color: Theme.accentInk; font.pixelSize: 38
                }
            }
            Column {
                x: medal.width+14; width: parent.width-x; anchors.verticalCenter: medal.verticalCenter; spacing: 5
                Text {
                    width: parent.width; text: root.earned ? "✓  Earned" : root.unknown ? "?  Unconfirmed" : "Not earned yet"
                    wrapMode: Text.WordWrap; font.family: Theme.displayFamily; font.pixelSize: 16; color: Theme.ink
                }
                Text { width: parent.width; visible: text.length>0; text: (root.detail.mode || "") + (root.detail.points >= 0 ? " · " + root.detail.points + " pts" : ""); color: Theme.muted; font.pixelSize: 12 }
                Text { width: parent.width; visible: text.length>0; text: root.detail.earnedDate || ""; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 11 }
            }
            Rectangle { y: medal.height+12; width: parent.width; height: 1; color: Qt.alpha(Theme.rim,.4) }
            Flickable {
                id: description; y: medal.height+21; width: parent.width; height: parent.height-y
                clip: true; contentHeight: descriptionText.height; boundsBehavior: Flickable.StopAtBounds
                Text {
                    id: descriptionText; width: parent.width
                    text: root.detail.description || ""; textFormat: Text.PlainText
                    color: Theme.ink; font.pixelSize: 13; wrapMode: Text.WordWrap
                }
                Connections {
                    target: root.shell.hall
                    function onDetailScrollRequested(delta) {
                        if(root.visible)description.contentY=Math.max(0,Math.min(description.contentHeight-description.height,description.contentY+delta))
                    }
                }
            }
        }
    }
}
