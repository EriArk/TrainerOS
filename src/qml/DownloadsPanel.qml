import QtQuick
import QtQuick.Controls

Item {
    id: root
    required property var flow
    readonly property var currentTask: flow.tasks[flow.focusIndex] || ({})
    readonly property string selectedKey: currentTask.key||""
    onSelectedKeyChanged: Qt.callLater(()=>details.contentY=0)
    readonly property bool commandsFocused: flow.actionsFocused
    property var queue: flow.tasks
    property string dragKey: ""
    property string dropKey: ""
    property bool dropValid: false
    property real dragY: 0
    function selectKey(key) { const n=flow.tasks.findIndex(t=>t.key===key); if(n>=0)flow.select(n) }
    function has(task,id) { return (task.actions||[]).some(a=>a.id===id&&a.enabled!==false) }
    function glyph(id) { return ({pause:"pause","pause-all":"pause",resume:"play","resume-all":"play",first:"download",cancel:"close",earlier:"arrow-up",later:"arrow-down",destination:"folder",open:"chevron",retry:"download",resolve:"folder"})[id]||"chevron" }
    function bytes(n) { if(n===undefined||n<0)return "—"; const units=["B","KB","MB","GB"]; let u=0;while(n>=1024&&u<3){n/=1024;u++}return n.toFixed(u===0?0:1)+" "+units[u] }
    function eta(t) { if(!(t.speed>0&&t.total>t.received))return "—";const s=Math.ceil((t.total-t.received)/t.speed);return s<60?s+" sec":s<3600?Math.ceil(s/60)+" min":Math.floor(s/3600)+" h "+Math.ceil((s%3600)/60)+" min" }
    function updateDrop() {
        const i=list.indexAt(8,list.contentY+dragY)
        const task=queue[i]
        dropValid=!!(task&&task.reorderable&&task.provider===queue.find(t=>t.key===dragKey)?.provider)
        dropKey=dropValid?task.key:""
        if(i<0&&list.contentY+dragY>=list.contentHeight)dropValid=true
    }
    Connections { target: root.flow; function onChanged() { if(!root.dragKey)root.queue=root.flow.tasks } }
    Rectangle { anchors.fill: parent; color: "#7a000000" }
    MouseArea { anchors.fill: parent }

    component CommandButton: Item {
        id: button
        property string label: ""
        property string icon: ""
        property bool selected: false
        property bool iconOnly: false
        signal clicked()
        implicitWidth: iconOnly?34:120; implicitHeight: 34
        opacity: enabled?1:.35
        Rectangle { anchors.fill: parent; radius: 6; color: button.selected?Theme.selection:mouse.containsMouse?Qt.rgba(Theme.accent.r,Theme.accent.g,Theme.accent.b,.09):"transparent"; border.width: button.selected?1:0; border.color: Theme.focusGlow }
        MenuGlyph { x: button.iconOnly?(parent.width-width)/2:9; anchors.verticalCenter: parent.verticalCenter; width: 17; height: 17; glyph: button.icon; ink: button.selected?Theme.accent:Theme.ink }
        Text { visible: !button.iconOnly; x: 34; width: parent.width-x-6; anchors.verticalCenter: parent.verticalCenter; text: button.label; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 12; elide: Text.ElideRight }
        MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: button.clicked() }
        ToolTip.visible: mouse.containsMouse; ToolTip.delay: 650; ToolTip.text: label
        Accessible.role: Accessible.Button; Accessible.name: label
        Accessible.onPressAction: if(enabled)clicked()
    }

    Rectangle {
        id: panel; objectName: "downloads-panel"
        anchors.centerIn: parent; width: Math.min(880,parent.width-32); height: Math.min(472,parent.height-24)
        radius: 16; color: Theme.paper; border.color: Theme.divider
        CrtTexture { anchors.fill: parent; anchors.margins: 2 }
        Text { x: 22; y: 16; text: "Downloads"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 24 }
        Text { x: 22; y: 46; text: root.flow.summary; color: Theme.muted; font.pixelSize: 11 }
        Row { anchors.right: close.left; anchors.rightMargin: 12; y: 19; spacing: 6
            CommandButton { objectName: "downloads-pause-all"; icon: "pause"; label: "Pause all"; width: 98; enabled: root.queue.some(t=>!t.terminal); onClicked: root.flow.controlAll("pause-all") }
            CommandButton { objectName: "downloads-resume-all"; icon: "play"; label: "Resume all"; width: 108; enabled: root.queue.some(t=>!t.terminal); onClicked: root.flow.controlAll("resume-all") }
        }
        CommandButton { id: close; objectName: "downloads-close"; anchors.right: parent.right; anchors.rightMargin: 15; y: 18; icon: "close"; iconOnly: true; label: "Close downloads"; onClicked: root.flow.close() }
        Rectangle { x: 20; y: 73; width: parent.width-40; height: 1; color: Theme.divider }
        readonly property real split: Math.round(width*.46)
        Rectangle { x: panel.split; y: 88; width: 1; height: parent.height-y-46; color: Theme.divider }

        ListView {
            id: list; objectName: "downloads-list"
            x: 14; y: 84; width: panel.split-x-12; height: panel.height-y-45
            model: root.queue; clip: true; spacing: 4; keyNavigationEnabled: false
            currentIndex: root.queue.findIndex(t=>t.key===root.currentTask.key)
            interactive: !root.dragKey
            onCurrentIndexChanged: if(!root.dragKey)Qt.callLater(()=>list.positionViewAtIndex(list.currentIndex,ListView.Contain))
            delegate: Rectangle {
                id: taskRow
                required property int index; required property var modelData
                objectName: "downloads-task-"+modelData.id
                width: list.width-4; height: 89; radius: 8
                color: index===list.currentIndex?Theme.selection:"transparent"
                border.width: index===list.currentIndex&&!root.flow.actionsFocused?1:0; border.color: Theme.focusGlow
                opacity: root.dragKey===modelData.key ? .5 : 1
                MouseArea { anchors.fill: parent; onClicked: root.selectKey(taskRow.modelData.key) }
                Image { id: art; x: 10; y: 10; width: 36; height: 36; source: taskRow.modelData.picture||""; asynchronous: true; sourceSize: Qt.size(72,72); fillMode: Image.PreserveAspectFit }
                MenuGlyph { x: 17; y: 17; width: 22; height: 22; glyph: "download"; ink: Theme.muted; visible: art.status!==Image.Ready }
                Text { x: 56; y: 10; width: parent.width-x-34; text: taskRow.modelData.title; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 14; color: Theme.ink }
                Text { x: 56; y: 30; width: parent.width-x-34; text: taskRow.modelData.detail||taskRow.modelData.state; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 10; color: Theme.muted }
                Text { x: 11; y: 58; width: parent.width-132; text: taskRow.modelData.state==="running"&&taskRow.modelData.speed!==undefined?root.bytes(taskRow.modelData.speed)+"/s":taskRow.modelData.source||taskRow.modelData.provider; elide: Text.ElideRight; font.pixelSize: 10; color: Theme.muted }
                Row { anchors.right: parent.right; anchors.rightMargin: 5; y: 47; spacing: 0
                    CommandButton { objectName: "downloads-first-"+taskRow.modelData.id; icon: "download"; label: "Download first"; iconOnly: true; visible: root.has(taskRow.modelData,"first"); onClicked: root.flow.command(taskRow.modelData.key,"first") }
                    CommandButton {
                        readonly property string action: root.has(taskRow.modelData,"pause")?"pause":root.has(taskRow.modelData,"pause-all")?"pause-all":root.has(taskRow.modelData,"resume")?"resume":root.has(taskRow.modelData,"resume-all")?"resume-all":""
                        objectName: "downloads-pause-"+taskRow.modelData.id; icon: root.glyph(action); label: action.startsWith("pause")?"Pause":"Resume"; iconOnly: true; visible: !!action
                        onClicked: root.flow.command(taskRow.modelData.key,action)
                    }
                    CommandButton { objectName: "downloads-cancel-"+taskRow.modelData.id; icon: "close"; label: "Cancel download"; iconOnly: true; visible: root.has(taskRow.modelData,"cancel"); onClicked: root.flow.command(taskRow.modelData.key,"cancel") }
                }
                Rectangle { x: 11; y: 83; height: 2; width: parent.width-22; color: Theme.divider; visible: !taskRow.modelData.terminal
                    Rectangle { height: parent.height; width: parent.width*Math.max(0,Math.min(1,taskRow.modelData.progress||0)); color: Theme.accent }
                }
                Item {
                    objectName: "downloads-drag-"+taskRow.modelData.id
                    anchors.right: parent.right; width: 32; height: 43; visible: taskRow.modelData.reorderable===true
                    MenuGlyph { anchors.centerIn: parent; width: 18; height: 18; glyph: "grip"; ink: Theme.muted }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.OpenHandCursor; preventStealing: true
                        onPressed: mouse => { root.dragKey=taskRow.modelData.key;root.dragY=mapToItem(list,mouse.x,mouse.y).y;root.updateDrop() }
                        onPositionChanged: mouse => { if(pressed){root.dragY=mapToItem(list,mouse.x,mouse.y).y;root.updateDrop()} }
                        onReleased: { const key=root.dragKey,before=root.dropKey,valid=root.dropValid;root.dragKey="";root.dropKey="";if(valid)root.flow.moveBefore(key,before);root.queue=root.flow.tasks }
                        onCanceled: { root.dragKey="";root.dropKey="";root.queue=root.flow.tasks }
                    }
                    Accessible.role: Accessible.Button; Accessible.name: "Reorder "+taskRow.modelData.title+". Move up and down are also available in details."
                }
                Rectangle { anchors.top: parent.top; width: parent.width; height: 2; color: Theme.accent; visible: !!root.dragKey&&root.dropKey===taskRow.modelData.key }
            }
            ScrollBar.vertical: ScrollBar { width: 4; policy: ScrollBar.AsNeeded }
            Text { anchors.centerIn: parent; width: parent.width-24; visible: list.count===0; text: "Your queue is clear"; horizontalAlignment: Text.AlignHCenter; color: Theme.muted; font.pixelSize: 14 }
        }
        Timer { interval: 50; repeat: true; running: !!root.dragKey
            onTriggered: { if(root.dragY<28)list.contentY=Math.max(0,list.contentY-8);else if(root.dragY>list.height-28)list.contentY=Math.min(Math.max(0,list.contentHeight-list.height),list.contentY+8);root.updateDrop() }
        }

        Flickable {
            id: details; objectName: "downloads-details"
            x: panel.split+20; y: 88; width: panel.width-x-22; height: panel.height-y-45
            clip: true; contentHeight: detailColumn.height; boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { width: 4 }
            Column {
                id: detailColumn; width: details.width-8; spacing: 7; visible: !!root.currentTask.key
                Item { width: parent.width; height: Math.max(40,heading.implicitHeight+14)
                    Image { id: detailArt; anchors.right: parent.right; width: 42; height: 42; source: root.currentTask.picture||""; asynchronous: true; fillMode: Image.PreserveAspectFit }
                    Text { id: heading; width: parent.width-(detailArt.status===Image.Ready?70:0); text: root.currentTask.title||""; textFormat: Text.PlainText; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 18; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
                    Text { anchors.bottom: parent.bottom; text: (root.currentTask.source||root.currentTask.provider||"")+(root.currentTask.version?" · "+root.currentTask.version:""); color: Theme.muted; font.pixelSize: 10 }
                }
                Text { width: parent.width; text: root.currentTask.description||root.currentTask.detail||""; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 11; lineHeight: 1.1; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
                Row { width: parent.width; spacing: 8; visible: root.currentTask.received!==undefined
                    Column { width: (parent.width-8)/2; spacing: 4
                        Text { text: root.bytes(root.currentTask.speed)+"/s"; color: Theme.ink; font.pixelSize: 18 }
                        Text { text: "Download speed"; color: Theme.muted; font.pixelSize: 10 }
                    }
                    Column { width: (parent.width-8)/2; spacing: 4
                        Text { text: root.eta(root.currentTask); color: Theme.ink; font.pixelSize: 18 }
                        Text { text: "Time remaining"; color: Theme.muted; font.pixelSize: 10 }
                    }
                }
                Canvas {
                    id: graph; objectName: "downloads-speed-graph"; width: parent.width; height: 42; visible: root.currentTask.received!==undefined
                    property var samples: root.currentTask.history||[]
                    property color line: Theme.accent
                    property color grid: Theme.divider
                    onSamplesChanged: requestPaint(); onWidthChanged: requestPaint(); onLineChanged: requestPaint(); onGridChanged: requestPaint()
                    onPaint: {
                        const c=getContext("2d");c.clearRect(0,0,width,height);c.strokeStyle=grid;c.lineWidth=1
                        for(let n=0;n<3;n++){const y=4+n*(height-8)/2;c.beginPath();c.moveTo(0,y);c.lineTo(width,y);c.stroke()}
                        if(samples.length<2)return
                        const max=Math.max(1,...samples);c.beginPath()
                        for(let i=0;i<samples.length;i++){const x=width*(60-samples.length+i)/59,y=height-4-(height-8)*samples[i]/max;if(i===0)c.moveTo(x,y);else c.lineTo(x,y)}
                        c.strokeStyle=line;c.lineWidth=1.8;c.stroke()
                    }
                    Text { anchors.right: parent.right; anchors.top: parent.top; text: "Last 60 seconds"; font.pixelSize: 9; color: Theme.muted }
                }
                Column { width: parent.width; spacing: 6
                    Text { width: parent.width; text: root.currentTask.received!==undefined?root.bytes(root.currentTask.received)+" / "+root.bytes(root.currentTask.total):root.currentTask.detail||root.currentTask.state||""; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight }
                    Rectangle { width: parent.width; height: 3; radius: 1.5; color: Theme.divider; visible: root.currentTask.progress!==undefined
                        Rectangle { height: parent.height; radius: 1.5; width: parent.width*Math.max(0,Math.min(1,root.currentTask.progress||0)); color: Theme.accent }
                    }
                }
                Column { width: parent.width; spacing: 4; visible: !!root.currentTask.destination
                    Text { text: root.currentTask.provider==="crates"?"Installation folder":root.currentTask.state==="done"?"Saved to":"Save to after downloading"; color: Theme.muted; font.pixelSize: 10 }
                    Text { objectName: "downloads-destination"; width: parent.width; text: root.currentTask.destination||""; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 11; elide: Text.ElideMiddle }
                }

                Flow { id: commands; width: parent.width; spacing: 4
                    Repeater { model: root.flow.actions
                        CommandButton {
                            required property int index; required property var modelData
                            objectName: "downloads-command-"+modelData.id
                            width: (commands.width-4)/2; label: modelData.label; icon: root.glyph(modelData.id)
                            selected: root.flow.actionsFocused&&root.flow.actionIndex===index
                            enabled: modelData.enabled!==false
                            onSelectedChanged: if(selected)Qt.callLater(()=>{const pos=mapToItem(detailColumn,0,0).y;details.contentY=Math.max(0,Math.min(details.contentHeight-details.height,pos+height-details.height))})
                            onClicked: root.flow.activate(index)
                        }
                    }
                }

            }
            Text { anchors.centerIn: parent; width: parent.width-24; visible: !root.currentTask.key; text: "Select a download to see\nits progress and details"; horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5; color: Theme.muted; font.pixelSize: 14 }
        }
        Text { x: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 15; text: Theme.buttonLabel("A")+" Actions · "+Theme.buttonLabel("B")+(root.flow.actionsFocused?" Queue":" Close"); color: Theme.muted; font.pixelSize: 10 }
        Text { anchors.right: parent.right; anchors.rightMargin: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 15; text: root.dragKey?"Drop to reorder this source":"Closing keeps downloads running"; color: Theme.muted; font.pixelSize: 10 }
    }
    ChatFilePicker { anchors.fill: parent; picker: root.flow.picker; title: "Choose download folder" }
}
