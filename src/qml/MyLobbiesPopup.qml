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
        width: Math.min(520,parent.width-54); height: Math.min(116+Math.max(1,Math.min(2,rooms.count))*96+(rooms.count>1?8:0),parent.height-26)
        MouseArea { anchors.fill: parent; onWheel: wheel => wheel.accepted=true }
        Text { x: 22; y: 15; text: "My Lobbies"; font.family: Theme.displayFamily; font.pixelSize: 26; color: Theme.ink }
        Text { anchors.right: parent.right; anchors.rightMargin: 25; y: 21; text: root.social.myLobbies.length+(root.social.myLobbies.length===1?" room":" rooms"); color: Theme.muted; font.pixelSize: 13 }
        ListView {
            id: rooms; objectName: "my-lobbies-list"; x: 20; y: 56; width: parent.width-40; height: parent.height-y-61
            clip: true; spacing: 8; model: root.social.myLobbies; currentIndex: root.social.myLobbiesIndex
            onCurrentIndexChanged: if(currentIndex<count)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Item {
                required property var modelData; required property int index
                width: rooms.width; height: 96
                readonly property var game: modelData.game || ({})
                Rectangle { x: 2; y: 4; width: parent.width-4; height: parent.height-5; radius: 15; color: Theme.chassisDark; opacity: .18 }
                Rectangle { anchors.fill: parent; anchors.bottomMargin: 4; radius: 14
                    gradient: Gradient { GradientStop { position: 0; color: Qt.lighter(Theme.paper,1.04) } GradientStop { position: 1; color: Qt.tint(Theme.paper,"#247cb4a9") } }
                    border.width: index===root.social.myLobbiesIndex?2:1; border.color: index===root.social.myLobbiesIndex?Theme.focusGlow:Theme.rim
                }
                Rectangle { x: 13; y: 13; width: 5; height: 66; radius: 2.5; color: index%2?Theme.blue:Theme.green }
                Text { x: 29; y: 11; width: parent.width-55; text: modelData.title; font.family: Theme.displayFamily; font.pixelSize: 21; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { x: 29; y: 36; width: parent.width-55; text: modelData.gameTitle || "Choosing Together"; font.pixelSize: 15; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { x: 29; y: 56; width: parent.width-55; text: (game.platform ? game.platform+"  \u00b7  " : "")+(game.mode ? modelData.modeLabel+"  \u00b7  " : "")+(game.seats ? modelData.occupied+" / "+game.seats+" players" : modelData.people.length+(modelData.people.length===1?" person":" people"))+(modelData.startsAt ? "  \u00b7  "+Qt.formatDateTime(new Date(modelData.startsAt*1000),"ddd HH:mm") : ""); font.pixelSize: 11; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
                Row { x: 29; y: 69; spacing: -4
                    Repeater { model: modelData.people.slice(0,8)
                        Rectangle { required property var modelData; required property int index; width: 21; height: 21; radius: 10.5; color: [Theme.blue,Theme.pink,Theme.green,Theme.yellow][index%4]; border.width: 2; border.color: Theme.paper
                            Text { anchors.centerIn: parent; text: (modelData.name || "?").substring(0,1); font.pixelSize: 12; color: Theme.ink }
                            Image { anchors.fill: parent; anchors.margins: 3; source: modelData.avatar || ""; asynchronous: true; fillMode: Image.PreserveAspectFit }
                        }
                    }
                }
                Text { anchors.right: parent.right; anchors.rightMargin: 24; y: 73; width: parent.width*.45; horizontalAlignment: Text.AlignRight; text: modelData.hostName+"  \u203a"; font.pixelSize: 12; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
                MouseArea { anchors.fill: parent; onClicked: root.social.chooseMyLobby(modelData.id) }
            }
        }
        Text { anchors.centerIn: rooms; visible: !rooms.count; text: "A new gathering starts here"; color: Theme.muted; font.family: Theme.displayFamily; font.pixelSize: 20 }
        Row { anchors.bottom: parent.bottom; anchors.bottomMargin: 15; anchors.right: parent.right; anchors.rightMargin: 21; spacing: 13
            CapButton { width: 100; height: 30; label: "Back"; textSize: 13; tint: Theme.blue; centered: true; claimsFocus: false; onActivated: root.social.closeMyLobbies() }
            CapButton { objectName: "my-lobbies-create"; width: 167; height: 30; label: "+ Create lobby"; textSize: 13; tint: Theme.green; centered: true; claimsFocus: false; selected: root.social.myLobbiesIndex===rooms.count; onActivated: root.social.createAnotherLobby() }
        }
    }
}
