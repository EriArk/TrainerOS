import QtQuick

Rectangle {
    id: root
    required property var shell
    required property var modelData
    property var board: shell.controls
    objectName: "start-widget-"+kind
    readonly property string kind: modelData.id
    readonly property bool selected: board.selected===kind
    readonly property bool external: modelData.external===true
    readonly property bool slider: kind==="volume" || kind==="brightness"
    readonly property var levelData: slider ? (shell.device.rows[kind==="volume"?0:1] || {}) : ({})
    readonly property int level: slider && levelData.level!==undefined ? Number(levelData.level) : -1
    readonly property bool radio: kind==="wifi" || kind==="bluetooth" || kind==="airplane"
    readonly property var radioData: radio ? (shell.device.radios[kind==="wifi"?0:kind==="bluetooth"?1:2] || {}) : ({})
    readonly property bool available: slider ? level>=0 : radio ? Number(radioData.level)>=0 : true
    readonly property color accent: kind==="brightness" || kind==="scraper" ? Theme.yellow : kind==="bluetooth" ? Theme.tabColors[4] : kind==="downloads" ? Theme.green : Theme.blue
    readonly property color readableAccent: Theme.dark ? Qt.tint(Theme.ink,Qt.alpha(accent,.45)) : accent
    readonly property string icon: ({volume:"speaker",brightness:"sun",wifi:"wifi",bluetooth:"bluetooth",airplane:"airplane",downloads:"download",battery:"battery",scraper:"pictures",clock:"clock",storage:"storage"})[kind] || "game-controller"
    readonly property bool narrow: width<90
    readonly property bool shortWidget: height<65
    readonly property bool tallDetails: modelData.h>=4 && (kind==="downloads"||kind==="storage")
    readonly property bool verticalSlider: slider && width<100 && height>width*1.4
    readonly property string valueText: slider ? (level<0 ? "Unavailable" : levelData.muted ? "Muted" : level+"%") : radio ? (!available ? "Unavailable" : radioData.level===1 ? "On" : "Off") : ""
    property real dragX: 0
    property real dragY: 0
    signal moved(real dx,real dy)
    signal swiped(int delta)
    radius: 10
    color: selected && kind!=="scraper" ? Theme.selection : Qt.alpha(Theme.ink,Theme.dark?.025:.035)
    border.width: selected ? 1.5 : 1
    border.color: selected ? Theme.focusGlow : Qt.alpha(Theme.divider,.5)
    z: dragX || dragY || board.moving&&selected ? 2 : 0
    transform: Translate { x: root.dragX; y: root.dragY }
    Accessible.role: Accessible.Button
    Accessible.name: modelData.title+" "+valueText
    Accessible.onPressAction: board.activate(kind)
    MouseArea {
        anchors.fill: parent
        property real startX
        property real startY
        property real distanceX: 0
        onPressed: mouse => { const p=mapToItem(root.parent,mouse.x,mouse.y); startX=p.x;startY=p.y;distanceX=0;root.board.select(root.kind) }
        onPositionChanged: mouse => { if(pressed){const p=mapToItem(root.parent,mouse.x,mouse.y);distanceX=p.x-startX;if(root.board.editing&&!root.board.moving){root.dragX=distanceX;root.dragY=p.y-startY}} }
        onReleased: {
            const dx=root.dragX,dy=root.dragY,swipe=distanceX
            root.dragX=0;root.dragY=0
            if(root.board.editing){if(Math.abs(dx)+Math.abs(dy)>8)root.moved(dx,dy);else if(root.board.moving)root.board.activate(root.kind);else root.board.select(root.kind)}
            else if(Math.abs(swipe)>50)root.swiped(swipe<0?1:-1)
            else root.board.activate(root.kind)
        }
        onCanceled: {root.dragX=0;root.dragY=0}
    }
    Item {
        anchors.fill: parent; anchors.margins: root.narrow?7:12
        enabled: !root.board.editing
        // Legacy single-column widgets remain usable; full names are in the footer.
        MenuGlyph { id: glyph; x: root.slider&&!root.verticalSlider&&!root.narrow ? 0 : (parent.width-width)/2; y: root.slider?0:3; width: 23; height: 23; glyph: root.icon
            visible: !root.external && root.kind!=="clock" && root.kind!=="storage" && root.kind!=="downloads" && !(root.kind==="scraper"&&root.width>=250) && !(root.shortWidget&&root.kind==="battery")
            ink: root.radio&&root.radioData.level===1 ? root.readableAccent : Theme.ink
            opacity: root.available?1:.4
            MouseArea { anchors.fill: parent; anchors.margins: -5; enabled: root.kind==="volume"&&root.level>=0; onClicked: {root.board.select(root.kind);root.shell.systemAction("volume")} }
        }
        Text { id: label; anchors.bottom: parent.bottom; width: parent.width; text: root.modelData.title; textFormat: Text.PlainText
            font.pixelSize: 12; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
            visible: !root.shortWidget&&!root.narrow&&!root.slider&&root.kind!=="storage"&&root.kind!=="downloads"&&root.kind!=="clock"&&!(root.kind==="scraper"&&root.width>=250) }
        ScrapeWidget { anchors.fill: parent; visible: root.kind==="scraper"&&root.width>=250; flow: root.shell.scraper; interactive: !root.board.editing; embedded: true }
        Text { x: 31; y: 3; width: parent.width-x; text: root.modelData.title; color: Theme.ink; font.pixelSize: 12; elide: Text.ElideRight; visible: root.slider&&!root.narrow }
        Text { anchors.fill: parent; anchors.bottomMargin: root.narrow?0:30; visible: root.external; text: root.modelData.text || ""; textFormat: Text.PlainText
            color: Theme.ink; font.pixelSize: 13; wrapMode: Text.Wrap; elide: Text.ElideRight; maximumLineCount: 3; verticalAlignment: Text.AlignVCenter; horizontalAlignment: Text.AlignHCenter }
        Item { id: bar; objectName: "start-slider-"+root.kind; visible: root.slider; x: root.verticalSlider ? (parent.width-width)/2 : 0; y: root.verticalSlider ? 36 : 34; width: root.verticalSlider?22:parent.width; height: root.verticalSlider?Math.max(18,parent.height-70):14
            Rectangle { anchors.fill: parent; radius: 5; color: Qt.alpha(Theme.ink,.12) }
            Rectangle { anchors.bottom: parent.bottom; width: root.verticalSlider?parent.width:parent.width*Math.max(0,Math.min(100,root.level))/100
                height: root.verticalSlider?parent.height*Math.max(0,Math.min(100,root.level))/100:parent.height; radius: 5; color: root.readableAccent; opacity: root.available?1:.3 }
            Rectangle { visible: root.board.adjusting&&root.selected; anchors.fill: parent; anchors.margins: -3; radius: 7; color: "transparent"; border.color: Theme.focusGlow }
            MouseArea { anchors.fill: parent; anchors.margins: -6; enabled: root.level>=0
                function setLevel(mouse){const p=mapToItem(bar,mouse.x,mouse.y);const v=root.verticalSlider?1-p.y/bar.height:p.x/bar.width;root.shell.device.setQuickLevel(root.kind==="volume"?0:1,Math.round(Math.max(0,Math.min(1,v))*100))}
                onPressed: mouse => {root.board.select(root.kind);setLevel(mouse)}
                onPositionChanged: mouse => {if(pressed)setLevel(mouse)}
            }
        }
        Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; visible: root.slider
            objectName: "start-output-"+root.kind
            text: (root.narrow&&!root.available?"—":root.valueText)+(root.kind==="volume"?" ›":""); font.pixelSize: root.narrow?12:13; color: root.available?Theme.ink:Theme.muted
            MouseArea { anchors.fill: parent; anchors.margins: 6; enabled: root.kind==="volume"
                onClicked: {root.board.select(root.kind);root.board.audioOutputs()}
            }
        }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: root.narrow?35:32; width: parent.width; visible: root.radio&&!root.shortWidget
            text: root.narrow&&!root.available?"—":root.valueText; font.pixelSize: 11; color: Theme.muted; horizontalAlignment: Text.AlignHCenter }
        Column { width: parent.width; spacing: 7; visible: root.kind==="downloads"&&!root.tallDetails
            Text { width: parent.width; text: "Downloads"; color: Theme.ink; font.pixelSize: 14; elide: Text.ElideRight }
            Text { width: parent.width; text: root.shell.downloads.summary; font.pixelSize: 11; color: Theme.muted; elide: Text.ElideRight }
            Rectangle { width: parent.width; height: 4; radius: 2; color: Qt.alpha(Theme.ink,.12)
                readonly property var current: root.shell.downloads.tasks.filter(t => t.state==="running" || t.state==="queued")[0] || ({})
                Rectangle { height: parent.height; radius: 2; width: parent.width*Math.max(0,Math.min(1,Number(parent.current.progress || 0))); color: root.readableAccent }
            }
        }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: root.shortWidget ? (parent.height-height)/2 : 31; visible: root.kind==="battery"
            text: typeof powerStatus!=="undefined"&&powerStatus.available?powerStatus.percent+"%":"—"; font.pixelSize: 19; color: Theme.ink }
        Column { anchors.centerIn: parent; width: parent.width; spacing: 5; visible: root.kind==="clock"
            property date now: new Date()
            Timer { triggeredOnStart: true; interval: 30000; running: root.visible&&root.kind==="clock"; repeat: true; onTriggered: parent.now=new Date() }
            Text { width: parent.width; text: Qt.formatTime(parent.now,"hh:mm"); horizontalAlignment: Text.AlignHCenter; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: root.shortWidget?21:24 }
            Text { width: parent.width; visible: !root.shortWidget; text: Qt.formatDate(parent.now,"ddd, d MMM"); horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
        }
        Column { anchors.centerIn: parent; width: parent.width; spacing: 5; visible: root.kind==="storage"&&!root.tallDetails
            Repeater { model: root.shell.device.status.slice(1)
                Column { required property var modelData; width: parent.width; spacing: 2
                    Text { width: parent.width; text: modelData.title; color: Theme.ink; font.pixelSize: 12; elide: Text.ElideRight }
                    Text { width: parent.width; text: modelData.value; color: Theme.muted; font.pixelSize: 10; elide: Text.ElideRight }
                }
            }
        }
        Column { width: parent.width; spacing: 10; visible: root.kind==="storage"&&root.tallDetails
            Text { text: "Storage"; color: Theme.ink; font.pixelSize: 16; font.family: Theme.displayFamily }
            Repeater { model: root.shell.device.status.slice(1)
                Column { required property var modelData; width: parent.width; spacing: 6
                    Rectangle { width: parent.width; height: 1; color: Theme.divider }
                    MenuGlyph { width: 23; height: 23; glyph: "storage"; ink: Theme.muted }
                    Text { width: parent.width; text: modelData.title; color: Theme.ink; font.pixelSize: 13; wrapMode: Text.Wrap }
                    Text { width: parent.width; text: modelData.value; color: Theme.muted; font.pixelSize: 11; wrapMode: Text.Wrap }
                }
            }
        }
        Item { anchors.fill: parent; visible: root.kind==="downloads"&&root.tallDetails
            Text { id: downloadTitle; text: "Downloads"; color: Theme.ink; font.pixelSize: 16; font.family: Theme.displayFamily }
            Text { y: 25; width: parent.width; text: root.shell.downloads.summary; color: Theme.muted; font.pixelSize: 10; elide: Text.ElideRight }
            Column { id: downloadRows; objectName: "start-download-rows"; y: 43; width: parent.width; spacing: 7
                readonly property int capacity: Math.max(1,Math.floor((parent.height-y-15)/39))
                Repeater { model: root.shell.downloads.tasks.slice(0,downloadRows.capacity)
                    Item { required property var modelData; objectName: "start-download-row-"+(modelData.id || ""); width: downloadRows.width; height: 32
                        Text { width: parent.width; text: modelData.title || "Download"; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 12; elide: Text.ElideRight }
                        Text { y: 15; width: parent.width; text: modelData.detail || modelData.state || "Queued"; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 10; elide: Text.ElideRight }
                        Rectangle { y: 29; width: parent.width; height: 2; color: Qt.alpha(Theme.ink,.12)
                            Rectangle { height: parent.height; width: parent.width*(modelData.state==="done"?1:Math.max(0,Math.min(1,Number(modelData.progress || 0)))); color: root.readableAccent }
                        }
                    }
                }
            }
            Text { anchors.centerIn: parent; visible: root.shell.downloads.tasks.length===0; text: "Nothing in the queue"; color: Theme.muted; font.pixelSize: 12 }
            Text { anchors.bottom: parent.bottom; text: root.shell.downloads.tasks.length>downloadRows.capacity ? "+"+(root.shell.downloads.tasks.length-downloadRows.capacity)+" more · Open queue" : "Open queue"; color: Theme.muted; font.pixelSize: 10 }
        }
    }
    Rectangle { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 5; width: 5; height: 5; radius: 3; visible: root.radio&&root.shortWidget
        color: root.radioData.level===1 ? Theme.accent : "transparent"; border.color: root.available?Theme.muted:Theme.divider
    }
    Rectangle { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 5; width: 15; height: 15; radius: 4; color: Theme.paper; visible: root.board.editing&&root.selected
        MenuGlyph { anchors.fill: parent; anchors.margins: 2; glyph: "expand"; ink: Theme.muted }
    }
}
