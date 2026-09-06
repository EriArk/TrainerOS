import QtQuick

Item {
    id: root
    required property var shell
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
        Text { text: (root.shell.home.hasTrainer ? "WELCOME BACK, " : "WELCOME, ") + root.shell.home.trainer; color: Theme.muted; font.pixelSize: 13; font.bold: true; font.letterSpacing: 2; width: 620; elide: Text.ElideRight; textFormat: Text.PlainText }
        Text { text: root.shell.home.world; width: 600; elide: Text.ElideRight; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: text.length > 14 ? 48 : 70; font.weight: Font.DemiBold }
        Text { text: root.shell.home.adventure; width: 600; elide: Text.ElideRight; textFormat: Text.PlainText; color: "#347561"; font.pixelSize: 24 }
        Text { text: "A little further. A new discovery."; color: Theme.muted; font.pixelSize: 16 }
    }
    Row {
        x: 33; y: 250; spacing: 12
        Repeater {
            model: [{number: root.shell.home.badges, label: "BADGES"}, {number: root.shell.home.caught, label: "CAUGHT"}]
            delegate: Rectangle {
                required property var modelData
                width: 142; height: 82; radius: 13; color: "#edf2e7"; border.color: "#c9d8c7"
                Text { x: 15; y: 7; text: modelData.number; color: Theme.ink; font.pixelSize: 34; font.bold: true }
                Text { x: 16; y: 53; text: modelData.label; color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1 }
            }
        }
    }
    Text { x: 34; y: 350; text: root.shell.home.milestone; color: Theme.muted; font.pixelSize: 15 }
    Rectangle {
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: 244
        gradient: Gradient {
            GradientStop { position: 0; color: "#dce9db" }
            GradientStop { position: 1; color: "#ecf2e6" }
        }
        Rectangle { width: 1; height: parent.height; color: "#c0d1c0" }
        Text { x: 23; y: 28; text: "FIELD COMPANION"; color: Theme.muted; font.pixelSize: 12; font.letterSpacing: 1.5; font.bold: true }
        Rectangle {
            x: 37; y: 77; width: 170; height: 170; radius: 85
            color: "#eef4e9"; border.width: 2; border.color: "#b1caba"
            Rectangle {
                anchors.centerIn: parent; width: 136; height: 136; radius: 68
                color: "#badac9"; border.color: "#8ebaa4"
                Rectangle { anchors.centerIn: parent; width: 77; height: 77; radius: 18; rotation: 45; color: "#3f8474"; border.color: "#195749"; border.width: 3 }
                Rectangle { anchors.centerIn: parent; width: 37; height: 37; radius: 19; color: Theme.yellow; border.color: "#fff0b9"; border.width: 3 }
            }
            Text { x: 79; y: 5; text: "N"; font.pixelSize: 13; color: Theme.muted; font.bold: true }
        }
        Text { x: 22; y: 274; width: 200; text: "Every journey\nstarts somewhere."; color: Theme.ink; font.pixelSize: 23; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter }
        Text { x: 22; y: 350; width: 200; text: root.shell.sampleLibrary ? "SAMPLE ADVENTURE DATA" : "YOUR TRAINER JOURNAL"; color: Theme.muted; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; font.letterSpacing: 1 }
    }
}
