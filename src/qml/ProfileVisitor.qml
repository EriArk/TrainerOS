import QtQuick

Item {
    id: root
    required property var shell
    readonly property var visitor: shell.visitorProfile
    readonly property var card: visitor.data
    readonly property var visibility: card.visibility||({})
    readonly property bool sharing: shell.profileRoute==="privacy"
    readonly property bool active: shell.profileContentActive
    readonly property var history: card.history||[]
    readonly property bool hasProfile: !!card.player_id
    readonly property bool compact: height<440
    component Link: Item {
        required property int actionIndex
        required property string label
        property bool available: true
        width: 108; height: 34; opacity: available?1:.4
        Rectangle { anchors.fill: parent; radius: 5; color: root.shell.profileFocus===parent.actionIndex?Theme.selection:"transparent"; border.color: Theme.focus; border.width: root.shell.profileFocus===parent.actionIndex?2:0 }
        Text { anchors.centerIn: parent; text: parent.label; color: Theme.ink; font.pixelSize: 11 }
        MouseArea { anchors.fill: parent; enabled: root.active&&parent.available; onClicked: root.shell.activate(parent.actionIndex,"profile") }
        Accessible.role: Accessible.Button; Accessible.name: label
        Accessible.onPressAction: if(available)root.shell.activate(actionIndex,"profile")
    }
    Item {
        anchors.fill: parent; visible: !root.sharing
        ProfileScene {
            id: scene; width: parent.width*.36; height: parent.height-44
            profile: ({avatarPreview: root.visitor.avatarPreview, avatarRendererAvailable: root.shell.trainer.avatarRendererAvailable, profile: {emblem:"compass"}})
            appearance: ["aurora","dusk","orbit"].indexOf(root.card.appearance)>=0?root.card.appearance:""
            showAvatar: root.hasProfile
            Text { anchors.bottom: parent.bottom; anchors.bottomMargin: 14; anchors.horizontalCenter: parent.horizontalCenter; text: root.visitor.preview?"As another player sees you":"TrainerOS player"; color: "#fff8e8"; style: Text.Outline; styleColor: "#183b40"; font.pixelSize: 11 }
        }
        Flickable {
            id: facts; x: scene.width+22; width: parent.width-x; height: parent.height-44
            contentWidth: width; contentHeight: content.implicitHeight; clip: true; boundsBehavior: Flickable.StopAtBounds
            Connections { target: root.shell; function onChanged() {
                if(root.shell.profileFocus<100)return
                const row=historyRows.itemAt(root.shell.profileFocus-100);if(!row)return
                const y=row.mapToItem(content,0,0).y
                if(y<facts.contentY)facts.contentY=y
                else if(y+row.height>facts.contentY+facts.height)facts.contentY=y+row.height-facts.height
            } }
            Column {
                id: content; width: facts.width; spacing: root.compact?10:16
                Text { width: parent.width; text: root.card.display_name||"Player profile"; textFormat: Text.PlainText; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: root.compact?28:36; elide: Text.ElideRight }
                Text { width: parent.width; text: root.hasProfile?(root.visibility.level?"Level "+root.card.level+" · Trainer Mastery":"Account level is private"):root.visitor.status; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.Wrap }
                Text { width: parent.width; visible: root.hasProfile&&!!root.visibility.activity&&!!text; text: root.shell.social.person.id===root.card.subject?(root.shell.social.person.status||""):""; color: Theme.muted; font.pixelSize: 11; textFormat: Text.PlainText; wrapMode: Text.Wrap }
                Column { width: parent.width; spacing: 5; visible: !!root.visibility.favorites&&(root.card.favorites||[]).length>0
                    Text { text: "Favorite games"; color: Theme.ink; font.pixelSize: 16 }
                    Text { width: parent.width; text: (root.card.favorites||[]).join("  ·  "); textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.Wrap }
                }
                Column { width: parent.width; spacing: 5; visible: (root.card.achievements||[]).length>0
                    Text { text: "Recent achievements"; color: Theme.ink; font.pixelSize: 16 }
                    Repeater { model: root.card.achievements||[]
                        Text { required property var modelData; width: parent.width; text: "✦  "+modelData.title+" · "+(modelData.earned_at||"").substring(0,10); textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
                    }
                }
                Column {
                    width: parent.width; spacing: 8; visible: root.hasProfile
                    Text { text: "Trophy shelf"; color: Theme.ink; font.pixelSize: 16 }
                    Text { visible: !root.visibility.shelf; text: "This shelf is private"; color: Theme.muted; font.pixelSize: 12 }
                    Row { width: parent.width; spacing: 8; visible: !!root.visibility.shelf
                        Repeater { model: 5
                            Column {
                                required property int index
                                readonly property var trophy: (root.card.shelf||[])[index]||({})
                                width: (parent.width-32)/5; spacing: 4
                                Item { width: parent.width; height: root.compact?44:60
                                    PrizeEmblem { anchors.centerIn: parent; width: parent.height; height: width; visible: !!parent.parent.trophy.game_id; kind: parent.parent.trophy.hardcore?"hardcore":parent.parent.trophy.completed?"completed":"beaten" }
                                    Text { anchors.centerIn: parent; visible: !parent.parent.trophy.game_id; text: "·"; font.pixelSize: 18; color: Theme.muted }
                                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
                                }
                                Text { width: parent.width; text: trophy.title||"Empty"; font.pixelSize: 10; color: trophy.game_id?Theme.ink:Theme.muted; textFormat: Text.PlainText; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
                            }
                        }
                    }
                }
                Column {
                    id: collection; width: parent.width; spacing: 4; visible: root.hasProfile
                    Text { text: "Hall of fame"; color: Theme.ink; font.pixelSize: 16; height: 26 }
                    Text { visible: !root.visibility.history||!root.history.length; text: !root.visibility.history?"Game history is private":"No completed games shared yet"; color: Theme.muted; font.pixelSize: 12 }
                    Repeater { id: historyRows; model: root.history
                        Item { required property var modelData; required property int index; width: collection.width; height: 50
                            Rectangle { anchors.fill: parent; radius: 5; color: root.shell.profileFocus===100+index?Theme.selection:"transparent" }
                            Text { x: 4; y: 4; width: parent.width-8; text: modelData.title; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 13; elide: Text.ElideRight }
                            Text { x: 4; y: 25; width: parent.width-8; text: (modelData.completed?"All achievements":"Game beaten")+(modelData.hardcore?" · Hardcore":"")+" · "+(modelData.earned_at||"").substring(0,10); color: Theme.muted; font.pixelSize: 10; elide: Text.ElideRight }
                        }
                    }
                }
            }
        }
        Row { anchors.bottom: parent.bottom; anchors.right: parent.right; spacing: 6; visible: root.hasProfile
            Link { actionIndex: 3; label: "Latest / Refresh"; available: !root.visitor.busy }
            Link { actionIndex: 4; label: "Earlier games"; available: !!root.card.next&&!root.visitor.busy }
        }
        Row { anchors.bottom: parent.bottom; spacing: 6
            Link { actionIndex: 0; label: "Message"; visible: !root.visitor.preview }
            Link { actionIndex: 1; label: "Invite to lobby"; visible: !root.visitor.preview }
            Link { actionIndex: 2; label: "Social options"; visible: !root.visitor.preview }
            Link { actionIndex: 5; label: "Sign in"; visible: !root.shell.profileAccount.signedIn }
            Link { actionIndex: 3; label: "Try again"; visible: !root.hasProfile; available: !root.visitor.busy }
        }
    }
    Flickable {
        id: privacyScroll; visible: root.sharing; width: Math.min(620,parent.width); height: parent.height; anchors.horizontalCenter: parent.horizontalCenter
        contentHeight: privacyContent.implicitHeight; contentWidth: width; clip: true; boundsBehavior: Flickable.StopAtBounds
        Connections { target: root.shell; function onChanged() {
            if(!root.sharing)return
            const focus=root.shell.profileFocus
            if(focus===4||focus===5||focus===6)privacyScroll.contentY=Math.max(0,privacyScroll.contentHeight-privacyScroll.height)
            else if(focus<4)privacyScroll.contentY=0
            else if(focus===7)privacyScroll.contentY=Math.min(Math.max(0,privacyScroll.contentHeight-privacyScroll.height),100)
        } }
    Column {
        id: privacyContent; width: parent.width; spacing: 12
        Text { text: "Your public profile"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 30 }
        Text { width: parent.width; text: "Choose what other signed-in players can see. Your name and avatar identify you; Sparks and purchases stay private."; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.Wrap }
        Repeater { model: [{key:"level",title:"Account level"},{key:"shelf",title:"Trophy shelf"},{key:"history",title:"Completed games and achievements"},{key:"activity",title:"Activity in the profile"},{key:"favorites",title:"Favorite games"}]
            Item {
                required property var modelData; required property int index; width: parent.width; height: root.compact?42:52
                readonly property int actionIndex: index===4?7:index
                Rectangle { anchors.fill: parent; radius: 6; color: root.shell.profileFocus===parent.actionIndex?Theme.selection:"transparent"; border.color: Theme.focus; border.width: root.shell.profileFocus===parent.actionIndex?2:0 }
                Text { x: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.title; color: Theme.ink; font.pixelSize: 13 }
                Text { anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; text: root.visitor.sharing.revision===undefined?"—":root.visitor.sharing[modelData.key]?"Visible to players  ✓":"Only me"; color: Theme.muted; font.pixelSize: 12 }
                MouseArea { anchors.fill: parent; enabled: root.active&&!root.visitor.busy; onClicked: root.shell.activate(parent.actionIndex,"profile") }
                Accessible.role: Accessible.CheckBox; Accessible.name: modelData.title; Accessible.checked: !!root.visitor.sharing[modelData.key]
                Accessible.onPressAction: root.shell.activate(actionIndex,"profile")
            }
        }
        Text { width: parent.width; text: "Activity sharing here does not change your Fluxer presence settings."; color: Theme.muted; font.pixelSize: 10; wrapMode: Text.Wrap }
        Text { width: parent.width; text: "Library selection (up to five): "+(root.shell.profileFavoriteTitles().join(" · ")||"No favorites selected"); textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 11; wrapMode: Text.Wrap }
        Link { actionIndex: 6; label: "Publish this selection"; width: 170; available: !root.visitor.busy&&root.visitor.sharing.revision!==undefined }
        Text { width: parent.width; text: root.visitor.status; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.Wrap }
        Row { spacing: 8; Link { actionIndex: 4; label: "Refresh"; available: !root.visitor.busy } Link { actionIndex: 5; label: "Sign in"; visible: !root.shell.profileAccount.signedIn } }
    }
    }
}
