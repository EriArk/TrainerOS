import QtQuick

Rectangle {
    id: root
    required property var shell
    required property var modelData
    property var board: shell.controls
    readonly property string kind: modelData.id
    readonly property bool selected: board.selected === kind
    readonly property bool slider: kind === "volume" || kind === "brightness"
    readonly property int level: slider ? Number(shell.device.rows[kind === "volume" ? 0 : 1].level) : -1
    readonly property int radioIndex: kind === "wifi" ? 0 : kind === "bluetooth" ? 1 : 2
    readonly property bool radio: kind === "wifi" || kind === "bluetooth" || kind === "airplane"
    readonly property var radioData: radio ? shell.device.radios[radioIndex] : ({})
    readonly property color accent: kind === "downloads" ? Theme.green : kind === "scraper" || kind === "brightness" ? Theme.yellow : kind === "bluetooth" ? Theme.tabColors[4] : Theme.blue
    readonly property string icon: ({volume:"speaker",brightness:"sun",wifi:"wifi",bluetooth:"bluetooth",airplane:"airplane",downloads:"download",battery:"battery",clock:"clock",storage:"storage",scraper:"pictures"})[kind]
    property real dragX: 0
    property real dragY: 0
    signal moved(real dx,real dy)
    readonly property color material: Qt.tint(Theme.paper,"#14c3a068")
    radius: Math.min(25,width/2)
    color: Theme.paper
    border.width: selected ? 2 : 1
    border.color: selected ? Theme.focusGlow : Qt.darker(Theme.paper,1.17)
    gradient: Gradient {
        GradientStop { position: 0; color: Qt.lighter(root.kind === "downloads" || root.kind === "scraper" ? root.accent : root.material,1.08) }
        GradientStop { position: .5; color: root.kind === "downloads" || root.kind === "scraper" ? root.accent : root.material }
        GradientStop { position: 1; color: Qt.darker(root.kind === "downloads" || root.kind === "scraper" ? root.accent : root.material,1.12) }
    }
    Rectangle { z: -1; x: -1; y: 5; width: parent.width+2; height: parent.height; radius: parent.radius; color: "#30342b22" }
    Rectangle { anchors.fill: parent; anchors.margins: 3; radius: parent.radius-3; color: "transparent"; border.color: "#b0fffbe8" }
    Rectangle { anchors.fill: parent; anchors.margins: 6; radius: parent.radius-6; color: "transparent"; border.color: "#204f3920" }
    transform: Translate { x: root.dragX; y: root.dragY }
    MouseArea {
        anchors.fill: parent
        property real startX; property real startY
        onPressed: mouse => {const p=mapToItem(null,mouse.x,mouse.y);startX=p.x;startY=p.y;root.board.select(root.kind)}
        onPositionChanged: mouse => {if(pressed && root.board.editing){const p=mapToItem(null,mouse.x,mouse.y);root.dragX=p.x-startX;root.dragY=p.y-startY}}
        onReleased: mouse => {
            if(root.board.editing){
                if(Math.abs(root.dragX)+Math.abs(root.dragY)>8)root.moved(root.dragX,root.dragY)
                else root.board.select(root.kind)
            } else root.board.activate(root.kind)
            root.dragX=0;root.dragY=0
        }
        onCanceled: {root.dragX=0;root.dragY=0}
    }
    Item {
        anchors.fill: parent; anchors.margins: 12
        enabled: !root.board.editing
        Rectangle { id: radioCap; visible: root.radio; anchors.horizontalCenter: parent.horizontalCenter; y: 2; width: Math.min(63,parent.width+5); height: width; radius: width/2
            readonly property color tone: root.radioData.level===1?root.accent:root.material
            Rectangle { z: -1; x: -2; y: 4; width: parent.width+4; height: parent.height; radius: width/2; color: "#35463824" }
            gradient: Gradient { GradientStop { position: 0; color: Qt.lighter(radioCap.tone,1.18) } GradientStop { position: 1; color: Qt.darker(radioCap.tone,1.15) } }
            border.color: Qt.darker(tone,1.25)
            Rectangle { anchors.fill: parent; anchors.margins: 3; radius: width/2; color: "transparent"; border.color: "#a6fffbe9" }
            MenuGlyph { anchors.centerIn: parent; width: parent.width*.48; height: width; glyph: root.icon }
        }
        MenuGlyph { id: glyph; anchors.horizontalCenter: parent.horizontalCenter; y: root.slider?7:root.kind==="battery"?1:8; width: root.kind==="battery"?24:30; height: width; glyph: root.icon; visible: !root.radio && root.kind !== "clock" && root.kind !== "storage" && root.kind !== "downloads"
            MouseArea { anchors.fill: parent; enabled: root.kind==="volume"; onClicked: root.shell.systemAction("volume") }
        }
        Text { id: label; x: -4; width: parent.width+8; anchors.bottom: parent.bottom; anchors.bottomMargin: root.slider?3:5; text: root.modelData.title; font.family: Theme.displayFamily; font.pixelSize: root.width<78?11:16; fontSizeMode: Text.HorizontalFit; minimumPixelSize: 9; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; visible: root.kind !== "storage" && root.kind !== "downloads" && root.kind !== "clock" }
        Item {
            visible: root.slider && root.height>180; x: (parent.width-width)/2; y: 49; width: Math.min(36,parent.width-4); height: parent.height-103
            Rectangle { anchors.fill: parent; radius: width/2; color: Qt.darker(Theme.paper,1.12); border.color: "#404f5b4a" }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Math.max(width/2,parent.height*Math.max(0,root.level)/100); radius: width/2; color: root.accent; border.color: Qt.darker(color,1.2) }
            Rectangle { width: parent.width-4; height: width; x: 2; y: Math.max(0,Math.min(parent.height-height,parent.height*(1-Math.max(0,root.level)/100)-height/2)); radius: width/2; color: Theme.paper; border.color: Qt.darker(root.accent,1.3) }
            MouseArea { anchors.fill: parent; enabled: root.level>=0; onPressed: mouse => set(mouse.y); onPositionChanged: mouse => {if(pressed)set(mouse.y)}
                function set(y){root.shell.device.setQuickLevel(root.kind==="volume"?0:1,Math.round(100*(1-y/height)))} }
        }
        Item { visible: root.slider && root.height<=180; x: 4; y: 51; width: parent.width-8; height: 20
            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 7; radius: 4; color: Qt.darker(Theme.paper,1.15) }
            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width*Math.max(0,root.level)/100; height: 7; radius: 4; color: root.accent }
            Rectangle { x: Math.max(0,Math.min(parent.width-18,(parent.width-18)*root.level/100)); width: 18; height: 18; radius: 9; color: Theme.paper; border.color: Theme.screenContact }
            MouseArea { anchors.fill: parent; onPressed: mouse => set(mouse.x); onPositionChanged: mouse => {if(pressed)set(mouse.x)}
                function set(x){root.shell.device.setQuickLevel(root.kind==="volume"?0:1,Math.round(x/width*100))} }
        }
        Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: label.top; anchors.bottomMargin: 8; text: root.level>=0?root.level+"%":"—"; font.pixelSize: root.height>180?22:15; font.bold: true; color: Theme.ink; visible: root.slider }
        Text { x: 0; y: radioCap.y+radioCap.height+9; width: parent.width; text: root.radioData.level<0?"Unavailable":root.radioData.level===1?"On":"Off"; font.pixelSize: 12; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; visible: root.radio }
        Column { x: 7; y: 12; width: parent.width-14; spacing: 14; visible: root.kind === "downloads"
            Text { text: "Downloads"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: root.width<180?16:23 }
            Text { width: parent.width; text: root.shell.downloads.summary; font.pixelSize: root.width<180?11:15; color: Theme.ink; elide: Text.ElideRight }
            Rectangle { width: parent.width; height: 7; radius: 4; color: "#33546243"
                readonly property var tasks: root.shell.downloads.tasks
                readonly property var current: tasks.filter(t => t.state === "running" || t.state === "queued")[0] || ({})
                Rectangle { height: parent.height; radius: 4; width: parent.width*Math.max(0,Math.min(1,Number(parent.current.progress || 0))); color: Theme.chassis }
            }
        }
        Text { anchors.centerIn: parent; text: typeof powerStatus !== "undefined" && powerStatus.available ? powerStatus.percent+"%" : "—"; font.family: Theme.displayFamily; font.pixelSize: 32; color: Theme.ink; visible: root.kind === "battery" }
        Column { anchors.centerIn: parent; width: parent.width; spacing: 8; visible: root.kind === "clock"
            property date now: new Date()
            Timer { triggeredOnStart: true; interval: 30000; running: root.visible && root.kind === "clock"; repeat: true; onTriggered: parent.now=new Date() }
            MenuGlyph { anchors.horizontalCenter: parent.horizontalCenter; width: 38; height: 38; glyph: "clock" }
            Text { width: parent.width; text: Qt.formatTime(parent.now,"hh:mm"); horizontalAlignment: Text.AlignHCenter; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25 }
            Text { width: parent.width; text: Qt.formatDate(parent.now,"ddd, d MMM"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 11 }
        }
        Column { anchors.centerIn: parent; width: parent.width-8; spacing: 11; visible: root.kind === "storage"
            Repeater { model: root.shell.device.status.slice(1)
                Column { required property var modelData; width: parent.width; spacing: 5
                    Text { width: parent.width; text: modelData.title; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 16; elide: Text.ElideRight }
                    Text { width: parent.width; text: modelData.value; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
                    Rectangle { width: parent.width; height: 1; color: "#24456850" }
                }
            }
        }
        Text { anchors.centerIn: parent; width: parent.width; text: "Find missing"; font.pixelSize: 12; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; visible: root.kind === "scraper" }
    }
    Rectangle { anchors.fill: parent; radius: parent.radius; color: "transparent"; border.width: 2; border.color: Theme.focusGlow; visible: root.board.editing && root.selected }
}
