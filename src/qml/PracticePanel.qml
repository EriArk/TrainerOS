import QtQuick

Item {
    id: root
    required property var shell
    required property var controller
    readonly property bool choosing: controller.stage === "first" || controller.stage === "second"
    readonly property bool takesFocus: visible && !shell.drawerOpen && !shell.menuOpen && !shell.keyboard.open && !shell.notice.length
    enabled: takesFocus
    readonly property var fighters: controller.fighters
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset; anchors.topMargin: Theme.contentTopInset
        PageHeader { id: heading; title: "Practice"; subtitle: "Emerald · Gen III simulation"; compact: true }
        Rectangle {
            id: field
            y: heading.height; width: parent.width; height: parent.height - y
            clip: true; color: "#bbd993"
            gradient: Gradient {
                GradientStop { position: 0; color: "#c4dea1" }
                GradientStop { position: 0.68; color: "#96c67c" }
                GradientStop { position: 1; color: "#6c9e67" }
            }
            // Original, lightweight meadow geometry; no cartridge graphics.
            Repeater {
                model: 25
                Rectangle {
                    required property int index
                    x: (index * 157 % 977) / 977 * field.width; y: (index * 73 % 241) / 241 * field.height
                    width: 4; height: 8; rotation: index % 2 ? 22 : -22; color: "#54864f"; opacity: 0.18
                }
            }
            Rectangle { y: 12; width: parent.width; height: 6; color: "#709867"; opacity: 0.35 }
            Rectangle { y: 22; width: parent.width; height: 3; color: "#ecedbb"; opacity: 0.6 }
            Item {
                visible: root.choosing; anchors.fill: parent
                Text { x: 20; y: 16; width: parent.width - 40; text: root.controller.message; color: Theme.ink; font.pixelSize: 21; font.bold: true; wrapMode: Text.WordWrap }
                Grid {
                    id: partners
                    x: 20; y: 63; width: parent.width * 0.58; columns: 2; spacing: 12
                    enabled: root.controller.ready
                    Repeater {
                        model: root.controller.candidates
                        CapButton {
                            deferredFocus: true
                            required property int index; required property var modelData
                            objectName: "practice-partner-" + index
                            width: (partners.width - 12) / 2; height: Math.min(68, (field.height - 130) / 3)
                            label: modelData.name || ""; detail: "Lv. " + (modelData.level || "") + (modelData.chosen ? " · First partner" : "")
                            textSize: 15; contentInset: 65; tint: modelData.chosen ? Theme.yellow : Theme.green
                            selected: root.takesFocus && root.controller.ready && root.controller.focusIndex === index
                            onActivated: root.controller.activate(index)
                            Image { x: 8; y: 7; width: 49; height: parent.height - 14; source: modelData.art && modelData.art.url ? modelData.art.url : ""; fillMode: Image.PreserveAspectFit; asynchronous: true }
                        }
                    }
                }
                PracticeFighter {
                    x: parent.width * 0.64; y: 64; width: parent.width * 0.32; height: parent.height - 91
                    fighter: root.controller.candidates[root.controller.focusIndex] || ({})
                    showHealth: false; playing: root.takesFocus
                }
                CapButton {
                    deferredFocus: true
                    objectName: "practice-unavailable"
                    anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 18
                    width: 200; height: 40; label: "Back"; centered: true; tint: Theme.blue
                    visible: !root.controller.ready; selected: root.takesFocus && visible
                    onActivated: root.controller.activate(-1)
                }
            }
            Item {
                visible: !root.choosing && root.controller.stage !== "error"
                x: 20; y: 24; width: parent.width - 40; height: parent.height - controls.height - 34
                PracticeFighter {
                    x: parent.width * 0.06; y: parent.height * 0.19; width: parent.width * 0.37; height: parent.height * 0.81
                    fighter: root.fighters[0] || ({}); playing: root.takesFocus
                    striking: root.controller.stage === "events" && root.controller.event.kind === "move" && root.controller.event.actor === 0
                }
                PracticeFighter {
                    x: parent.width * 0.58; y: 0; width: parent.width * 0.32; height: parent.height * 0.80
                    fighter: root.fighters[1] || ({}); opponent: true; playing: root.takesFocus
                    striking: root.controller.stage === "events" && root.controller.event.kind === "move" && root.controller.event.actor === 1
                }
            }
            Rectangle {
                id: controls
                anchors.bottom: parent.bottom; width: parent.width
                height: 135
                visible: !root.choosing
                color: "#e7edcf"; border.color: "#779579"; border.width: 2
                Text {
                    x: 19; y: 10; width: parent.width - 38; text: root.controller.message
                    color: Theme.ink; font.pixelSize: 18; font.bold: true; wrapMode: Text.WordWrap
                }
                Grid {
                    x: 18; y: 39; width: parent.width - 36; columns: 2; spacing: 8
                    visible: root.controller.stage === "moves"
                    Repeater {
                        model: root.controller.moves
                        CapButton {
                            deferredFocus: true
                            required property int index; required property var modelData
                            objectName: "practice-move-" + index
                            width: (parent.width - 8) / 2; height: 38
                            label: modelData.move + (modelData.maxPp ? "    " + modelData.pp + "/" + modelData.maxPp + " PP" : "")
                            tint: [Theme.blue, Theme.green, Theme.yellow, Theme.pink][index % 4]; textSize: 15
                            selected: root.takesFocus && root.controller.focusIndex === index
                            onActivated: root.controller.activate(index)
                        }
                    }
                }
                CapButton {
                    deferredFocus: true
                    anchors.right: parent.right; anchors.rightMargin: 18; anchors.bottom: parent.bottom; anchors.bottomMargin: 12
                    width: 200; height: 35; centered: true; tint: Theme.yellow; textSize: 15
                    visible: ["ready", "events", "finished", "error"].indexOf(root.controller.stage) >= 0
                    label: root.controller.stage === "ready" ? "Let's begin!" : root.controller.stage === "events" ? "Next" : "Choose partners"
                    selected: root.takesFocus && visible
                    onActivated: root.controller.activate(0)
                }
            }
        }
    }
}
