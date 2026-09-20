import QtQuick

Item {
    id: root
    required property var shell
    readonly property var hall: shell.hall
    enabled: !shell.drawerOpen
    readonly property bool takesFocus: visible && !shell.drawerOpen && !hall.editor.open && !hall.account.open && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    readonly property bool detailOpen: hall.route === "archive-detail" || hall.route === "achievement-detail"
    Item {
    anchors.fill: parent; visible: !root.hall.overview
    PageHeader { id: hallHeader; title: root.hall.archive ? "Hall of Fame" : "RetroAchievements"; trailing: root.hall.archive ? "Every journey leaves a story" : "Your account's achievements" }
    MountedPanel {
        x: 0; y: hallHeader.height; width: parent.width; height: 68; color: "#d2dcd6"
        Rectangle { x: 0; y: -parent.y; width: 6; height: parent.y; color: parent.color }
        Rectangle { anchors.right: parent.right; y: -parent.y; width: 6; height: parent.y; color: parent.color }
        Text { x: 24; y: 11; text: root.hall.archive ? "Adventure memories" : "Account collection · All matched Adventures"; color: Theme.ink; font.pixelSize: 21; font.weight: Font.DemiBold }
        Text { x: 24; y: 39; width: 495; text: root.hall.status; elide: Text.ElideRight; color: Theme.muted; font.pixelSize: 14 }
        Text { x: 548; y: 25; width: parent.width - 570; visible: root.hall.archive && root.hall.editable; text: "Select · New memory     X · Edit"; color: Theme.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
        Text { x: 548; y: 25; width: parent.width - 570; visible: !root.hall.archive; text: "Select · Refresh" + (root.hall.account.available ? "     X · Account" : ""); color: Theme.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
    }
    Item {
        anchors.fill: parent; visible: !root.detailOpen
        Rectangle {
            x: 0; y: hallHeader.height + 68; width: 538; height: Math.max(0, root.height - 71 - y); color: "#e2e7de"
            // Reveal the complete focused row above the attached action panel.
            ControllerList {
                objectName: "hall-list"
                x: 19; y: 0; width: 482; height: parent.height
                model: root.hall.rows; currentIndex: root.hall.rowIndex
                rowTints: !root.hall.archive
                namePrefix: "hall-row-"; tint: root.hall.archive ? "#c6b4df" : Theme.green
                takesFocus: root.takesFocus && !root.hall.overview && !root.detailOpen && root.hall.zone === "list"
                onActivated: row => root.shell.activate(row, "list")
            }
            Text {
                x: 28; y: 34; width: 463; text: root.hall.emptyMessage; textFormat: Text.PlainText
                wrapMode: Text.WordWrap; color: Theme.ink; font.pixelSize: 21
                visible: root.hall.rows.length === 0
            }
        }
        Rectangle {
            x: 538; y: hallHeader.height + 68; width: parent.width - x; height: Math.max(0, root.height - 71 - y); color: "#edf0df"
            TrainerEmblem { x: 23; y: 19; width: 97; height: 97; emblem: "compass" }
            Text { x: 136; y: 39; width: parent.width - 154; text: root.hall.archive ? "A JOURNEY\nREMEMBERED" : "ACHIEVEMENT\nRECORDS"; color: Theme.muted; font.pixelSize: 13; font.letterSpacing: 1.3 }
            Text { x: 23; y: 123; width: parent.width - 46; text: root.hall.detail.title; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 22; font.weight: Font.DemiBold; elide: Text.ElideRight }
            Text { x: 23; y: 160; width: parent.width - 46; text: root.hall.detail.summary; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 15 }
        }
    }
    Item {
        anchors.fill: parent; visible: root.detailOpen
        Text { x: 28; y: 135; width: parent.width - 56; text: root.hall.detail.title; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 27; font.weight: Font.DemiBold; elide: Text.ElideRight }
        Rectangle {
            x: 0; y: 172; width: 493; height: 138; color: "#e2e7de"
            Grid {
                x: 25; y: 12; columns: 3; spacing: 9; visible: root.hall.archive
                Repeater {
                    model: root.hall.team
                    delegate: Rectangle {
                        required property int index
                        required property var modelData
                        width: 141; height: 51; radius: 12; color: "#f5f4e9"; border.color: "#bacbbd"
                        Rectangle {
                            x: 8; y: 8; width: 22; height: 22; radius: 11; color: modelData.known ? Theme.yellow : "#d6dfd5"
                            Text { anchors.centerIn: parent; text: index + 1; color: Theme.ink; font.pixelSize: 12; font.bold: true }
                        }
                        Text { x: 35; y: 9; width: 101; text: modelData.name; color: Theme.ink; font.pixelSize: 14; elide: Text.ElideRight }
                        Text { x: 35; y: 32; text: modelData.level; color: Theme.muted; font.pixelSize: 12 }
                    }
                }
            }
            Item {
                anchors.fill: parent; visible: !root.hall.archive
                Rectangle {
                    x: 26; y: 12; width: 122; height: 122; radius: 61
                    color: root.hall.detail.earnedState === "earned" ? Theme.yellow : root.hall.detail.earnedState === "locked" ? "#c8d3d0" : "#d9cfdf"
                    border.width: 3; border.color: "#829687"
                    Text { anchors.centerIn: parent; text: root.hall.detail.earnedState === "earned" ? "✓" : root.hall.detail.earnedState === "locked" ? "—" : "?"; font.pixelSize: 54; font.bold: true; color: Theme.ink }
                }
                Text { x: 170; y: 22; width: 297; text: root.hall.detail.summary; wrapMode: Text.WordWrap; color: Theme.ink; font.pixelSize: 23; font.weight: Font.DemiBold }
                Text { x: 170; y: 91; width: 297; text: root.hall.detail.time; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 14 }
            }
        }
        Column {
            x: 519; y: 183; width: 365; spacing: 9
            Text { width: parent.width; text: root.hall.detail.world + (root.hall.archive ? " · " + root.hall.detail.time : ""); color: Theme.ink; font.pixelSize: 17; elide: Text.ElideRight }
            Text { width: parent.width; text: root.hall.detail.date; color: Theme.muted; font.pixelSize: 13; elide: Text.ElideRight }
            Text { width: parent.width; text: root.hall.detail.description; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 15; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
            Text { width: parent.width; text: root.hall.detail.source; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight }
        }
    }
    MountedPanel {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 71; color: "#ccdcd1"
        Row {
            x: 24; y: 14; spacing: 14
            Repeater {
                model: root.hall.actions
                delegate: CapButton {
                    required property int index
                    required property var modelData
                    objectName: "hall-action-" + index
                    width: 274; height: 42; label: modelData.label
                    tint: index === 0 ? Theme.blue : Theme.yellow
                    enabled: modelData.enabled; opacity: enabled ? 1 : 0.5
                    selected: root.takesFocus && !root.hall.overview && root.hall.zone === "actions" && root.hall.focusIndex === index && enabled
                    onActivated: root.shell.activate(index, "actions")
                }
            }
        }
        Text {
            anchors { right: parent.right; rightMargin: 25 }
            y: 19; width: 270
            text: root.detailOpen ? "B · Return to previous list"
                : root.hall.rows.length > 0 ? root.hall.rows.length + " records"
                : "No records to display"
            color: Theme.muted; font.pixelSize: 13; horizontalAlignment: Text.AlignRight
        }
    }
    }
    JourneyPanel { anchors.fill: parent; shell: root.shell; visible: root.hall.overview; takesFocus: root.takesFocus && root.hall.overview }
    ArchiveEditorPanel { anchors.fill: parent; shell: root.shell; visible: root.hall.editor.open }
    AchievementAccountPanel { anchors.fill: parent; shell: root.shell; visible: root.hall.account.open }
}
