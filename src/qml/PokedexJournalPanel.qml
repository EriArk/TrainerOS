import QtQuick

Item {
    id: root
    required property var shell
    readonly property var journal: shell.pokedex.journal
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.keyboard.open && shell.notice.length === 0
    Rectangle { anchors.fill: parent; color: Theme.paper; ShellBackgroundPattern { anchors.fill: parent } }
    MouseArea { anchors.fill: parent }
    PageHeader {
        id: journalHeader; compact: true; title: "Field journal · " + root.journal.name
        subtitle: "Your own marks · shared across your library · no game-save changes"
    }
    MountedPanel { x: 0; y: journalHeader.height; width: parent.width; height: parent.height - y; color: "#d7e2d6" }
    Repeater {
        model: root.journal.fields
        delegate: CapButton {
            required property int index
            required property var modelData
            objectName: "journal-field-" + index
            x: index === 1 ? root.width / 2 + 9 : 27
            y: index === 2 ? 187 : 105
            width: index === 2 ? root.width - 54 : (root.width - 72) / 2
            height: index === 2 ? 73 : 59
            label: modelData.value; detail: modelData.label
            tint: index === 0 ? Theme.blue : index === 1 ? Theme.green : Theme.yellow
            selected: root.takesFocus && root.journal.focusIndex === index
            onActivated: root.journal.activate(index)
        }
    }
    MountedPanel {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 106; color: "#b7cfbf"
        Text { x: 28; y: 9; width: parent.width - 56; height: 37; text: root.journal.error.length ? root.journal.error : root.journal.saving ? "Saving your field journal…" : "A · Change field     Y · Save journal     B · Discard draft"; color: root.journal.error.length ? "#873c32" : Theme.ink; textFormat: Text.PlainText; font.pixelSize: 15; wrapMode: Text.WordWrap }
        CapButton { x: 27; y: 51; width: 338; height: 41; label: root.journal.saving ? "Saving…" : "Y   Save journal"; tint: Theme.yellow; onActivated: root.journal.submit() }
        CapButton { x: 383; y: 51; width: 233; height: 41; label: "B   Discard draft"; tint: Theme.blue; onActivated: root.journal.back() }
        Text { x: 641; y: 61; text: "MANUAL TRAINER RECORD"; color: Theme.muted; font.pixelSize: 11; font.letterSpacing: 1 }
    }
}
