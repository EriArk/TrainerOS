import QtQuick

Item {
    id: root
    required property var shell
    readonly property var editor: shell.hall.editor
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    Rectangle { anchors.fill: parent; color: Theme.paper; ShellBackgroundPattern { anchors.fill: parent } }
    PageHeader {
        id: editorHeader; compact: true
        title: root.editor.route === "team" ? "Your champion team" : root.editor.route === "adventures" ? "Choose an Adventure" : "A journey to remember"
        subtitle: root.editor.route === "adventures" ? (root.editor.query.length ? "Search: " + root.editor.query : "Your library · X to search · ← / → jump 8") : "Your own record · optional details can stay unknown"
    }
    MountedPanel { x: 0; y: editorHeader.height; width: parent.width; height: parent.height - y; color: "#dce4d8" }
    Item {
        anchors.fill: parent; visible: root.editor.route !== "adventures"
        Repeater {
            model: root.editor.fields
            delegate: CapButton {
                required property int index
                required property var modelData
                readonly property bool team: root.editor.route === "team"
                readonly property int cell: team ? index : Math.max(0, index - 1)
                objectName: "memory-field-" + index
                x: !team && index === 0 ? 25 : 25 + (cell % 2) * ((root.width - 68) / 2 + 18)
                y: team ? 91 + Math.floor(cell / 2) * 72 : index === 0 ? 91 : 163 + Math.floor(cell / 2) * 72
                width: !team && index === 0 ? root.width - 50 : (root.width - 68) / 2
                height: 58
                label: modelData.value
                detail: modelData.label + (team ? " · " + modelData.subtitle : "")
                tint: team ? Theme.tabColors[index % Theme.tabColors.length] : index === 0 ? Theme.yellow : index === 3 ? "#c7c0df" : Theme.blue
                selected: root.takesFocus && root.editor.focusIndex === index
                onActivated: root.editor.activate(index)
            }
        }
    }
    ControllerList {
        objectName: "memory-adventure-list"
        x: 21; y: 82; width: parent.width - 42; height: 216
        visible: root.editor.route === "adventures"
        model: root.editor.rows; currentIndex: root.editor.focusIndex; namePrefix: "memory-adventure-"
        takesFocus: root.takesFocus && visible
        onActivated: row => root.editor.activate(row)
    }
    Text {
        x: 29; y: 142; width: parent.width - 58
        visible: root.editor.route === "adventures" && root.editor.rows.length === 0
        text: root.editor.query.length ? "No matches. Press X to change your search." : "Add an Adventure in Worlds first. Press B to return."
        color: Theme.ink; font.pixelSize: 22; wrapMode: Text.WordWrap
    }
    MountedPanel {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 102; color: "#b8cbbc"
        Text {
            x: 26; y: 7; width: parent.width - 52; height: 35; textFormat: Text.PlainText
            text: root.editor.error.length ? root.editor.error : root.editor.saving ? "Saving your memory…" : root.editor.route === "team" ? "A · Name     X · Level     B · Back to your memory" : root.editor.route === "adventures" ? "A · Choose     X · Search     B · Back" : "A · Edit field     Y · Save memory     B · Discard draft"
            color: root.editor.error.length ? "#873c32" : Theme.ink; font.pixelSize: 15; wrapMode: Text.WordWrap
        }
        CapButton {
            objectName: "memory-save"; x: 25; y: 48; width: 337; height: 40
            label: root.editor.route === "form" ? (root.editor.saving ? "Saving…" : "Y   Save this memory") : "B   Back to your memory"
            tint: Theme.yellow
            selected: root.takesFocus && root.editor.route === "adventures" && root.editor.rows.length === 0
            onActivated: root.editor.route === "form" ? root.editor.submit() : root.editor.back()
        }
        // Empty pickers still own a visible, deterministic recovery focus.
        CapButton {
            objectName: "memory-back"; x: 380; y: 48; width: 225; height: 40
            label: root.editor.route === "form" ? "B   Discard draft" : "B   Back"
            tint: Theme.blue
            visible: root.editor.route === "form"
            onActivated: root.editor.back()
        }
        Text { anchors.right: parent.right; anchors.rightMargin: 26; y: 58; text: "LOCAL TRAINER JOURNAL"; color: Theme.muted; font.pixelSize: 11; font.letterSpacing: 1 }
    }
}
