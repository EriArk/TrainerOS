import QtQuick

Item {
    id: root
    required property var member
    required property int slot
    property bool playing: false
    property bool selected: false
    property real presentationScale: 1
    property bool freeRoam: false
    readonly property var clips: member.clips || ({})
    readonly property var portraits: member.portraits || ({})
    readonly property bool resting: member.condition === "Fainted" || member.condition === "Sleep"
    readonly property bool active: visible && playing && !Theme.reducedMotion && Qt.application.state === Qt.ApplicationActive
    property string action: "Idle"
    property string emotion: ""
    property string symbol: ""
    property real position: 0.5
    property real destination: 0.5
    property real travelDuration: 1000
    property bool called: false
    property real targetX: x
    property real targetY: y
    readonly property bool busy: travel.running || crossing.running || pause.running
    readonly property real footX: x + body.x + body.width/2
    readonly property real footY: y + body.y + body.height
    readonly property var portrait: portraits[emotion] || portraits.Normal || ({})
    onSelectedChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    onVisibleChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
    Component.onCompleted: { settle(); if (selected && visible) forceActiveFocus(Qt.OtherFocusReason) }
    function settle() {
        travel.stop(); crossing.stop(); pause.stop(); called = false; emotion = ""; symbol = ""
        position = 0.5; action = resting && clips.Sleep ? "Sleep" : "Idle"
    }
    function walk(to) {
        destination = Math.max(0.05, Math.min(0.95, to))
        const direction = destination >= position ? "WalkRight" : "WalkLeft"
        travelDuration = Math.max(650, Math.abs(destination-position) * (width-body.width) / 28 * 1000)
        if (!active || resting || !clips[direction]) { action = resting && clips.Sleep ? "Sleep" : "Idle"; pause.restart(); return }
        action = direction; travel.restart()
    }
    function walkTo(toX, toY) {
        travel.stop(); crossing.stop(); pause.stop()
        if (!active || resting) return
        const dx = toX-x, dy = toY-y
        const horizontal = dx >= 0 ? "WalkRight" : "WalkLeft"
        const direction = Math.abs(dy) > Math.abs(dx) ? (dy > 0 ? "WalkDown" : "WalkUp") : horizontal
        const clip = clips[direction] ? direction : horizontal
        if (!clips[clip]) return
        emotion = ""; symbol = ""; called = false
        targetX = toX; targetY = toY
        travelDuration = Math.max(650, Math.sqrt(dx*dx+dy*dy)/34*1000)
        action = clip; crossing.start()
    }
    function respond(kind, toward) {
        travel.stop(); crossing.stop(); pause.stop(); called = false
        if (resting || kind === "rest") {
            emotion = "Normal"; symbol = "z z"; action = clips.Sleep ? "Sleep" : "Idle"; pause.restart(); return
        }
        emotion = kind === "curious" ? "Normal" : "Happy"
        symbol = kind === "greet" ? "♥" : kind === "curious" ? "?" : "♪"
        if (freeRoam) { action = "Idle"; pause.restart() }
        else if (kind === "call") { called = true; walk(0.5) }
        else if (kind === "play" || kind === "chat") walk(toward)
        else { action = "Idle"; pause.restart() }
    }
    function wander(choice, to) {
        if (!active || busy || resting) return
        emotion = ""; symbol = ""; called = false
        if (choice === 0 && clips.Sleep) { action = "Sleep"; pause.restart() }
        else walk(to)
    }
    onActiveChanged: if (!active) settle()
    onMemberChanged: settle()
    Rectangle {
        x: body.x + 13; y: body.y + body.height - 5
        width: body.width - 26; height: 10; radius: 5
        color: root.selected ? "#b0ffcc38" : "#50618b43"
        border.width: root.selected ? 2 : 0; border.color: "#fff4a0"
    }
    Item {
        id: body
        x: (root.width - width) * root.position
        y: parent.height - height - 9 + (root.called ? 12 : 0)
        width: 94 * root.presentationScale; height: 76 * root.presentationScale
        Behavior on y { NumberAnimation { duration: Theme.motion(250) } }
        SpritePreview {
            id: sprite; objectName: "playroom-sprite-" + root.slot
            anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom
            width: parent.width; height: Math.min(parent.height, cellHeight > 0 ? cellHeight * 2 * root.presentationScale : parent.height)
            asset: root.clips[root.action] || root.clips.Idle || ({})
            pixelScale: 2 * root.presentationScale; trimTransparentMargins: true; playing: root.active
        }
        Image {
            anchors.fill: parent; source: root.visible && !sprite.ready && root.member.art ? root.member.art.url || "" : ""
            visible: !sprite.ready; asynchronous: true; fillMode: Image.PreserveAspectFit
        }
        Text {
            anchors.centerIn: parent; width: parent.width; visible: !sprite.ready && !(root.member.art && root.member.art.url)
            text: root.member.name; textFormat: Text.PlainText; horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 15
        }
        Rectangle {
            id: bubble; objectName: "playroom-bubble-" + root.slot
            visible: root.emotion.length > 0
            x: parent.width-12; y: -31; width: 55; height: 50; radius: 8
            color: "#fffce4"; border.width: 2; border.color: "#566d49"
            scale: visible ? 1 : 0.8
            Behavior on scale { NumberAnimation { duration: Theme.motion(140); easing.type: Easing.OutBack } }
            Rectangle { x: 7; y: parent.height-5; width: 9; height: 9; rotation: 45; color: "#fffce4"; border.color: "#566d49" }
            Rectangle { x: 5; y: parent.height-8; width: 16; height: 6; color: "#fffce4" }
            Image {
                id: face; anchors.centerIn: parent; width: 40; height: 40
                source: bubble.visible ? root.portrait.url || "" : ""
                asynchronous: true; smooth: false; fillMode: Image.PreserveAspectFit
            }
            Text { anchors.centerIn: parent; visible: face.status !== Image.Ready; text: root.symbol; color: "#bc5973"; font.pixelSize: 25; font.bold: true }
        }
        Text { x: parent.width-8; y: parent.height-24; text: root.action === "Sleep" ? "z z" : root.symbol; color: root.action === "Sleep" ? "#435f71" : "#b24d67"; font.pixelSize: 16; font.bold: true }
    }
    Timer { id: pause; interval: 4200; onTriggered: { root.emotion = ""; root.symbol = ""; root.called = false; root.action = root.resting && root.clips.Sleep ? "Sleep" : "Idle" } }
    NumberAnimation {
        id: travel; target: root; property: "position"; to: root.destination; duration: root.travelDuration
        onFinished: { root.action = "Idle"; pause.restart() }
    }
    ParallelAnimation {
        id: crossing
        NumberAnimation { target: root; property: "x"; to: root.targetX; duration: root.travelDuration }
        NumberAnimation { target: root; property: "y"; to: root.targetY; duration: root.travelDuration }
        onFinished: { root.action = "Idle"; pause.restart() }
    }
}
