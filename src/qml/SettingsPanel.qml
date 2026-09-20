import QtQuick
Item {
    id: root
    required property var shell
    readonly property var settings: shell.settings
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.notice.length
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset; anchors.topMargin: Theme.contentTopInset
        PageHeader { id: heading; compact: true; title: "Settings"; subtitle: "A little more you." }
        MountedPanel {
            y: heading.height; width: parent.width; height: parent.height-y; color: "#d4e2d6"
            Rectangle { x: 0; y: 0; width: 245; height: parent.height; color: "#c5d8ca" }
            Column { x: 13; y: 13; spacing: 6
                Repeater { model: root.settings.categories
                    Rectangle {
                        required property int index; required property string modelData
                        objectName: "settings-category-"+index
                        width: 218; height: 36; radius: 9
                        readonly property bool selected: root.takesFocus && !root.settings.controlsFocused && root.settings.category===index
                        color: root.settings.category===index ? "#f5e5a7" : "transparent"
                        border.width: selected ? 3 : 0; border.color: Theme.focus
                        onSelectedChanged: if(selected && visible) forceActiveFocus(Qt.OtherFocusReason)
                        onVisibleChanged: if(selected && visible) forceActiveFocus(Qt.OtherFocusReason)
                        Component.onCompleted: if(selected && visible) forceActiveFocus(Qt.OtherFocusReason)
                        Text { x: 14; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: Theme.ink; font.pixelSize: 18; font.bold: root.settings.category===index }
                        TapHandler { onTapped: root.settings.selectCategory(index,true) }
                    }
                }
            }
            Item { x: 260; width: parent.width-x-16; height: parent.height
                Column { x: 0; y: 13; width: parent.width; spacing: 12; visible: root.settings.category!==6
                    Repeater { model: root.settings.controls
                        SettingControl {
                            required property int index; required property var modelData
                            objectName: "settings-control-"+index; width: parent.width; height: 78
                            title: modelData.title; detail: modelData.detail; kind: modelData.kind
                            checked: root.settings.reducedMotion
                            level: kind==="volume" ? root.shell.device.rows[0].level : kind==="brightness" ? root.shell.device.rows[1].level : -1
                            muted: kind==="volume" && root.shell.device.rows[0].muted
                            selected: root.takesFocus && root.settings.controlsFocused && root.settings.rowFocus===index
                            onActivated: root.settings.activateRow(index)
                            onLevelRequested: value => root.shell.device.setQuickLevel(kind==="volume"?0:1,value)
                        }
                    }
                }
                Item { anchors.fill: parent; visible: root.settings.category===6
                Column {
                    x: 22; y: 16; spacing: 12; width: parent.width - 44
                    Text { text: "Pokémon League badge SVG recreations"; color: Theme.ink; font.pixelSize: 18; font.bold: true }
                    Text { text: "Stephen Griffiths · 2011"; color: Theme.ink; font.pixelSize: 18 }
                    Text { text: "Creative Commons Attribution 3.0 Unported\nhttps://creativecommons.org/licenses/by/3.0/\nhttps://github.com/SteGriff/pokemon-badges"; color: Theme.muted; font.pixelSize: 14; lineHeight: 1.3 }
                    Text { width: parent.width; text: "Adapted for TrainerOS: vector layers isolated, padded and rendered for the handheld. Badge shapes and colors preserved. Original source references: Bulbapedia.\nPokémon designs belong to their respective owners."; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; lineHeight: 1.2 }
                }

                    CapButton { objectName: "credits-back"; x: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 48; width: 240; height: 38; label: "B  Categories"; selected: root.takesFocus && root.settings.controlsFocused && root.settings.category===6; onActivated: root.settings.selectCategory(6,false) }
                }
                Text { x: 5; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; width: parent.width-10; text: root.settings.saving ? "Saving..." : root.settings.error || root.shell.device.error || (root.settings.controlsFocused ? "Left / Right adjust     B Categories" : "A Open category     B Back"); font.pixelSize: 14; color: root.settings.error.length || root.shell.device.error.length ? "#853b24" : Theme.muted; wrapMode: Text.WordWrap }
            }
        }
    }
}
