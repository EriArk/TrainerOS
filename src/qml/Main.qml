import QtQuick
import QtQuick.Window
import QtQuick.Shapes

Window {
    id: window
    objectName: "trainer-window"
    width: 960; height: 540
    minimumWidth: 800; minimumHeight: 450
    visible: true
    title: "TrainerOS — native prototype"
    color: Theme.chassisDark
    AdventureExitWindow {
        presentation: adventureExitPresentation
        width: window.width; height: window.height
        x: window.x; y: window.y
    }
    Binding { target: Theme; property: "themeId"; value: shell.settings.theme }
    Binding { target: Theme; property: "reducedMotion"; value: shell.settings.reducedMotion }
    onClosing: function(close) { close.accepted = false; sessionState.requestExit() }
    Connections {
        target: controllerInput
        function onConfirmPressed() {
            const control = window.activeFocusItem;
            if (control && typeof control.pressFeedback === "function") control.pressFeedback();
        }
    }
    Item {
        id: viewport
        objectName: "viewport"
        anchors.centerIn: parent
        width: Theme.viewportWidth; height: Theme.viewportHeight
        scale: Math.min(window.width / width, window.height / height)
        clip: true
        Item {
        anchors.fill: parent
        visible: !sessionState.blocked
        enabled: !adventureLaunch.active
        ChassisFrame { anchors.fill: parent }
        Item {
            id: brand
            objectName: "brand-extension"
            width: Theme.brandWidth; height: Theme.brandHeight
            z: 1
            Text {
                anchors.centerIn: parent; width: parent.width - 24
                horizontalAlignment: Text.AlignHCenter
                text: "TRAINER OS"; color: "#edf5e9"; font.pixelSize: 25; font.weight: Font.Bold
                font.family: Theme.brandFamily; font.letterSpacing: 0.6
            }
        }
        Row {
            objectName: "primary-tabs"
            x: Theme.brandWidth; y: 0; spacing: Theme.tabSpacing
            z: 1 // Individual chassis-mounted keys; modal surfaces stay above them.
            Repeater {
                model: ["Home", "Worlds", "Pokédex", "Trainer", "Hall of Fame"]
                delegate: Item {
                    id: tab
                    required property int index
                    required property string modelData
                    objectName: "primary-tab-" + index
                    width: Theme.tabWidth
                    height: Theme.tabBaseline + (shell.page === index ? Theme.activeTabOverlap : 0)
                    Behavior on height { NumberAnimation { duration: Theme.motion(130); easing.type: Easing.OutCubic } }
                    function outline(offset, spread) {
                        // The last key's shadow stops at its joint with the
                        // sidewall, keeping the chassis edge highlight intact.
                        const l = -spread, w = width + (index === 4 ? 0 : spread), h = height + offset;
                        // Home keeps the same softened left diagonal as its peers.
                        const left = " H " + (l + 14) + " Q " + (l + 10) + " " + h + " " + (l + 7) + " " + (h - 3)
                            + " L " + (l + 3) + " " + (h - 7) + " Q " + l + " " + (h - 10) + " " + l + " " + (h - 14);
                        return "M " + l + " " + offset + " H " + w + " V " + (h - 17)
                            + " Q " + w + " " + (h - 13) + " " + (w - 3) + " " + (h - 10)
                            + " L " + (w - 10) + " " + (h - 3)
                            + " Q " + (w - 13) + " " + h + " " + (w - 17) + " " + h + left + " Z";
                    }
                    Shape {
                        anchors.fill: parent
                        // Local penumbra and contact shadow. Nothing spans the
                        // whole bank underneath the unchanged tab faces.
                        ShapePath {
                            strokeColor: "transparent"
                            fillColor: "#08101e1d"
                            PathSvg { path: tab.outline(10, 3) }
                        }
                        ShapePath {
                            strokeColor: "transparent"; fillColor: "#10101e1d"
                            PathSvg { path: tab.outline(8, 2.5) }
                        }
                        ShapePath {
                            strokeColor: "transparent"
                            fillColor: shell.page === index ? "#28101e1d" : "#1c101e1d"
                            PathSvg { path: tab.outline(6, 2) }
                        }
                        ShapePath {
                            strokeColor: "transparent"; fillColor: "#50101e1d"
                            PathSvg { path: tab.outline(4, 1) }
                        }
                        ShapePath {
                            strokeColor: "transparent"; fillColor: "#60101e1d"
                            PathSvg { path: tab.outline(2, 0.5) }
                        }
                        ShapePath {
                            strokeColor: Qt.darker(Theme.tabColors[index], 1.65); strokeWidth: 1
                            fillColor: shell.page === index ? Theme.tabColors[index] : Qt.darker(Theme.tabColors[index], 1.18)
                            PathSvg { path: tab.outline(0, 0) }
                        }
                    }
                    Rectangle {
                        visible: index === 0
                        x: -2; y: 0; width: 2; height: parent.height - Theme.tabBevel
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#90101e1d" }
                            GradientStop { position: 0.7; color: "#70101e1d" }
                            GradientStop { position: 1; color: "#00101e1d" }
                        }
                    }
                    Rectangle {
                        // Deep seams between adjacent keys, and a small contact
                        // seam inside the final key, clear of the chassis slope.
                        x: parent.width - (index === 4 ? 1 : 0); y: 0
                        width: index === 4 ? 1 : Theme.tabSpacing
                        height: index === 4 ? parent.height - Theme.tabBevel : Theme.tabBaseline
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#a0101e1d" }
                            GradientStop { position: 0.6; color: "#90101e1d" }
                            GradientStop { position: 1; color: "#00101e1d" }
                        }
                    }
                    Rectangle { x: 2; y: 1; width: parent.width - 4; height: 2; color: "#90ffffff" }
                    Rectangle { x: 2; y: 3; width: 1; height: parent.height - 17; color: "#55ffffff" }
                    Text {
                        anchors.centerIn: parent; anchors.verticalCenterOffset: Theme.topRimHeight / 2
                        text: modelData; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: shell.page === index ? 21 : 19; font.weight: Font.DemiBold
                    }
                    Rectangle { x: 18; y: parent.height - 8; width: parent.width - 36; height: 3; radius: 1.5; color: "#7a4a24"; visible: shell.page === index }
                    MouseArea { anchors.fill: parent; onClicked: shell.goToPage(index) }
                }
            }
        }
        ChassisTopRim { z: 1.5 }
        Item {
            id: screen
            objectName: "primary-screen"
            x: Theme.screenBounds.x; y: Theme.screenBounds.y
            width: Theme.screenBounds.width; height: Theme.screenBounds.height
            Item {
                objectName: "page-viewport"
                enabled: !shell.drawerOpen
                anchors.fill: parent; anchors.margins: Theme.panelInset
                anchors.topMargin: Theme.contentTopInset; clip: true
                anchors.bottomMargin: shell.page === 0 || shell.page === 1 ? Theme.panelInset : Theme.panelInset + 30
                HomePage { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 0 && !shell.multiverseHome }
                MultiverseHome { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 0 && shell.multiverseHome }
                HallOfFamePage { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 4 }
                TrainerPage { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 3 }
                WorldsPage { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 1 && !shell.multiverseFace }
                MultiversePage { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 1 && shell.multiverseFace }
                PokedexPage { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 2 && !shell.centerFace }
            }
        }
        ContinueDrawer {
            id: drawer
            objectName: "continue-drawer"
            x: -Theme.screenBevel; anchors.bottom: footer.top; shell: shellController
            z: 1
            visible: shell.chooseAdventureAvailable || shell.drawerOpen
        }
        Item {
            id: footer
            x: 0; y: Theme.footerTop; width: parent.width; height: Theme.footerHeight
            BatteryGauge { x: 18; y: 5; status: powerStatus }
            Text {
                objectName: "controller-warning"
                x: 132; y: 10; visible: !controllerInput.connected; text: "!  Connect a controller"
                color: "#ffe29c"; font.pixelSize: 11
                Accessible.role: Accessible.AlertMessage; Accessible.name: text
            }
            function hint(button, label) { return {button:button, label:label} }
            readonly property var actions: {
                const h = hint
                if (shell.notice.length) return [h("A", shell.modeConfirmation ? "Continue" : "OK"), h("B","Cancel")]
                if (shell.menuOpen) return shell.powerMenu ? [h("A","Select"),h("B","Back")] : [h("X","Quick controls"),h("←→","Adjust"),h("A","Select"),h("B","Close")]
                if (shell.keyboard.open) return [h("X","Case"),h("Y","Symbols"),h("A","Type"),h("B","Cancel")]
                if (shell.libraryTools.open) return [h("A","Select"),h("B","Back")]
                if (shell.drawerOpen) return [h("A","Choose"),h("B","Close")]
                if (shell.trainer.picker.open) return [h("X","Search"),h("Y","Clear"),h("←→","Jump 8"),h("A","Choose"),h("B","Cancel")]
                if (shell.hall.account.open) return [h("A","Select"),h("B","Back")]
                if (shell.service === "trainer-setup") {
                    let result = [h("A",shell.trainerSetup.keypad ? "Enter" : "Select"),h("B","Back")]
                    if (shell.trainerSetup.canRemove) result.unshift(h("X","Remove Trainer"))
                    return result
                }
                if (shell.service === "settings") return [h("←→","Adjust"),h("A",shell.settings.controlsFocused ? "Select" : "Open"),h("B",shell.settings.controlsFocused ? "Categories" : "Back")]
                if (shell.service === "device") return [h("←→","Adjust"),h("Y","Refresh"),h("A","Select"),h("B","Back")]
                if (shell.serviceOpen) return [h("A","Select"),h("B","Back")]
                if (shell.page === 1) {
                    const list = shell.multiverseFace ? shell.multiverse.route === "games" : shell.worlds.route === "adventures"
                    let result = list ? [h("X","Search"),h("Y","Filter"),h("A",shell.multiverseFace ? "Home" : "Open"),h("B",shell.multiverseFace ? "Systems" : "Regions")] : [h("A","Open"),h("B","Back")]
                    if(shell.canHoldConfirm) result.push(h("Hold A","Options"))
                    if(shell.canEditWorld) result.push(h("Select","Edit World"))
                    return result
                }
                if (shell.page === 2 && !shell.centerFace) {
                    const dex = shell.pokedex
                    if (dex.journal.open) return [h("Y","Save"),h("A","Edit"),h("B","Discard")]
                    if (dex.zone === "art") return [h("←→","Browse"),h("A","Use image"),h("B","Cancel")]
                    if (dex.zone === "picker") return [h("A","Apply"),h("B","Cancel")]
                    let result = [h("X","Search")]
                    if (dex.detail.id) result.push(h("Select","Journal"))
                    if (dex.zone === "list") result.push(h("←→","Jump 8"))
                    result.push(h("A",dex.zone === "list" ? (dex.detail.favorite ? "Unfavorite" : "Favorite") : "Select"),h("B",dex.zone === "rail" ? "Entries" : "Filters"))
                    return result
                }
                if (shell.page === 2 && shell.centerFace) {
                    const party = shell.party
                    if (party.section === "saves") return shell.center.confirming ? [h("A","Restore"),h("B","Cancel")] : [h("X",shell.center.route === "adventures" ? "Search" : "Refresh"),h("Select","Backup"),h("A","Open"),h("B","Back")]
                    if (party.section === "activities") return party.activities.route === "playroom" && party.activities.sample ? [h("←→","Partner"),h("X","Greet"),h("A","Call"),h("B","Back")] : [h("A","Select"),h("B","Back")]
                    if (party.detailOpen) return [h("A","Select"),h("B","Close")]
                    if (party.boxFocused) return [h("←→","Box"),h("↓","Slots"),h("X","Party"),h("B","Back")]
                    const actions = [h("X",party.section === "party" ? "Storage" : "Party"),h("Select","Backups"),h("A",party.sample && !party.activitiesFocused ? "Actions" : "Open"),h("B","Back")]
                    if (party.section === "storage" && party.sample && !party.activitiesFocused && party.focusIndex < 6) actions.unshift(h("↑","Boxes"))
                    return actions
                }
                if (shell.page === 4) {
                    const hall = shell.hall
                    if (hall.editor.open) return hall.editor.route === "form" ? [h("Y","Save"),h("A","Edit"),h("B","Discard")] : [h("X",hall.editor.route === "team" ? "Level" : "Search"),h("A",hall.editor.route === "team" ? "Name" : "Choose"),h("B","Back")]
                    let result = []
                    if (hall.archive && hall.editable) result.push(h("Select","New memory"))
                    if (!hall.archive) result.push(h("Select","Refresh"))
                    if (!hall.archive && hall.account.available) result.push(h("X","Account"))
                    else if (hall.route === "archive-journey") result.push(h("X","Champions"))
                    else if (hall.archive && hall.editable && hall.rows.length && !hall.overview) result.push(h("X","Edit"))
                    result.push(h("A","Open"),h("B","Back")); return result
                }
                if (shell.page === 0) return [h("X",shell.multiverseHome ? "Pokémon" : "Multiverse"),h("A",shell.multiverseHome ? (shell.multiverse.selected.id ? (shell.multiverse.selected.playable ? "Play" : "Set up") : "Explore") : shell.home.actionHint),h("B","Back")]
                return [h("A","Select"),h("B",shell.trainer.editing ? "Cancel" : "Back")]
            }
            Row {
                objectName: "shell-button-hints"
                anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
                spacing: 9
                transformOrigin: Item.Right
                scale: Math.min(1, (footer.width - 136) / Math.max(1, implicitWidth))
                Hint { visible: shell.pairedNavigationAvailable; button: "L2 R2"; label: shell.page === 1 ? (shell.multiverseFace ? "Worlds" : "Multiverse") : shell.page === 4 ? (shell.hall.archive ? "Achievements" : "Hall of Fame") : (shell.centerFace ? "Pokédex" : "Center"); tint: Theme.green }
                Hint { button: "L1 R1"; label: "Sections"; tint: Theme.blue }
                Repeater { model: footer.actions
                    delegate: Hint {
                        required property var modelData
                        button: modelData.button; label: modelData.label
                        tint: button === "B" ? Theme.pink : button === "X" || button === "←→" ? Theme.blue : button === "Y" ? Theme.yellow : Theme.green
                    }
                }
                Hint { button: "Start"; label: "System"; tint: Theme.yellow }
            }
        }
        LibraryPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "library" }
        TrainerSettingsPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "trainer-settings" }
        TrainerSetupPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "trainer-setup" }
        SettingsPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "settings" }
        DevicePanel { anchors.fill: screen; shell: shellController; visible: shell.service === "device" }
        DiagnosticsPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "diagnostics" }
        PartyPanel { anchors.fill: screen; shell: shellController; visible: !shell.serviceOpen && shell.centerFace && shell.party.section !== "saves" && shell.party.section !== "activities" }
        CenterActivitiesPanel { anchors.fill: screen; shell: shellController; visible: !shell.serviceOpen && shell.centerFace && shell.party.section === "activities" }
        SaveCenterPanel { anchors.fill: screen; shell: shellController; visible: !shell.serviceOpen && shell.centerFace && shell.party.section === "saves" }
        LibraryToolsPanel { anchors.fill: screen; shell: shellController; z: 1; visible: shell.libraryTools.open }
        KeyboardPanel {
            anchors { left: screen.left; right: screen.right; top: screen.top; bottom: footer.top }
            z: 2; shell: shellController
        }
        SystemPanel {
            anchors { left: screen.left; right: parent.right; top: screen.top; bottom: footer.top }
            z: 3; shell: shellController
        }
        }
        Item {
            anchors.fill: parent; visible: sessionState.entryGate && !sessionState.access.active
            ChassisFrame { anchors.fill: parent }
            Text { x: 38; y: 23; text: "TRAINER OS"; color: "#f6e4b3"; font.pixelSize: 26; font.bold: true }
            TrainerSetupPanel { entry: true; x: 0; y: 43; width: parent.width; height: parent.height-63; shell: shellController }
            KeyboardPanel { x: Theme.screenBounds.x; y: Theme.screenBounds.y; width: Theme.screenBounds.width; height: Theme.screenBounds.height; shell: shellController }
            Row { anchors.right: parent.right; anchors.rightMargin: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 9; spacing: 18
                Hint { visible: shell.keyboard.open; button: "X"; label: "Case" }
                Hint { visible: shell.keyboard.open; button: "Y"; label: "Symbols" }
                Hint { button: "A"; label: shell.keyboard.open ? "Type" : "Select" }
                Hint { button: "B"; label: "Back"; tint: Theme.pink }
            }
        }
        StoragePanel { anchors.fill: parent; visible: sessionState.blocked && !sessionState.entryGate && !sessionState.access.active; stateController: sessionState }
        TrainerAccessPanel { anchors.fill: parent; access: sessionState.access; visible: sessionState.access.active }
        LaunchPanel { anchors.fill: parent; visible: adventureLaunch.preparing; launch: adventureLaunch }
    }
    readonly property var shell: shellController
}
