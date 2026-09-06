import QtQuick

Item {
    id: root
    required property var shell
    readonly property var manager: shell.libraryManager
    readonly property var files: manager.files
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    Panel { anchors.fill: parent }
    Item {
        anchors.fill: parent; anchors.margins: 10; clip: true
        Text { x: 28; y: 18; text: root.manager.route === "files" ? "Choose an Adventure file" : root.manager.title; color: Theme.ink; font.pixelSize: 29; font.weight: Font.DemiBold }
        Text {
            x: 29; y: 59; width: parent.width - 58; font.pixelSize: 14; color: Theme.muted; elide: Text.ElideMiddle; textFormat: Text.PlainText
            text: root.manager.route === "files" ? root.files.path : root.manager.saving ? "Saving… You can leave; the submitted save will finish." : "Your files stay where they are. Play setup follows device validation."
        }
        Rectangle { x: 0; y: 91; width: parent.width; height: parent.height - y; color: "#d4e2d6" }
        Item {
            anchors.fill: parent; visible: root.manager.route === "list"
            ControllerList {
                x: 24; y: 102; width: parent.width - 48; height: 204
                model: root.manager.rows; currentIndex: root.manager.rowIndex; namePrefix: "manage-row-"
                takesFocus: root.takesFocus && parent.visible && root.manager.zone === "list"
                onActivated: function(row) { root.shell.activate(row, "list") }
            }
            Text { x: 30; y: 128; visible: root.manager.rows.length === 0; text: "A new library, ready for your Adventures."; font.pixelSize: 24; color: Theme.ink }
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 72; color: "#bfd6c8"
                Row {
                    x: 30; y: 16; spacing: 18
                    Repeater {
                        model: ["Add Adventure", "Back to system menu"]
                        delegate: CapButton {
                            required property int index; required property string modelData
                            objectName: "manage-action-" + index
                            width: 330; height: 43; label: modelData; tint: index === 0 ? Theme.yellow : Theme.blue
                            selected: root.takesFocus && root.manager.route === "list" && root.manager.zone === "actions" && root.manager.focusIndex === index
                            onActivated: root.shell.activate(index, "actions")
                        }
                    }
                }
            }
        }
        Item {
            anchors.fill: parent; visible: root.manager.route === "edit"
            Grid {
                x: 30; y: 108; columns: 2; columnSpacing: 18; rowSpacing: 13
                Repeater {
                    model: root.manager.fields
                    delegate: CapButton {
                        required property int index; required property var modelData
                        objectName: "adventure-field-" + index
                        width: 415; height: 52; label: modelData.title; detail: modelData.value
                        tint: Theme.tabColors[index % Theme.tabColors.length]
                        selected: root.takesFocus && root.manager.route === "edit" && root.manager.focusIndex === index
                        onActivated: root.shell.activate(index)
                    }
                }
            }
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 72; color: "#bfd6c8"
                CapButton {
                    objectName: "adventure-save"; x: 30; y: 16; width: 415; height: 43
                    label: root.manager.saving ? "Saving…" : "Save Adventure"
                    selected: root.takesFocus && root.manager.route === "edit" && root.manager.focusIndex === 6
                    onActivated: root.shell.activate(6)
                }
                CapButton {
                    objectName: "adventure-cancel"; x: 463; y: 16; width: 415; height: 43; tint: Theme.pink
                    label: root.manager.saving ? "Back to library" : "Cancel changes"
                    selected: root.takesFocus && root.manager.route === "edit" && root.manager.focusIndex === 7
                    onActivated: root.shell.activate(7)
                }
            }
        }
        ControllerList {
            x: 24; y: 101; width: parent.width - 48; height: 204
            visible: root.manager.route === "world" || root.manager.route === "extras"
            model: root.manager.choices.map(function(choice) { return {id: choice.id, title: (choice.selected ? "●  " : "") + choice.title, subtitle: choice.selected ? "Selected" : "A to choose"} })
            currentIndex: root.manager.focusIndex; namePrefix: "manage-choice-"; tint: Theme.blue
            takesFocus: root.takesFocus && visible
            onActivated: function(row) { root.shell.activate(row) }
        }
        Item {
            anchors.fill: parent; visible: root.manager.route === "files"
            ControllerList {
                x: 24; y: 101; width: parent.width - 48; height: 204
                model: root.files.rows; currentIndex: root.files.rowIndex; namePrefix: "file-row-"; tint: Theme.blue
                takesFocus: root.takesFocus && parent.visible && root.files.zone === "list"
                onActivated: function(row) { root.shell.activate(row, "list") }
            }
            Text {
                x: 30; y: 314; width: parent.width - 60; elide: Text.ElideRight; font.pixelSize: 14; color: Theme.muted
                text: root.files.busy ? "Reading folder… B cancels." : root.files.error || (root.files.rows.length === 0 ? "This folder is empty." : "Folders and files · use the arrows to browse")
            }
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 72; color: "#bfd6c8"
                Row {
                    x: 16; y: 16; spacing: 9
                    Repeater {
                        model: root.files.actions
                        delegate: CapButton {
                            required property int index; required property var modelData
                            objectName: "file-action-" + index
                            width: 139; height: 43; textSize: 13; label: modelData.title; centered: true
                            tint: index === 5 ? Theme.pink : Theme.yellow
                            selected: root.takesFocus && root.manager.route === "files" && root.files.zone === "actions" && root.files.focusIndex === index
                            onActivated: root.shell.activate(index, "actions")
                        }
                    }
                }
            }
        }
        Text {
            x: 30; y: 311; width: parent.width - 60; elide: Text.ElideRight; color: "#853b24"; font.pixelSize: 14
            visible: root.manager.route !== "files"; text: root.manager.error; textFormat: Text.PlainText
        }
    }
}
