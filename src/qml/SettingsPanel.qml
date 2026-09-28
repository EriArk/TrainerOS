import QtQuick
Item {
    id: root
    required property var shell
    readonly property var settings: shell.settings
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.notice.length && !shell.keyboard.open && !shell.trainer.picker.open && !shell.libraryTools.open
    readonly property string localError: settings.category===4 && shell.trainer.editing ? shell.trainer.error
        : (settings.category===0 || settings.category===8 || settings.category===9) && settings.error.length ? settings.error
        : [0,1,5,10].includes(settings.category) ? shell.device.error : ""
    readonly property string statusText: settings.category===4 && shell.trainer.saving || settings.saving ? "Saving..."
        : localError || (shell.hall.account.open ? shell.hall.account.status : settings.category===8 ? settings.libraryStatus : "")

    Item {
        anchors.fill: parent; anchors.margins: 12
        PageHeader { id: heading; compact: true; title: "Settings" }
        MountedPanel {
            y: heading.height; width: parent.width; height: parent.height-y; color: "#d4e2d6"
            Rectangle { x: 0; y: 0; width: 245; height: parent.height; color: "#c5d8ca" }
            Column { x: 13; y: 13; spacing: 4
                Repeater { model: root.settings.categories
                    Rectangle {
                        required property int index; required property string modelData
                        objectName: "settings-category-"+index
                        width: 218; height: Math.min(32,Math.floor((parent.parent.height-26-(root.settings.categories.length-1)*4)/root.settings.categories.length)); radius: 9
                        readonly property bool selected: root.takesFocus && !root.settings.controlsFocused && root.settings.category===index
                        color: root.settings.category===index ? "#f5e5a7" : "transparent"
                        border.width: selected ? 3 : 0; border.color: Theme.focus
                        onSelectedChanged: if(selected && visible) forceActiveFocus(Qt.OtherFocusReason)
                        onVisibleChanged: if(selected && visible) forceActiveFocus(Qt.OtherFocusReason)
                        Component.onCompleted: if(selected && visible) forceActiveFocus(Qt.OtherFocusReason)
                        Text { x: 14; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: Theme.ink; font.pixelSize: 18; font.bold: root.settings.category===index }
                        TapHandler { enabled: !root.shell.trainer.editing && !root.shell.hall.account.open && !root.settings.storage.busy; onTapped: root.settings.selectCategory(index,true) }
                    }
                }
            }
            Item { x: 260; width: parent.width-x-16; height: parent.height
                ConnectionsPane { anchors.fill: parent; shell: root.shell; visible: root.settings.category===10 }
                Item {
                    anchors.fill: parent; visible: root.settings.storage.open
                    Text { x: 5; y: 13; text: "Game storage"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 25 }
                    Text { x: 5; y: 48; width: parent.width-10; text: "Choose where to look for games. Existing files stay in place."; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap }
                    StorageChoices { x: 0; y: 75; width: parent.width; height: parent.height-y-43; compact: true; flow: root.settings.storage; takesFocus: root.takesFocus && root.settings.controlsFocused }
                    Text { x: 5; y: parent.height-37; width: parent.width-10; height: 33; text: root.settings.storage.busy ? "Preparing your library…" : root.settings.storage.error || root.settings.storage.rows[root.settings.storage.focusIndex]?.path || ""; textFormat: Text.PlainText; color: root.settings.storage.error.length ? "#853b24" : Theme.muted; font.pixelSize: 12; wrapMode: Text.WrapAnywhere; maximumLineCount: 2; elide: Text.ElideMiddle }
                }
                Column { x: 0; y: root.settings.category===5 ? 158 : 13; width: parent.width; spacing: root.settings.category===4 || root.settings.category===5 || root.settings.category===8 || root.shell.trainer.editing || root.shell.hall.account.open ? 5 : 12; visible: root.settings.category!==6 && root.settings.category!==10 && !root.settings.storage.open
                    Repeater { model: root.shell.hall.account.open ? root.shell.hall.account.rows.map(r => ({title: r.label, detail: r.detail, kind: r.enabled ? "action" : "unavailable"})) : root.shell.trainer.editing ? [
                        {title: "Name", kind: "action", detail: root.shell.trainer.draftName || "Choose your name"},
                        {title: "Emblem", kind: "action", detail: root.shell.trainer.draftEmblem},
                        {title: "Favorite", kind: "action", detail: root.shell.trainer.draftFavorite},
                        {title: "Save Trainer", kind: "action", detail: ""},
                        {title: "Cancel", kind: "action", detail: ""}
                    ] : root.settings.controls
                        SettingControl {
                            required property int index; required property var modelData
                            objectName: (root.shell.hall.account.open ? "achievement-account-" : "settings-control-")+index; width: parent.width; height: root.shell.hall.account.open ? 56 : root.shell.trainer.editing ? 44 : root.settings.category===5 ? 32 : root.settings.category===4 ? 44 : root.settings.category===8 ? 58 : 68
                            title: root.settings.category===4 && !root.shell.trainer.editing && !root.shell.hall.account.open && index===0 ? (root.shell.trainer.exists ? "Edit Trainer" : "Create Trainer") : root.settings.category===4 && !root.shell.trainer.editing && !root.shell.hall.account.open && index===2 && root.shell.sampleLibrary ? "Preview registration & PIN" : modelData.title
                            detail: root.settings.category===4 && !root.shell.trainer.editing && !root.shell.hall.account.open && index===0 ? (root.shell.trainer.profile.name || "Give your journey a name") : root.settings.category===4 && !root.shell.trainer.editing && !root.shell.hall.account.open && index===2 && root.shell.sampleLibrary ? "Development preview only" : modelData.detail
                            kind: root.settings.category===4 && !root.shell.trainer.editing && !root.shell.hall.account.open && index===2 && root.shell.sampleLibrary ? "action" : modelData.kind
                            checked: root.settings.category===9 ? root.settings.readOnlySaves : root.settings.category===8 ? root.settings.worldEditing : root.settings.category===2 ? root.settings.videoPreviews : root.settings.reducedMotion
                            level: kind==="volume" ? root.shell.device.rows[0].level : kind==="brightness" ? root.shell.device.rows[1].level : -1
                            muted: kind==="volume" && root.shell.device.rows[0].muted
                            selected: root.takesFocus && root.settings.controlsFocused && (root.shell.hall.account.open ? root.shell.hall.account.focusIndex : root.shell.trainer.editing ? root.shell.trainer.focusIndex : root.settings.rowFocus)===index
                            onActivated: root.shell.activate(index)
                            onLevelRequested: value => root.shell.device.setQuickLevel(kind==="volume"?0:1,value)
                        }
                    }
                }
                Column { x: 0; y: 10; width: parent.width; spacing: 4; visible: root.settings.category===5
                    Repeater { model: root.shell.device.status
                        Rectangle {
                            required property var modelData
                            width: parent.width; height: 44; radius: 7; color: "#eaf0e6"
                            Text { x: 13; y: 4; text: modelData.title; color: Theme.ink; font.pixelSize: 16; font.bold: true }
                            Text { x: 13; y: 24; width: parent.width-26; text: modelData.value || "Unavailable"; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.muted; font.pixelSize: 14 }
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

                    CapButton { objectName: "credits-back"; x: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 48; width: 240; height: 38; label: "Categories"; selected: root.takesFocus && root.settings.controlsFocused && root.settings.category===6; onActivated: root.settings.selectCategory(6,false) }
                }
                Text {
                    objectName: "settings-status"
                    visible: root.settings.category!==10 && !root.settings.storage.open
                    x: 5; y: parent.height-58; width: parent.width-10; height: 52
                    text: root.statusText; textFormat: Text.PlainText
                    font.pixelSize: 14; color: root.localError.length ? "#853b24" : Theme.muted
                    wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight
                }
            }
        }
    }
    SpeciesPickerPanel { anchors.fill: parent; picker: root.shell.trainer.picker; visible: root.shell.trainer.picker.open; takesFocus: visible && !root.shell.menuOpen && !root.shell.keyboard.open && !root.shell.notice.length }
}
