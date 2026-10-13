import QtQuick
import QtQuick.Effects

Item {
    id: root
    objectName: "player-bubbles"
    required property var controller
    property real unit: 1
    property real bottomInset: 0
    readonly property var people: controller.players
    readonly property int count: Math.min(4, people.length)
    readonly property real diameter: 48 * unit
    readonly property real gap: 8 * unit
    readonly property real margin: (Theme.television ? 36 : 16) * unit
    readonly property real travelX: Math.max(0,width - margin * 2 - count * (diameter + gap) + gap)
    readonly property real travelY: Math.max(0,height - margin * 2 - diameter - bottomInset)
    property real dragX: 0
    property real dragY: 0
    property string pressed: ""
    property point start: Qt.point(0,0)
    property point origin: Qt.point(0,0)
    property bool dragged: false
    property string pressedIdentity: ""
    readonly property real bubbleLeft: margin + Math.max(0,Math.min(travelX,controller.position.x * travelX + dragX))
    readonly property real bubbleTop: margin + Math.max(0,Math.min(travelY,controller.position.y * travelY + dragY))
    readonly property var regions: {
        let result = []
        for (let i=0;i<count;i++) result.push({id: "seat:" + people[i].seat, rect: Qt.rect(bubbleLeft + i * (diameter+gap),bubbleTop,diameter,diameter)})
        return result
    }
    onVisibleChanged: if (!visible) cancelDrag()
    function cancelDrag() { pressed = ""; dragX = 0; dragY = 0; dragged = false }
    function pointer(target, phase, point) {
        if (phase === "press") { pressed = target; pressedIdentity = ""; for (let i=0;i<regions.length;i++) if(regions[i].id===target)pressedIdentity = people[i].id || people[i].name; start = point; origin = Qt.point(controller.position.x*travelX,controller.position.y*travelY); dragged = false; return }
        if (pressed !== target) return
        if (phase === "move" || phase === "release") {
            const dx = point.x-start.x, dy = point.y-start.y
            dragged = dragged || Math.abs(dx)+Math.abs(dy)>8*unit
            if (dragged) { dragX = dx; dragY = dy }
        }
        if (phase !== "release") return
        if (dragged) {
            const destination = Qt.point(travelX ? (origin.x+dragX)/travelX : 0, travelY ? (origin.y+dragY)/travelY : 0)
            cancelDrag(); controller.setPosition(destination); return
        }
        cancelDrag()
        for (let i=0;i<regions.length;i++) if (regions[i].id===target) {
            if ((people[i].id || people[i].name) !== pressedIdentity) return
            const rect = regions[i].rect
            if (point.x<rect.x || point.x>=rect.x+rect.width || point.y<rect.y || point.y>=rect.y+rect.height) return
            if ((i===3 && people.length>4) || !people[i].canMessage) controller.requestPlayers()
            else controller.openPlayer(people[i].id)
        }
    }
    Repeater {
        model: root.count
        Rectangle {
            id: bubble
            required property int index
            readonly property var person: root.people[index] || ({})
            readonly property bool collapsed: index===3 && root.people.length>4
            x: root.bubbleLeft + index*(root.diameter+root.gap); y: root.bubbleTop
            width: root.diameter; height: width; radius: width/2
            color: [Theme.blue,Theme.pink,Theme.green,Theme.yellow][index%4]
            border.width: (person.speaking && !collapsed ? 4 : 2)*root.unit
            border.color: person.speaking && !collapsed ? "#58b875" : Theme.paper
            Text { anchors.centerIn: parent; visible: bubble.collapsed || avatar.status!==Image.Ready; text: bubble.collapsed ? "+"+(root.people.length-3) : (bubble.person.name || "?").substring(0,1).toUpperCase(); color: Theme.ink; font.pixelSize: 20*root.unit; textFormat: Text.PlainText }
            Image { id: avatar; objectName: "player-avatar-" + bubble.person.seat; anchors.fill: parent; anchors.margins: 3*root.unit; source: bubble.collapsed ? "" : bubble.person.avatar || ""; asynchronous: true; sourceSize: Qt.size(96,96); fillMode: Image.PreserveAspectCrop; visible: false }
            Rectangle { id: mask; anchors.fill: avatar; radius: width/2; visible: false; layer.enabled: true; color: "white" }
            MultiEffect { anchors.fill: avatar; source: avatar; maskEnabled: true; maskSource: mask; visible: avatar.status===Image.Ready }
            Rectangle { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; width: 24*root.unit; height: 15*root.unit; radius: 6*root.unit; color: Theme.paper; visible: !bubble.collapsed
                Text { anchors.centerIn: parent; text: "P"+bubble.person.seat; font.pixelSize: 10*root.unit; font.bold: true; color: Theme.ink } }
            Rectangle { anchors.right: parent.right; width: 18*root.unit; height: width; radius: width/2; color: Theme.yellow; visible: !bubble.collapsed && bubble.person.unread>0
                Text { anchors.centerIn: parent; text: Math.min(99,bubble.person.unread || 0); font.pixelSize: 10*root.unit; color: Theme.ink } }
            MouseArea {
                anchors.fill: parent
                onPressed: function(mouse) { root.pointer("seat:"+bubble.person.seat,"press",mapToItem(root,mouse.x,mouse.y)) }
                onPositionChanged: function(mouse) { if(pressed)root.pointer("seat:"+bubble.person.seat,"move",mapToItem(root,mouse.x,mouse.y)) }
                onReleased: function(mouse) { root.pointer("seat:"+bubble.person.seat,"release",mapToItem(root,mouse.x,mouse.y)) }
                onCanceled: root.cancelDrag()
            }
            Accessible.role: Accessible.Button
            Accessible.name: collapsed ? "All players" : "P"+person.seat+" · "+person.name+(person.speaking ? " · Speaking" : "")
            Accessible.onPressAction: person.canMessage && !collapsed ? root.controller.openPlayer(person.id) : root.controller.requestPlayers()
        }
    }
}
