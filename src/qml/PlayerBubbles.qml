import QtQuick
import QtQuick.Effects

Item {
    id: root
    objectName: "player-bubbles"
    required property var controller
    property real unit: 1
    property real bottomInset: 0
    readonly property var people: controller.players
    readonly property bool crowded: people.length>4
    property var heldPeople: null
    readonly property var shown: heldPeople || (crowded ? people.filter(p => p.speaking).concat(people.filter(p => !p.speaking)).slice(0,2) : people)
    readonly property int count: crowded ? 3 : shown.length
    readonly property real avatarTop: (crowded?6:9)*unit
    readonly property real controlsTop: (crowded?7:18)*unit
    function targetAt(i) { return crowded && i===2 ? "players" : "player:"+shown[i].id }
    readonly property real diameter: (crowded?32:48) * unit
    readonly property real gap: 8 * unit
    readonly property real margin: (Theme.television ? 36 : 16) * unit
    readonly property real panelWidth: count*(diameter+gap)+116*unit
    readonly property real panelHeight: (crowded?44:66)*unit
    readonly property real travelX: Math.max(0,width-margin*2-panelWidth)
    readonly property real travelY: Math.max(0,height-margin*2-panelHeight-bottomInset)
    property real dragX: 0
    property real dragY: 0
    property string pressed: ""
    property point start: Qt.point(0,0)
    property point origin: Qt.point(0,0)
    property bool dragged: false
    readonly property real bubbleLeft: margin + Math.max(0,Math.min(travelX,controller.position.x*travelX+dragX))
    readonly property real bubbleTop: margin + Math.max(0,Math.min(travelY,controller.position.y*travelY+dragY))
    readonly property var regions: {
        let result=[{id:"handle",rect:Qt.rect(bubbleLeft,bubbleTop,16*unit,panelHeight)}]
        for(let i=0;i<count;i++)result.push({id:targetAt(i),rect:Qt.rect(bubbleLeft+20*unit+i*(diameter+gap),bubbleTop+avatarTop,diameter,diameter)})
        for(let i=0;i<3;i++)result.push({id:["mic","deaf","comments"][i],rect:Qt.rect(bubbleLeft+panelWidth-(90-i*28)*unit,bubbleTop+controlsTop,28*unit,30*unit)})
        return result
    }
    onWidthChanged: controller.setDisplay((Theme.television?"tv":"handheld")+":"+(width>height?"landscape":"portrait"))
    onVisibleChanged: if(!visible)cancelDrag()
    function cancelDrag(){heldPeople=null;pressed="";dragX=0;dragY=0;dragged=false}
    function pointer(target,phase,point){
        if(phase==="press"){heldPeople=shown.slice();pressed=target;start=point;origin=Qt.point(controller.position.x*travelX,controller.position.y*travelY);dragged=false;return}
        if(pressed!==target)return
        if(target==="handle"&&(phase==="move"||phase==="release")){
            dragX=point.x-start.x;dragY=point.y-start.y;dragged=dragged||Math.abs(dragX)+Math.abs(dragY)>8*unit
        }
        if(phase!=="release")return
        if(dragged){const dest=Qt.point(travelX?(origin.x+dragX)/travelX:0,travelY?(origin.y+dragY)/travelY:0);cancelDrag();controller.setPosition(dest);return}
        let inside=false
        for(let r of regions)if(r.id===target&&point.x>=r.rect.x&&point.x<r.rect.x+r.rect.width&&point.y>=r.rect.y&&point.y<r.rect.y+r.rect.height)inside=true
        cancelDrag()
        if(!inside)return
        if(target==="mic")controller.voiceAction("mute")
        else if(target==="deaf")controller.voiceAction("output")
        else if(target==="comments")controller.toggleSpectators()
        else if(target==="players")controller.requestPlayers()
        else if(target.indexOf("player:")===0)controller.openPlayer(target.substring(7))
    }
    Rectangle {
        x: root.bubbleLeft; y: root.bubbleTop; width: root.panelWidth; height: root.panelHeight; radius: 18*root.unit
        color: Theme.surfaceColor("#ba1b3437"); border.color: Theme.surfaceColor("#657eb9ad"); border.width: root.unit
        Text { x: 4*root.unit; anchors.verticalCenter: parent.verticalCenter; text: "⋮"; color: Theme.textColor("#c3dad0"); font.pixelSize: 20*root.unit }
        MouseArea { width: 16*root.unit; height: parent.height
            onPressed: mouse => root.pointer("handle","press",mapToItem(root,mouse.x,mouse.y))
            onPositionChanged: mouse => {if(pressed)root.pointer("handle","move",mapToItem(root,mouse.x,mouse.y))}
            onReleased: mouse => root.pointer("handle","release",mapToItem(root,mouse.x,mouse.y))
            onCanceled: root.cancelDrag()
        }
    }
    Repeater { model: ["mic","deaf","comments"]
        Item { required property int index; required property string modelData
            x: root.bubbleLeft+root.panelWidth-(90-index*28)*root.unit; y: root.bubbleTop+root.controlsTop; width: 28*root.unit; height: 30*root.unit
            readonly property bool active: index===0 ? !root.controller.voice.muted && root.controller.voice.state==="connected" : index===1 ? !root.controller.voice.deaf && root.controller.voice.state==="connected" : root.controller.spectatorComments
            MenuGlyph { anchors.centerIn: parent; width: 19*root.unit; height: width; ink: "#e1eadb"; glyph: index===0?"microphone":index===1?"speaker":"chat"; opacity: parent.active?1:.35 }
            Rectangle { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; width: 10*root.unit; height: 2*root.unit; radius: root.unit; color: parent.active?Theme.green:Theme.pink }
            Rectangle { visible: index===2 && root.controller.unread>0; anchors.right: parent.right; width: 13*root.unit; height: width; radius: width/2; color: Theme.yellow
                Text { anchors.centerIn: parent; text: Math.min(99,root.controller.unread); font.pixelSize: 8*root.unit; color: Theme.ink } }
            MouseArea { anchors.fill: parent
                onPressed: mouse => root.pointer(modelData,"press",mapToItem(root,mouse.x,mouse.y))
                onReleased: mouse => root.pointer(modelData,"release",mapToItem(root,mouse.x,mouse.y)) }
        }
    }
    Repeater {
        model: root.count
        Rectangle {
            id: bubble
            required property int index
            readonly property var person: root.shown[index] || ({})
            readonly property bool collapsed: root.crowded && index===2
            x: root.bubbleLeft + 20*root.unit + index*(root.diameter+root.gap); y: root.bubbleTop+root.avatarTop
            width: root.diameter; height: width; radius: width/2
            color: [Theme.blue,Theme.pink,Theme.green,Theme.yellow][index%4]
            border.width: (person.speaking && !collapsed ? 4 : 2)*root.unit
            border.color: person.speaking && !collapsed ? Theme.surfaceColor("#58b875") : Theme.paper
            Text { anchors.centerIn: parent; visible: bubble.collapsed || avatar.status!==Image.Ready; text: bubble.collapsed ? "+"+(root.people.length-2) : (bubble.person.name || "?").substring(0,1).toUpperCase(); color: Theme.ink; font.pixelSize: (root.crowded?14:20)*root.unit; textFormat: Text.PlainText }
            Image { id: avatar; objectName: "player-avatar-" + bubble.person.seat; anchors.fill: parent; anchors.margins: 3*root.unit; source: bubble.collapsed ? "" : bubble.person.avatar || ""; asynchronous: true; sourceSize: Qt.size(96,96); fillMode: Image.PreserveAspectCrop; visible: false }
            Rectangle { id: mask; anchors.fill: avatar; radius: width/2; visible: false; layer.enabled: true; color: "white" }
            MultiEffect { anchors.fill: avatar; source: avatar; maskEnabled: true; maskSource: mask; visible: avatar.status===Image.Ready }
            Rectangle { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; width: 24*root.unit; height: 15*root.unit; radius: 6*root.unit; color: Theme.paper; visible: !bubble.collapsed && !root.crowded
                Text { anchors.centerIn: parent; text: "P"+bubble.person.seat; font.pixelSize: 10*root.unit; font.bold: true; color: Theme.ink } }
            Rectangle { anchors.right: parent.right; width: 18*root.unit; height: width; radius: width/2; color: Theme.yellow; visible: !bubble.collapsed && bubble.person.unread>0
                Text { anchors.centerIn: parent; text: Math.min(99,bubble.person.unread || 0); font.pixelSize: 10*root.unit; color: Theme.ink } }
            MouseArea {
                anchors.fill: parent
                onPressed: function(mouse) { root.pointer(root.targetAt(index),"press",mapToItem(root,mouse.x,mouse.y)) }
                onPositionChanged: function(mouse) { if(pressed)root.pointer(root.targetAt(index),"move",mapToItem(root,mouse.x,mouse.y)) }
                onReleased: function(mouse) { root.pointer(root.targetAt(index),"release",mapToItem(root,mouse.x,mouse.y)) }
                onCanceled: root.cancelDrag()
            }
            Rectangle { visible: !!bubble.person.localMuted; x: parent.width-8*root.unit; y: parent.height-8*root.unit; width: 9*root.unit; height: width; radius: width/2; color: Theme.pink }
            Accessible.role: Accessible.Button
            Accessible.name: collapsed ? "All players" : "P"+person.seat+" · "+person.name+(person.speaking ? " · Speaking" : "")
            Accessible.onPressAction: !collapsed ? root.controller.openPlayer(person.id) : root.controller.requestPlayers()
        }
    }
    Repeater { model: root.controller.balloons
        Rectangle { required property var modelData; required property int index
            property int seatIndex: {for(let i=0;i<root.count;i++)if(root.shown[i] && root.shown[i].id===modelData.author)return i;return -1}
            visible: modelData.spectator || (seatIndex>=0 && !(seatIndex===2&&root.crowded))
            width: Math.min(248*root.unit,root.width-2*root.margin); height: 62*root.unit; radius: 13*root.unit
            x: modelData.spectator ? root.width-width-root.margin : Math.max(root.margin,Math.min(root.width-width-root.margin,root.bubbleLeft+20*root.unit+seatIndex*(root.diameter+root.gap)))
            y: modelData.spectator ? root.height-root.bottomInset-root.margin-height-index*68*root.unit : root.bubbleTop+root.panelHeight+height<root.height-root.bottomInset-root.margin ? root.bubbleTop+root.panelHeight+7*root.unit : root.bubbleTop-height-7*root.unit
            color: modelData.spectator ? Theme.surfaceColor("#e9d9e7e4") : Qt.tint(Theme.paper,Theme.surfaceColor("#367eb9ad")); border.color: Theme.surfaceColor("#9eb5a7")
            Text { x: 12*root.unit; y: 7*root.unit; width: parent.width-24*root.unit; text: (modelData.spectator?"Spectator ? ":"")+modelData.name; color: Theme.muted; font.pixelSize: 11*root.unit; elide: Text.ElideRight; textFormat: Text.PlainText }
            Text { x: 12*root.unit; y: 23*root.unit; width: parent.width-24*root.unit; text: modelData.text; font.pixelSize: 13*root.unit; maximumLineCount: 2; wrapMode: Text.Wrap; elide: Text.ElideRight; color: Theme.ink; textFormat: Text.PlainText }
        }
    }

}
