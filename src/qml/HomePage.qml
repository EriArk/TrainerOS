import QtQuick

Item {
    id: root
    required property var shell
    readonly property var currentAdventure: shell.home
    clip: true
    Canvas {
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.strokeStyle = "#e1eadd";
            ctx.lineWidth = 2;
            for (let i = 0; i < 6; i++) {
                ctx.beginPath();
                ctx.ellipse(360 - i * 25, 35 - i * 24, 280 + i * 50, 280 + i * 48);
                ctx.stroke();
            }
            ctx.setLineDash([4, 7]);
            ctx.strokeStyle = "#a7c3b4";
            ctx.beginPath(); ctx.moveTo(280, 365); ctx.bezierCurveTo(480, 340, 365, 200, 650, 80); ctx.stroke();
        }
    }
    Column {
        x: 32; y: 27; spacing: 9
        Text { text: (root.currentAdventure.hasTrainer ? "WELCOME BACK, " : "WELCOME, ") + root.currentAdventure.trainer; color: Theme.muted; font.pixelSize: 13; font.bold: true; font.letterSpacing: 2; width: 620; elide: Text.ElideRight; textFormat: Text.PlainText }
        Text { text: root.currentAdventure.world; width: 600; elide: Text.ElideRight; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: text.length > 14 ? 48 : 70; font.weight: Font.DemiBold }
        Text { text: root.currentAdventure.adventure; width: 600; elide: Text.ElideRight; textFormat: Text.PlainText; color: "#347561"; font.pixelSize: 24 }
        Text { text: root.currentAdventure.adventureId.length ? "Your selected Adventure. Your next discovery." : "Your next adventure is waiting."; color: Theme.muted; font.pixelSize: 16 }
    }
    Text { x: 34; y: 219; width: 580; elide: Text.ElideRight; textFormat: Text.PlainText; text: root.currentAdventure.progressNote; color: Theme.muted; font.pixelSize: 12 }
    Row {
        x: 33; y: 250; spacing: 12
        Repeater {
            model: [{number: root.currentAdventure.badges, label: "BADGES", badges: true}, {number: root.currentAdventure.caught, label: "CAUGHT", badges: false}, {number: root.currentAdventure.recordedTime, label: "RECORDED TIME", badges: false}]
            delegate: Rectangle {
                required property var modelData
                width: 142; height: 92; radius: 13; color: "#edf2e7"; border.color: "#c9d8c7"
                Text { x: 15; y: 5; width: parent.width - 30; height: 38; verticalAlignment: Text.AlignVCenter; text: modelData.number + (modelData.badges && root.currentAdventure.badgeSlots.length ? " / 8" : ""); color: Theme.ink; font.pixelSize: text.length > 4 ? 22 : 34; font.bold: true; elide: Text.ElideRight }
                Row {
                    x: 18; y: 47; spacing: 6
                    visible: modelData.badges
                    Repeater {
                        model: modelData.badges ? root.currentAdventure.badgeSlots : []
                        delegate: Rectangle {
                            required property bool modelData
                            width: 8; height: 8; rotation: 45; radius: 1
                            color: modelData ? "#dfad46" : "#d9e0d4"
                            border.color: modelData ? "#906429" : "#aebcab"
                            Rectangle { x: 1; y: 1; width: 5; height: 2; color: modelData ? "#ffdc83" : "#ebeee7" }
                        }
                    }
                }
                Text { x: 16; y: 65; text: modelData.label; color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1 }
            }
        }
    }
    Text { x: 34; y: 350; width: 610; elide: Text.ElideRight; textFormat: Text.PlainText; text: root.currentAdventure.milestone; color: Theme.muted; font.pixelSize: 15 }
    Rectangle {
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: 244
        gradient: Gradient {
            GradientStop { position: 0; color: "#dce9db" }
            GradientStop { position: 1; color: "#ecf2e6" }
        }
        Rectangle { width: 1; height: parent.height; color: "#c0d1c0" }
        Text { x: 23; y: 28; text: "READY WHEN YOU ARE"; color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1; font.bold: true }
        AdventureButton { x: 12; y: 57; width: 220; height: 220; shell: root.shell }
        Text { x: 22; y: 287; width: 200; text: root.currentAdventure.action; wrapMode: Text.WordWrap; color: Theme.ink; font.pixelSize: 26; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter }
        Text { x: 22; y: 360; width: 200; text: "Y · CHOOSE ADVENTURE"; color: Theme.muted; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; font.letterSpacing: 0.6 }
    }
}
