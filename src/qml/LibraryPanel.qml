import QtQuick

Item {
    id: root
    required property var shell
    readonly property var manager: shell.libraryManager
    readonly property var files: manager.files
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    // The main chassis owns the recessed surface for pages and services alike.
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset
        anchors.topMargin: Theme.contentTopInset; clip: true
        PageHeader {
            id: libraryHeader; compact: true; subtitleElide: Text.ElideMiddle
            title: root.manager.route === "files" ? "Choose an Adventure file" : root.manager.title
            subtitle: root.manager.route === "files" ? root.files.path : root.manager.saving ? "Saving… You can leave; the submitted save will finish." : "Link your own file. Supported play setups are applied automatically."
        }
        MountedPanel { x: 0; y: libraryHeader.height; width: parent.width; height: parent.height - y; color: "#d4e2d6" }
        Item {
            anchors.fill: parent; visible: root.manager.route === "list"
            ControllerList {
                x: 24; y: libraryHeader.height + 11; width: parent.width - 48; height: 216
                model: root.manager.rows; currentIndex: root.manager.rowIndex; namePrefix: "manage-row-"
                takesFocus: root.takesFocus && parent.visible && root.manager.zone === "list"
                onActivated: function(row) { root.shell.activate(row, "list") }
            }
            Text { x: 30; y: 128; visible: root.manager.rows.length === 0; text: "A new library, ready for your Adventures."; font.pixelSize: 24; color: Theme.ink }
            MountedPanel {
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
                x: 30; y: libraryHeader.height + 17; columns: 2; columnSpacing: 18; rowSpacing: 13
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
            MountedPanel {
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
            x: 24; y: libraryHeader.height + 11; width: parent.width - 48; height: 216
            visible: root.manager.route === "world" || root.manager.route === "extras" || root.manager.route === "edition"
            model: root.manager.choices.map(function(choice) { return {id: choice.id, title: (choice.selected ? "●  " : "") + choice.title, subtitle: choice.selected ? "Selected" : "A to choose"} })
            currentIndex: root.manager.focusIndex; namePrefix: "manage-choice-"; tint: Theme.blue
            takesFocus: root.takesFocus && visible
            onActivated: function(row) { root.shell.activate(row) }
        }
        Item {
            anchors.fill: parent; visible: root.manager.route === "files"
            ControllerList {
                x: 24; y: libraryHeader.height + 11; width: parent.width - 48; height: 216
                model: root.files.rows; currentIndex: root.files.rowIndex; namePrefix: "file-row-"; tint: Theme.blue
                takesFocus: root.takesFocus && parent.visible && root.files.zone === "list"
                onActivated: function(row) { root.shell.activate(row, "list") }
            }
            Text {
                x: 30; y: 314; width: parent.width - 60; elide: Text.ElideRight; font.pixelSize: 14; color: Theme.muted
                text: root.files.busy ? "Reading folder… B cancels." : root.files.error || (root.files.rows.length === 0 ? "This folder is empty." : "Folders and files · use the arrows to browse")
            }
            MountedPanel {
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
