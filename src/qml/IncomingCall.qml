import QtQuick

Item {
    id: root
    required property var social
    property bool ready: true
    readonly property var call: social.incomingCall
    visible: !!call.id
    function respond(accept) {
        if (!ready || call.callBusy || (accept && !call.answerable)) return
        social.respondToIncomingCall(call.id, accept)
    }
    Rectangle { anchors.fill: parent; color: "#99102223" }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted = true }
    Panel {
        objectName: "incoming-call-card"
        anchors.centerIn: parent; width: Math.min(420, parent.width-40); height: 338
        radius: 32; surface: Qt.tint(Theme.paper,"#0c86b3a2"); patterned: true
        Rectangle { x: 4; y: 10; width: parent.width-8; height: parent.height; radius: 30; color: "#55081216"; z: -1 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: 26; text: "Incoming call"; font.pixelSize: 16; color: Theme.muted }
        AccountAvatar { anchors.horizontalCenter: parent.horizontalCenter; y: 57; width: 88; height: 88; name: root.call.name || ""; source: root.call.avatar || "" }
        Text { x: 26; y: 160; width: parent.width-52; height: 40; text: root.call.name || ""; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: 28; minimumPixelSize: 18; fontSizeMode: Text.Fit; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; color: Theme.ink }
        Text { x: 26; y: 204; width: parent.width-52; text: root.call.callBusy ? "Declining…" : root.call.callError || (!root.ready ? "Release the controls" : root.call.answerable ? "Microphone starts muted" : (root.social.account.voice || {}).channel ? "You are already in another call" : "Voice is unavailable"); horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Theme.muted }
        Row { anchors.horizontalCenter: parent.horizontalCenter; y: 252; spacing: 20
            CapButton { objectName: "incoming-call-decline"; width: 166; height: 51; label: "Decline"; centered: true; textSize: 21; tint: Theme.pink; claimsFocus: false; enabled: root.ready && !root.call.callBusy; onActivated: root.respond(false) }
            CapButton { objectName: "incoming-call-answer"; width: 166; height: 51; label: "Answer"; centered: true; textSize: 21; tint: Theme.green; claimsFocus: false; enabled: root.ready && !root.call.callBusy && !!root.call.answerable; onActivated: root.respond(true) }
        }
    }
}
