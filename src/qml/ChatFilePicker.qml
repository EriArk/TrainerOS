import QtQuick
Item {
    id: root
    required property var files
    readonly property var picker: files.picker
    visible: picker.open
    Rectangle { anchors.fill: parent; color: "#98102022" }
    MouseArea { anchors.fill: parent }
    Panel {
        anchors.centerIn: parent; width: Math.max(0,Math.min(740,parent.width-48)); height: Math.max(0,Math.min(430,parent.height-42))
        Text { x: 26; y: 22; text: "Attach a file"; font.family: Theme.displayFamily; font.pixelSize: 25; color: Theme.ink }
        SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 22; y: 18; icon: "x"; label: "Close"; onClicked: root.picker.activate(5,"actions") }
        Text { x: 26; y: 63; width: parent.width-52; text: root.picker.path; elide: Text.ElideMiddle; textFormat: Text.PlainText; font.pixelSize: 13; color: Theme.muted }
        ControllerList { x: 24; y: 92; width: Math.max(0,parent.width-48); height: Math.max(0,parent.height-176)
            model: root.picker.rows; currentIndex: root.picker.rowIndex; takesFocus: root.picker.zone==="list"
            namePrefix: "chat-file-"; tint: Theme.blue; onActivated: row => root.picker.activate(row,"list") }
        Text { x: 26; anchors.bottom: actions.top; anchors.bottomMargin: 10; width: parent.width-52
            text: root.picker.busy ? "Loading…" : root.picker.error; color: Theme.muted; font.pixelSize: 12; textFormat: Text.PlainText }
        Row { id: actions; x: 24; anchors.bottom: parent.bottom; anchors.bottomMargin: 22; spacing: 6
            Repeater { model: root.picker.actions
                CapButton { required property int index; required property var modelData
                    width: (actions.parent.width-78)/6; height: 34; label: modelData.title; textSize: 11
                    tint: index===5?Theme.pink:Theme.blue; selected: root.picker.zone==="actions"&&root.picker.focusIndex===index
                    onActivated: root.picker.activate(index,"actions") }
            }
        }
    }
}
