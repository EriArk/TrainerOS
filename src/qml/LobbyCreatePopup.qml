import QtQuick

Item {
    id: root
    required property var social
    readonly property var form: social.lobbyDraft
    visible: social.lobbyCreateOpen
    objectName: "lobby-create-popup"
    Rectangle { anchors.fill: parent; color: "#80152728" }
    MouseArea { anchors.fill: parent; onClicked: root.social.cancelLobbyDraft(); onWheel: wheel => wheel.accepted=true }
    Panel {
        anchors.centerIn: parent; width: Math.min(450,parent.width-32); height: Math.min(390,parent.height-12)
        MouseArea { anchors.fill: parent; onWheel: wheel => wheel.accepted=true }
        Text { x: 25; y: 20; text: "Create lobby"; font.family: Theme.displayFamily; font.pixelSize: 26; color: Theme.ink }
        Text { anchors.right: parent.right; anchors.rightMargin: 20; y: 16; width: 28; height: 30; text: "×"; color: Theme.muted; font.pixelSize: 28; horizontalAlignment: Text.AlignHCenter
            MouseArea { anchors.fill: parent; onClicked: root.social.cancelLobbyDraft() }
        }
        Flickable { id: fieldsView; x: 25; y: 62; width: parent.width-50; height: parent.height-135; clip: true; contentHeight: fields.height; boundsBehavior: Flickable.StopAtBounds
            Connections { target: root.social; function onChanged() { const top=Math.min(4,root.form.focus)*52; if(top<fieldsView.contentY)fieldsView.contentY=top; else if(top+43>fieldsView.contentY+fieldsView.height)fieldsView.contentY=Math.max(0,Math.min(fieldsView.contentHeight-fieldsView.height,top+43-fieldsView.height)); } }
        Column { id: fields; width: fieldsView.width; spacing: 9
            Repeater { model: [
                {key:"name",label:"Name",value:root.form.title || "",tint:Theme.paper},
                {key:"game",label:"Game",value:root.form.gameTitle || "Choosing Together",tint:Theme.blue},
                {key:"category",label:"Category",value:root.form.category || "Other games",tint:Theme.green},
                {key:"access",label:"Access",value:root.form.accessLabel || "Invite only",tint:Theme.pink}
            ]
                Item { required property var modelData; required property int index; width: parent.width; height: 43
                    Rectangle { anchors.fill: parent; radius: 9; color: Qt.tint(Theme.paper,Qt.rgba(modelData.tint.r,modelData.tint.g,modelData.tint.b,.30)); border.color: root.form.focus===index?Theme.focusGlow:Qt.rgba(Theme.rim.r,Theme.rim.g,Theme.rim.b,.45); border.width: root.form.focus===index?2:1 }
                    Text { x: 12; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: Theme.muted; font.pixelSize: 12 }
                    Text { x: 84; width: parent.width-x-27; anchors.verticalCenter: parent.verticalCenter; text: modelData.value; textFormat: Text.PlainText; elide: Text.ElideRight; font.family: Theme.displayFamily; font.pixelSize: 18; color: Theme.ink }
                    Text { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "›"; color: Theme.muted; font.pixelSize: 21 }
                    MouseArea { anchors.fill: parent; onClicked: root.social.lobbyDraftAction(modelData.key) }
                }
            }
            Item { width: parent.width; height: 41
                Text { x: 12; anchors.verticalCenter: parent.verticalCenter; text: "When"; font.pixelSize: 12; color: Theme.muted }
                CapButton { x: 84; width: 72; height: 32; y: 4; label: "Now"; textSize: 13; centered: true; tint: root.form.startsAt?Theme.paper:Theme.yellow; claimsFocus: false; onActivated: root.social.lobbyDraftAction("now") }
                CapButton { x: 166; width: parent.width-x; height: 32; y: 4; label: root.form.startsAt?Qt.formatDateTime(new Date(root.form.startsAt*1000),"dd MMM · HH:mm"):"Schedule…"; textSize: 13; centered: true; tint: root.form.startsAt?Theme.yellow:Theme.paper; selected: root.form.focus===4; claimsFocus: false; onActivated: root.social.lobbyDraftAction("time") }
            }
        }
        }
        Text { x: 25; y: parent.height-73; width: parent.width-50; height: 20; text: root.form.error || (root.form.access==="public"?"Anyone can join this room.":root.form.access==="approval"?"You approve requests to join.":"Only people you invite can join."); font.pixelSize: 11; color: root.form.error?Theme.chassisDark:Theme.muted; elide: Text.ElideRight }
        CapButton { objectName: "lobby-create-submit"; anchors.right: parent.right; anchors.rightMargin: 25; y: parent.height-52; width: 166; height: 32; label: "Create lobby"; textSize: 14; centered: true; tint: Theme.green; selected: root.form.focus===5; claimsFocus: false; onActivated: root.social.submitLobbyDraft() }
    }
    Item { anchors.fill: parent; visible: root.social.menu.length>0
        Rectangle { anchors.fill: parent; color: "#70152728" }
        MouseArea { anchors.fill: parent; onClicked: root.social.back() }
        Panel { anchors.centerIn: parent; width: 270; height: Math.min(root.height-20,77+root.social.menu.length*34)
            MouseArea { anchors.fill: parent }
            Text { x: 23; y: 20; text: root.social.menuTitle; font.family: Theme.displayFamily; font.pixelSize: 22; color: Theme.ink }
            ListView { x: 20; y: 55; width: parent.width-40; height: parent.height-y-18; clip: true; model: root.social.menu; currentIndex: root.social.menuIndex
                onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                delegate: Item { required property string modelData; required property int index; width: ListView.view.width; height: 34
                    Rectangle { anchors.fill: parent; anchors.margins: 2; radius: 6; color: Theme.green; opacity: index===root.social.menuIndex?.55:.08 }
                    Text { x: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: Theme.ink; font.pixelSize: 14 }
                    MouseArea { anchors.fill: parent; onClicked: root.social.selectMenu(index) }
                }
            }
        }
    }
}
