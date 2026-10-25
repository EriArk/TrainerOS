import QtQuick

Item {
    id: root
    required property var social
    readonly property var view: social.lobbyPicker
    readonly property var ballot: view.vote || ({})
    readonly property var choices: ballot.choices || []
    readonly property var votes: ballot.votes || ({})
    visible: view.voteOpen
    function count(i) { return Object.values(votes).filter(v=>v===i).length }
    function isLeading(i) { for(let j=0;j<choices.length;j++)if(count(j)>count(i))return false; return true }
    Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#80152728") }
    MouseArea { anchors.fill: parent; onClicked: root.social.lobbyPickerAction("vote-close"); onWheel: wheel=>wheel.accepted=true }
    Panel {
        anchors.centerIn: parent; width: Math.min(540,parent.width-28); height: Math.min(root.choices.length>2?370:260,parent.height-12)
        MouseArea { anchors.fill: parent }
        Text { x: 24; y: 20; text: "What shall we play?"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25 }
        Text { anchors.right: parent.right; anchors.rightMargin: 22; y: 16; text: "×"; font.pixelSize: 27; color: Theme.muted; MouseArea { anchors.fill: parent; onClicked: root.social.lobbyPickerAction("vote-close") } }
        Text { x: 24; y: 54; text: "Pick a favourite · The organizer confirms the result"; color: Theme.muted; font.pixelSize: 12 }
        Grid { x: 24; y: 85; columns: 2; spacing: 10; width: parent.width-48
            Repeater { model: root.choices
                Item { required property var modelData; required property int index; width: (parent.width-10)/2; height: 97
                    Rectangle { anchors.fill: parent; radius: 12; gradient: Gradient { GradientStop { position: 0; color: [Theme.blue,Theme.green,Theme.pink,Theme.yellow][index] } GradientStop { position: 1; color: Theme.paper } }
                        border.color: root.view.voteFocus===index?Theme.focusGlow:Theme.rim; border.width: root.view.voteFocus===index?2:1 }
                    Image { x: 10; y: 10; width: 40; height: 55; source: modelData.cover || ""; fillMode: Image.PreserveAspectFit; asynchronous: true }
                    Text { x: modelData.cover?60:12; y: 11; width: parent.width-x-12; text: modelData.title; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: 19; color: Theme.ink; elide: Text.ElideRight }
                    Text { x: modelData.cover?60:12; y: 37; width: parent.width-x-12; text: modelData.platform.toUpperCase()+" · "+(modelData.modeLabel || ""); font.pixelSize: 11; color: Theme.muted; elide: Text.ElideRight }
                    Text { x: 12; y: 67; text: root.count(index)+(root.count(index)===1?" vote":" votes"); color: Theme.ink; font.pixelSize: 14 }
                    Text { anchors.right: parent.right; anchors.rightMargin: 12; y: 63; text: root.votes[root.view.self]===index?"✓":""; color: Theme.ink; font.pixelSize: 23 }
                    MouseArea { anchors.fill: parent; onClicked: root.social.lobbyAction("ballot:"+root.ballot.id+":"+index) }
                }
            }
        }
        Text { x: 24; y: parent.height-48; text: "A · Vote"; color: Theme.muted; font.pixelSize: 13 }
        CapButton { anchors.right: parent.right; anchors.rightMargin: 24; y: parent.height-56; width: 230; height: 34; visible: root.view.host; enabled: root.isLeading(root.view.voteFocus); label: "Y · Confirm selected game"; textSize: 13; tint: Theme.green; claimsFocus: false; onActivated: root.social.lobbyAction("winner:"+root.ballot.id+":"+root.view.voteFocus) }
    }
}
