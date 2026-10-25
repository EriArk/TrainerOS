import QtQuick

Item {
    id: root
    required property var shell
    readonly property var checks: shell.diagnostics
    readonly property bool takesFocus: visible && !shell.menuOpen && shell.notice.length === 0
    // The main chassis owns the recessed surface for pages and services alike.
    Item {
        visible: !root.checks.health
        anchors.fill: parent; anchors.margins: Theme.panelInset
        anchors.topMargin: Theme.contentTopInset; clip: true
        PageHeader { id: diagnosticsHeader; compact: true; title: "Check your field gear"; subtitle: root.checks.connection }
        MountedPanel {
            x: 0; y: diagnosticsHeader.height; width: parent.width; height: parent.height - y; color: Theme.surfaceColor("#d4e2d6")
            Rectangle { x: 522; width: parent.width - x; height: parent.height; color: Theme.surfaceColor("#e1ecde") }
            Text { x: 30; y: 10; text: "LIVE INPUT  ·  DOT = SIGNAL OBSERVED"; color: Theme.muted; font.pixelSize: 10; font.letterSpacing: 0.5 }
            Grid {
                x: 30; y: 30; columns: 5; columnSpacing: 8; rowSpacing: 6
                Repeater {
                    model: root.checks.buttons
                    delegate: Rectangle {
                        required property int index; required property var modelData
                        objectName: "diagnostic-button-" + modelData.label
                        width: 85; height: 29; radius: 6
                        color: modelData.held ? Theme.yellow : Theme.surfaceColor("#f5f6e9")
                        border.color: modelData.held ? Theme.surfaceColor("#976a29") : Theme.surfaceColor("#8ab29c")
                        Text { anchors.centerIn: parent; text: modelData.label; color: Theme.ink; font.pixelSize: 13 }
                        Rectangle { x: 6; y: 11; width: 6; height: 6; radius: 3; color: Theme.ink; visible: modelData.seen }
                    }
                }
            }
            Row {
                x: 30; y: 137; spacing: 24
                Repeater {
                    model: 2
                    delegate: Item {
                        required property int index
                        width: 105; height: 76
                        Rectangle {
                            width: 66; height: 66; radius: 33; color: Theme.surfaceColor("#edf3e7"); border.color: Theme.surfaceColor("#81a593"); border.width: 2
                            Rectangle { x: 32; y: 4; width: 1; height: 58; color: Theme.surfaceColor("#bbcfbf") }
                            Rectangle { x: 4; y: 32; width: 58; height: 1; color: Theme.surfaceColor("#bbcfbf") }
                            Rectangle {
                                objectName: "diagnostic-stick-" + index
                                width: 14; height: 14; radius: 7; color: Theme.ink
                                x: 26 + root.checks.axes[index * 2].value * 25
                                y: 26 + root.checks.axes[index * 2 + 1].value * 25
                            }
                        }
                        Text { x: 72; y: 23; text: index === 0 ? "L" : "R"; color: Theme.ink; font.pixelSize: 17; font.bold: true }
                    }
                }
                Column {
                    spacing: 12
                    Repeater {
                        model: 2
                        delegate: Column {
                            required property int index
                            spacing: 3
                            Text { text: (index === 0 ? "L2" : "R2") + " · " + Math.round(root.checks.axes[index + 4].value * 100) + "%"; color: Theme.ink; font.pixelSize: 12 }
                            Rectangle {
                                width: 130; height: 9; radius: 4; color: Theme.surfaceColor("#f5f6e9"); border.color: Theme.surfaceColor("#81a593")
                                Rectangle { x: 1; y: 1; height: 7; radius: 3; width: Math.max(0, Math.min(1, root.checks.axes[index + 4].value)) * 128; color: Theme.surfaceColor("#3e8b76") }
                            }
                        }
                    }
                }
            }
            Column {
                x: 545; y: 12; spacing: 9
                Text { text: "SCREEN & RUNTIME"; color: Theme.muted; font.pixelSize: 10; font.letterSpacing: 0.5 }
                Repeater {
                    model: root.checks.runtimeLines
                    delegate: Text { required property string modelData; width: 342; text: modelData; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 14 }
                }
                Text { width: 340; text: root.checks.gate; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap }
                Text { width: 340; text: "Last action: " + root.checks.lastAction; color: Theme.ink; font.pixelSize: 13; wrapMode: Text.WordWrap }
            }
        }
        Text {
            x: 30; y: 310; width: parent.width - 60; height: 28; font.pixelSize: 12; color: Theme.muted; wrapMode: Text.WordWrap
            text: root.checks.status || "Compare these labels with your physical buttons."
            textFormat: Text.PlainText
        }
        MountedPanel {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 65; color: Theme.surfaceColor("#bfd6c8")
            Row {
                x: 30; y: 12; spacing: 12
                Repeater {
                    model: ["Refresh display", root.checks.saving ? "Saving report…" : "Save report", "Reset checks", "Back to settings"]
                    delegate: CapButton {
                        required property int index; required property string modelData
                        objectName: "diagnostic-action-" + index
                        width: 205; height: 43; textSize: 14; label: modelData
                        tint: index === 0 ? Theme.blue : index === 3 ? Theme.pink : Theme.yellow
                        selected: root.takesFocus && root.checks.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
        }
    }
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset; anchors.topMargin: Theme.contentTopInset
        visible: root.checks.health
        Text { x: 16; y: 8; text: "System health"; font.family: Theme.displayFamily; font.pixelSize: 26; color: Theme.ink }
        Text { x: 16; y: 45; width: parent.width-32; text: root.checks.healthSummary; font.pixelSize: 12; color: Theme.muted; wrapMode: Text.Wrap; maximumLineCount: 3; elide: Text.ElideRight }
        ListView { id: health; x: 10; y: 102; width: parent.width*0.43; height: parent.height-y-12; clip: true
            model: ["Refresh setup",root.checks.saving ? "Saving…" : "Export support archive","Open reports in Files","Offline help","Back"].concat(root.checks.healthRows)
            currentIndex: root.checks.focusIndex
            onCurrentIndexChanged: if(currentIndex>=0) positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: SettingControl { required property int index; required property var modelData
                width: health.width; height: index<5 ? 43 : 60; flat: true; title: index<5 ? modelData : modelData.title
                detail: index<5 ? "" : modelData.state+" · "+modelData.games.length+" games"; kind: "action"
                selected: root.takesFocus && index===health.currentIndex; onActivated: root.checks.healthAction(index)
            }
        }
        Rectangle { x: health.x+health.width+12; y: 106; width: 1; height: health.height-8; color: Theme.divider }
        Flickable { id: healthDetail; x: health.x+health.width+30; y: 107; width: parent.width-x-20; height: parent.height-y-12; clip: true; contentWidth: width; contentHeight: healthBody.implicitHeight; boundsBehavior: Flickable.StopAtBounds
        Connections { target: root.checks; function onScrollRequested(direction) { healthDetail.contentY=Math.max(0,Math.min(Math.max(0,healthDetail.contentHeight-healthDetail.height),healthDetail.contentY+direction*38)) } }
        Column { id: healthBody; width: parent.width; spacing: 13
            Text { width: parent.width; text: root.checks.selectedHealth.title || "Game setup"; color: Theme.ink; font.pixelSize: 23; font.family: Theme.displayFamily }
            Text { width: parent.width; text: root.checks.selectedHealth.detail || "Import games to see their runtime and firmware requirements here."; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 15; wrapMode: Text.Wrap }
            Text { width: parent.width; text: (root.checks.selectedHealth.titles || []).slice(0,5).join("\n"); textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 13; maximumLineCount: 5; elide: Text.ElideRight }
            SettingControl { width: parent.width; height: 44; flat: true; title: "Game properties · Select"; kind: "action"; visible: !!root.checks.selectedHealth.id; onActivated: root.checks.openGameSetup() }
            Text { width: parent.width; text: root.checks.status; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.Wrap }
        }
        }
    }
}
