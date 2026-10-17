import QtQuick

Item {
    id: root
    required property var social
    objectName: "my-lobbies-popup"
    visible: social.myLobbiesOpen
    Rectangle { anchors.fill: parent; color: "#70152728" }
    MouseArea { anchors.fill: parent; onClicked: root.social.closeMyLobbies(); onWheel: wheel => wheel.accepted=true }
    Panel {
        id: surface; anchors.centerIn: parent
        width: Math.min(310,parent.width-36); height: Math.min(410,parent.height-12)
        MouseArea { anchors.fill: parent; onWheel: wheel => wheel.accepted=true }
        Text { x: 20; y: 16; text: "My Lobbies"; font.family: Theme.displayFamily; font.pixelSize: 23; color: Theme.ink }
        Text { objectName: "my-lobbies-close"; anchors.right: parent.right; anchors.rightMargin: 16; y: 12; width: 28; height: 30; text: "×"; color: Theme.muted; font.pixelSize: 28; horizontalAlignment: Text.AlignHCenter
            MouseArea { anchors.fill: parent; anchors.margins: -3; onClicked: root.social.closeMyLobbies() }
        }
        Text { x: 20; y: 63; text: root.social.myLobbies.length+(root.social.myLobbies.length===1?" room":" rooms"); color: Theme.muted; font.pixelSize: 12 }
        CapButton { objectName: "my-lobbies-create"; anchors.right: parent.right; anchors.rightMargin: 18; y: 52; width: 166; height: 34; label: "+  Create lobby"; textSize: 13; tint: Theme.green; claimsFocus: false; selected: root.social.myLobbiesIndex===rooms.count; onActivated: root.social.createAnotherLobby() }
        ListView {
            id: rooms; objectName: "my-lobbies-list"; x: 17; y: 99; width: parent.width-34; height: parent.height-y-17
            clip: true; spacing: 8; model: root.social.myLobbies; currentIndex: root.social.myLobbiesIndex
            onCurrentIndexChanged: if(currentIndex<count)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Item {
                required property var modelData; required property int index
                width: rooms.width; height: game.platform || game.mode ? Math.max(120,Math.min(136,Math.floor((rooms.height-8)/2))) : 108
                readonly property var game: modelData.game || ({})
                Rectangle { x: 2; y: 4; width: parent.width-4; height: parent.height-5; radius: 15; color: Theme.chassisDark; opacity: .18 }
                Rectangle { anchors.fill: parent; anchors.bottomMargin: 4; radius: 14
                    gradient: Gradient { GradientStop { position: 0; color: Qt.lighter(Theme.paper,1.04) } GradientStop { position: 1; color: Qt.tint(Theme.paper,"#247cb4a9") } }
                    border.width: index===root.social.myLobbiesIndex?2:1; border.color: index===root.social.myLobbiesIndex?Theme.focusGlow:Theme.rim
                }
                Text { x: 14; y: 10; width: parent.width-28; text: modelData.title; font.family: Theme.displayFamily; font.pixelSize: 20; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { x: 14; y: 35; width: parent.width-28; text: modelData.gameTitle || "Choosing Together"; font.pixelSize: 14; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { x: 14; y: 53; width: parent.width-28; text: [game.platform || "",game.mode ? modelData.modeLabel : ""].filter(v => v.length).join("  \u00b7  "); font.pixelSize: 11; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { x: 14; y: game.platform || game.mode ? 69 : 55; text: game.seats ? modelData.occupied+" / "+game.seats+" players" : modelData.people.length+(modelData.people.length===1?" person":" people"); font.pixelSize: 11; color: Theme.muted }
                Text { anchors.right: parent.right; anchors.rightMargin: 14; y: game.platform || game.mode ? 69 : 55; text: modelData.startsAt ? Qt.formatDateTime(new Date(modelData.startsAt*1000),"ddd HH:mm") : ""; font.pixelSize: 11; color: Theme.muted }
                Rectangle { x: 14; y: parent.height-36; width: parent.width-28; height: 1; color: Theme.rim; opacity: .35 }
                Row { x: 14; y: parent.height-31; spacing: -5
                    Repeater { model: modelData.people.slice(0,6)
                        Rectangle { required property var modelData; required property int index; width: 24; height: 24; radius: 12; color: [Theme.blue,Theme.pink,Theme.green,Theme.yellow][index%4]; border.width: 2; border.color: Theme.paper
                            Text { anchors.centerIn: parent; text: (modelData.name || "?").substring(0,1); font.pixelSize: 12; color: Theme.ink }
                            Image { anchors.fill: parent; anchors.margins: 3; source: modelData.avatar || ""; asynchronous: true; fillMode: Image.PreserveAspectFit }
                        }
                    }
                }
                Text { anchors.right: parent.right; anchors.rightMargin: 14; y: parent.height-28; width: parent.width*.4; horizontalAlignment: Text.AlignRight; text: modelData.hostName+"  \u203a"; font.pixelSize: 12; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
                MouseArea { anchors.fill: parent; onClicked: root.social.chooseMyLobby(modelData.id) }
            }
        }
        Text { anchors.centerIn: rooms; width: rooms.width-30; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; visible: !rooms.count; text: "A new gathering starts here"; color: Theme.muted; font.family: Theme.displayFamily; font.pixelSize: 20 }
    }
}
