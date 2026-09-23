import QtQuick

Item {
    id: root
    required property var shell
    readonly property var activity: shell.party.activities
    readonly property bool takesFocus: visible && !shell.drawerOpen && !shell.menuOpen && !shell.keyboard.open && !shell.notice.length
    enabled: !shell.drawerOpen
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset
        anchors.topMargin: Theme.contentTopInset; anchors.bottomMargin: Theme.panelInset + 30; clip: true
        PageHeader {
            id: heading; compact: true; title: root.activity.page.title; trailing: "Pokémon Center"
            subtitle: root.activity.sample ? "Development rehearsal · no game, save or connection changes" : root.shell.party.title || "Choose an Adventure"
        }
        MountedPanel {
            y: heading.height; width: parent.width; height: parent.height - y; color: "#dce6dc"
            Text { x: 24; y: 15; width: parent.width - 48; text: root.activity.page.message; color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap }
            Column {
                x: 24; y: 52; spacing: 13; visible: root.activity.route === "menu"
                Repeater {
                    model: ["Party Playroom", "Practice", "Link Counter"]
                    CapButton {
                        required property int index; required property string modelData
                        objectName: "activity-menu-" + index; width: 384; height: 55; label: modelData
                        tint: index === 0 ? Theme.green : index === 1 ? Theme.blue : Theme.yellow
                        selected: root.takesFocus && root.activity.route === "menu" && root.activity.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            Text {
                x: 449; y: 68; width: parent.width - 478; visible: root.activity.route === "menu"
                text: root.activity.focusIndex === 0 ? "Spend a quiet moment with your Party.\n\nA separate space from Party management."
                    : root.activity.focusIndex === 1 ? "Prepare a read-only practice session.\n\nNo rewards or changes to the saved game."
                    : "Review a partner and proposal.\n\nTransfers require supported games on both devices."
                color: Theme.ink; font.pixelSize: 20; wrapMode: Text.WordWrap
            }
            Text {
                x: 28; y: 95; width: parent.width - 56; visible: !root.activity.sample && root.activity.route !== "menu"
                text: "Not available for this Adventure yet.\nYou can return to Party or use ordinary save backups."; color: Theme.ink; font.pixelSize: 24; wrapMode: Text.WordWrap
            }
            Item {
                anchors.fill: parent; visible: root.activity.sample && root.activity.route === "playroom"
                Repeater {
                    model: 2
                    Item {
                        required property int index
                        x: 40 + index * 262; y: 61; width: 234; height: 160
                        Rectangle { x: 57; y: 103; width: 112; height: 18; radius: 9; color: "#35677e6b" }
                        Rectangle {
                            x: 56 + (root.activity.focusIndex === index && root.activity.reaction.indexOf("called") >= 0 ? 20 : 0)
                            y: 0; width: 114; height: 114; radius: 57; color: index ? Theme.blue : Theme.green; border.color: "#728f80"; border.width: 3
                            Behavior on x { NumberAnimation { duration: root.visible ? Theme.motion(160) : 0 } }
                            Text { anchors.centerIn: parent; text: index + 1; color: Theme.ink; font.pixelSize: 38; font.bold: true }
                        }
                        CapButton {
                            objectName: "playroom-actor-" + index; y: 134; width: parent.width; height: 42
                            label: "Sample partner " + (index + 1); tint: Theme.green
                            selected: root.takesFocus && root.activity.route === "playroom" && root.activity.sample && root.activity.focusIndex === index
                            onActivated: root.shell.activate(index)
                        }
                    }
                }
                MountedPanel {
                    anchors.right: parent.right; y: 60; width: 304; height: 174; color: "#c8d9cd"
                    Text { x: 18; y: 15; width: parent.width - 36; text: "PORTRAIT\nNo sprite art installed\n\n" + (root.activity.reaction || "Select a partner, call it over or offer a greeting."); color: Theme.ink; font.pixelSize: 16; wrapMode: Text.WordWrap }
                }
            }
            Item {
                anchors.fill: parent; visible: root.activity.sample && root.activity.route === "practice"
                Row {
                    x: 40; y: 70; spacing: 30
                    Repeater {
                        model: ["Sample partner 1", "Sample partner 2"]
                        Rectangle {
                            required property string modelData
                            width: 350; height: 110; radius: 20; color: "#c4d9da"; border.color: "#8aa89a"; border.width: 2
                            Text { anchors.centerIn: parent; text: modelData + "\nCopied preview slot"; horizontalAlignment: Text.AlignHCenter; color: Theme.ink; font.pixelSize: 22 }
                        }
                    }
                }
                Text { x: 44; y: 199; width: parent.width - 88; text: root.activity.stage === "setup" ? "Rules engine unavailable · starting a battle is disabled" : "Preview only · HP, PP, items, EXP and money stay unchanged"; color: Theme.ink; font.pixelSize: 17; wrapMode: Text.WordWrap }
            }
            Column {
                x: 32; y: 68; width: parent.width - 64; spacing: 20
                visible: root.activity.sample && root.activity.route === "link"
                Text { width: parent.width; text: root.activity.stage === "setup" ? "Sample partner device" : root.activity.stage === "review" ? "Your sample record  ↔  Partner's sample record" : "Connection needs attention"; color: Theme.ink; font.pixelSize: 26; font.bold: true; wrapMode: Text.WordWrap }
                Text { width: parent.width; text: root.activity.stage === "setup" ? "Offline rehearsal · no Bluetooth scan"
                    : root.activity.stage === "review" ? "Compatibility: unverified\nBoth confirmations and protected saves are required. No transfer is enabled."
                    : "Neither save was changed. This preview started no transaction."; color: Theme.muted; font.pixelSize: 19; wrapMode: Text.WordWrap }
            }
            CapButton {
                objectName: "activity-primary"; x: 24; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; width: 395; height: 44
                visible: root.activity.route !== "menu" && !(root.activity.sample && root.activity.route === "playroom")
                label: root.activity.page.action; tint: Theme.blue; selected: root.takesFocus && visible
                onActivated: root.shell.activate(root.activity.focusIndex)
            }
            Text {
                x: 28; anchors.bottom: parent.bottom; anchors.bottomMargin: 23
                visible: root.activity.route === "menu" || root.activity.sample && root.activity.route === "playroom"
                text: ""
                color: Theme.ink; font.pixelSize: 16
            }
        }
    }
}
