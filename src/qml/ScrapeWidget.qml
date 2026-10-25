import QtQuick
import QtQuick.Controls

Item {
    id: root
    required property var flow
    property bool interactive: true
    property bool controllerFocus: false
    property bool embedded: false
    readonly property bool compact: height<250
    readonly property var tasks: flow.tasks
    readonly property var current: tasks.find(t=>t.state==="running") || tasks.find(t=>t.state==="queued"||t.state==="paused") || ({})
    readonly property var attention: tasks.filter(t=>t.state==="attention"||t.state==="failed")
    readonly property int remaining: tasks.filter(t=>t.state==="running"||t.state==="queued"||t.state==="paused").length
    readonly property int finished: tasks.filter(t=>t.state==="done").length
    readonly property bool paused: flow.paused===true
    readonly property bool wide: width>=380
    component ActionButton: Rectangle {
        property string label: ""
        property bool selected: false
        signal activated()
        radius: 6; color: selected?Theme.selection:"transparent"; border.width: selected?1:0; border.color: Theme.focusGlow
        Text { anchors.centerIn: parent; text: parent.label; color: Theme.ink; font.pixelSize: 12 }
        MouseArea { anchors.fill: parent; onClicked: parent.activated() }
    }
    Text { id: title; text: "ScreenScraper"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 20 }
    Text { y: 29; width: parent.width; text: root.remaining?root.remaining+" remaining · "+root.finished+" finished":root.attention.length?root.attention.length+" need your choice":root.finished?root.finished+" games updated":"Artwork and information for your library"; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
    Item { id: currentCard; y: root.compact?50:60; width: root.wide?parent.width*.48:parent.width; height: parent.height-y-40; clip: true
        Image { id: cover; x: 0; y: 4; width: Math.min(110,parent.width*.35); height: Math.max(20,Math.min(135,parent.height-10)); source: root.current.picture||""; asynchronous: true; fillMode: Image.PreserveAspectFit }
        MenuGlyph { anchors.centerIn: cover; width: 40; height: 40; glyph: "pictures"; ink: Theme.muted; visible: cover.status!==Image.Ready }
        Column { x: cover.width+14; width: parent.width-x; spacing: root.compact?5:9
            Text { width: parent.width; text: root.current.title|| (root.flow.busy?"Review when ready":"Ready for your library"); textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: root.compact?13:16; wrapMode: Text.Wrap; maximumLineCount: root.compact?2:3; elide: Text.ElideRight }
            Text { width: parent.width; visible: !root.compact; text: root.current.platform||""; color: Theme.muted; font.pixelSize: 11 }
            Text { width: parent.width; text: root.current.detail||root.flow.status||"Fetch missing artwork and game information using your saved settings."; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 11; wrapMode: Text.Wrap; maximumLineCount: root.compact?2:4; elide: Text.ElideRight }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: Theme.divider
            Rectangle { height: 2; width: root.tasks.length?parent.width*root.finished/root.tasks.length:0; color: Theme.accent }
        }
    }
    Rectangle { x: parent.width*.51; y: currentCard.y; width: 1; height: currentCard.height; color: Theme.divider; visible: root.wide }
    ListView { id: choices; objectName: "scraper-review-cards"; x: parent.width*.54; y: currentCard.y; width: parent.width-x; height: currentCard.height; visible: root.wide
        clip: true; spacing: 8; model: root.attention; currentIndex: root.controllerFocus?root.flow.overviewFocus-3:-1; keyNavigationEnabled: false
        onCurrentIndexChanged: if(currentIndex>=0)Qt.callLater(()=>positionViewAtIndex(currentIndex,ListView.Contain))
        ScrollBar.vertical: ScrollBar { width: 3 }
        Text { anchors.centerIn: parent; width: parent.width; visible: !choices.count; text: "Games that need a choice\nwill appear here.\nThe rest keep downloading."; color: Theme.muted; horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5; font.pixelSize: 12 }
        delegate: Rectangle { required property var modelData; required property int index; objectName: "scraper-review-"+index; width: choices.width-6; height: 70; radius: 7
            color: root.controllerFocus&&choices.currentIndex===index?Theme.selection:"transparent"; border.width: 1; border.color: root.controllerFocus&&choices.currentIndex===index?Theme.focusGlow:Theme.divider
            Image { x: 7; y: 9; width: 40; height: 52; source: modelData.picture||""; asynchronous: true; fillMode: Image.PreserveAspectFit }
            Text { x: 57; y: 10; width: parent.width-x-9; text: modelData.title; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 12; elide: Text.ElideRight }
            Text { x: 57; y: 31; width: parent.width-x-9; text: modelData.state==="attention"?"Choose game ›":modelData.detail; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 10; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
            MouseArea { anchors.fill: parent; enabled: root.interactive; onClicked: root.flow.overviewAction(index+3) }
        }
    }
    Row { anchors.bottom: parent.bottom; spacing: 5
        Repeater { model: [root.flow.busy?(root.paused?"Resume":"Pause"):"Find missing",root.flow.busy?"Stop":"Retry errors",root.embedded?"Open":"Close"]
            ActionButton { required property var modelData; required property int index; width: Math.min(126,(root.width-10)/3); height: 32; label: modelData; selected: root.controllerFocus&&root.flow.overviewFocus===index; enabled: root.interactive&&(index!==1||root.flow.busy||root.attention.some(t=>t.state==="failed")); opacity: enabled?1:.4; onActivated: {if(root.embedded&&index===2)root.flow.showOverview();else root.flow.overviewAction(index)} }
        }
    }
}
