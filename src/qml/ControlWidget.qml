import QtQuick

Rectangle {
    id: root
    required property var shell
    required property var modelData
    property var board: shell.controls
    readonly property string kind: modelData.id
    readonly property bool selected: board.selected === kind
    readonly property bool external: modelData.external === true
    readonly property bool slider: kind === "volume" || kind === "brightness"
    readonly property var levelData: slider ? shell.device.rows[kind === "volume" ? 0 : 1] : ({})
    readonly property int level: slider ? Number(levelData.level) : -1
    readonly property bool radio: kind === "wifi" || kind === "bluetooth" || kind === "airplane"
    readonly property var radioData: radio ? shell.device.radios[kind === "wifi" ? 0 : kind === "bluetooth" ? 1 : 2] : ({})
    readonly property color accent: kind === "downloads" ? Theme.green : kind === "brightness" || kind === "scraper" ? Theme.yellow : kind === "bluetooth" ? Theme.tabColors[4] : Theme.blue
    readonly property string icon: ({volume:"speaker",brightness:"sun",wifi:"wifi",bluetooth:"bluetooth",airplane:"airplane",downloads:"download",battery:"battery",scraper:"pictures"})[kind] || "game-controller"
    readonly property bool narrow: width < 85
    property real dragX: 0
    property real dragY: 0
    signal moved(real dx,real dy)
    radius: 12
    color: selected ? Qt.tint(Theme.paper,Theme.surfaceColor("#20edbd50")) : Qt.tint(Theme.paper,Qt.alpha(accent,.18))
    border.width: selected ? 2 : 1
    border.color: selected ? Theme.focusGlow : Qt.alpha(Theme.screenContact,.2)
    transform: Translate { x: root.dragX; y: root.dragY }
    MouseArea {
        anchors.fill: parent
        property real startX; property real startY
        onPressed: mouse => { const p=mapToItem(null,mouse.x,mouse.y); startX=p.x;startY=p.y;root.board.select(root.kind) }
        onPositionChanged: mouse => { if(pressed && root.board.editing){const p=mapToItem(null,mouse.x,mouse.y);root.dragX=p.x-startX;root.dragY=p.y-startY} }
        onReleased: {
            if(root.board.editing){if(Math.abs(root.dragX)+Math.abs(root.dragY)>8)root.moved(root.dragX,root.dragY);else root.board.select(root.kind)}
            else root.board.activate(root.kind)
            root.dragX=0;root.dragY=0
        }
        onCanceled: {root.dragX=0;root.dragY=0}
    }
    Item {
        anchors.fill: parent; anchors.margins: root.narrow?6:10
        enabled: !root.board.editing
        MenuGlyph { id: glyph; anchors.horizontalCenter: parent.horizontalCenter; y: 1; width: root.narrow?22:25; height: width; glyph: root.icon
            visible: !root.external && root.kind!=="clock" && root.kind!=="storage" && root.kind!=="downloads"
            opacity: root.radio && root.radioData.level!==1 ? .55 : 1
            MouseArea { anchors.fill: parent; anchors.margins: -4; enabled: root.kind==="volume" && root.level>=0; onClicked: root.shell.systemAction("volume") }
        }
        Text { id: label; anchors.bottom: parent.bottom; width: parent.width; text: root.modelData.title; textFormat: Text.PlainText
            font.family: Theme.displayFamily; font.pixelSize: root.narrow?11:14; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
            visible: root.kind!=="storage" && root.kind!=="downloads" && root.kind!=="clock" }
        Text { anchors.fill: parent; anchors.bottomMargin: 21; visible: root.external; text: root.modelData.text || ""; textFormat: Text.PlainText
            color: Theme.ink; font.pixelSize: 14; wrapMode: Text.Wrap; elide: Text.ElideRight; maximumLineCount: 3; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
        Item { id: bar; visible: root.slider; x: (parent.width-width)/2; y: 31; width: root.narrow?24:parent.width-12; height: root.narrow?Math.max(18,parent.height-78):16
            Rectangle { anchors.fill: parent; radius: 6; color: Qt.alpha(Theme.screenContact,.13) }
            Rectangle { anchors.bottom: parent.bottom; width: root.narrow?parent.width:parent.width*Math.max(0,Math.min(100,root.level))/100
                height: root.narrow?parent.height*Math.max(0,Math.min(100,root.level))/100:parent.height; radius: 6; color: root.accent }
            Rectangle { visible: root.board.adjusting && root.selected; anchors.fill: parent; radius: 6; color: "transparent"; border.color: Theme.focusGlow; border.width: 2 }
            MouseArea { anchors.fill: parent; anchors.margins: -4; enabled: root.level>=0
                function setLevel(mouse){ const v=root.narrow?1-(mouse.y-4)/bar.height:(mouse.x-4)/bar.width;root.shell.device.setQuickLevel(root.kind==="volume"?0:1,Math.round(v*100)) }
                onPressed: mouse => setLevel(mouse); onPositionChanged: mouse => {if(pressed)setLevel(mouse)}
            }
        }
        Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: label.top; anchors.bottomMargin: 5
            visible: root.slider; text: root.level<0?"?":root.levelData.muted?"Muted":root.level+"%"; font.pixelSize: 13; color: Theme.ink }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: 32; width: parent.width; visible: root.radio
            text: root.radioData.level<0?(root.narrow?"?":"Unavailable"):root.radioData.level===1?"On":"Off"
            font.pixelSize: 12; color: Theme.muted; horizontalAlignment: Text.AlignHCenter }
        Column { width: parent.width; spacing: 5; visible: root.kind==="downloads"
            Text { text: "Downloads"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 16 }
            Text { width: parent.width; text: root.shell.downloads.summary; font.pixelSize: 12; color: Theme.muted; elide: Text.ElideRight }
            Rectangle { width: parent.width; height: 5; radius: 3; color: Qt.alpha(Theme.screenContact,.15)
                readonly property var current: root.shell.downloads.tasks.filter(t => t.state==="running" || t.state==="queued")[0] || ({})
                Rectangle { height: parent.height; radius: 3; width: parent.width*Math.max(0,Math.min(1,Number(parent.current.progress || 0))); color: Theme.chassis }
            }
        }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: 30; visible: root.kind==="battery"
            text: typeof powerStatus!=="undefined" && powerStatus.available?powerStatus.percent+"%":"?"; font.pixelSize: 20; color: Theme.ink }
        Column { anchors.centerIn: parent; width: parent.width; spacing: 5; visible: root.kind==="clock"
            property date now: new Date()
            Timer { triggeredOnStart: true; interval: 30000; running: root.visible && root.kind==="clock"; repeat: true; onTriggered: parent.now=new Date() }
            Text { width: parent.width; text: Qt.formatTime(parent.now,"hh:mm"); horizontalAlignment: Text.AlignHCenter; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 24 }
            Text { width: parent.width; text: Qt.formatDate(parent.now,"ddd, d MMM"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 11 }
        }
        Column { anchors.centerIn: parent; width: parent.width; spacing: 6; visible: root.kind==="storage"
            Repeater { model: root.shell.device.status.slice(1)
                Column { required property var modelData; width: parent.width; spacing: 1
                    Text { width: parent.width; text: modelData.title; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 13; elide: Text.ElideRight }
                    Text { width: parent.width; text: modelData.value; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
                }
            }
        }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: 33; width: parent.width; visible: root.kind==="scraper"; text: "Find missing"; font.pixelSize: 11; color: Theme.muted; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
    }
}
