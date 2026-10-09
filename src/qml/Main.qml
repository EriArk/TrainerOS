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
    Binding { target: Theme; property: "television"; value: shell.settings.displayProfile === "tv" }
    Binding { target: shell.social; property: "surfaceAvailable"; value: window.active && !sessionState.blocked && (!adventureLaunch.active || adventureLaunch.minimized) && shell.unobstructed }
    Binding { target: shell.social; property: "conversationVisible"; value: shell.page === 4 && !shell.serviceOpen }
    Binding { target: shell.social; property: "gameActive"; value: adventureLaunch.active }
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
        enabled: !adventureLaunch.active || adventureLaunch.minimized
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
            Text {
                x: 12; y: 46; width: parent.width - 24
                visible: !!shell.liveGame.id; text: "Still running · Home to return"
                horizontalAlignment: Text.AlignHCenter; color: "#ffe3a4"; font.pixelSize: 10
            }
        }
        Row {
            objectName: "primary-tabs"
            x: Theme.brandWidth; y: 0; spacing: Theme.tabSpacing
            z: 1 // Individual chassis-mounted keys; modal surfaces stay above them.
            Repeater {
                model: shell.primaryNames
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
                    Rectangle {
                        visible: index === 4 && (shell.social.account.unreadCount || 0) > 0
                        anchors.right: parent.right; anchors.rightMargin: 6; y: Theme.topRimHeight + 3
                        width: 23; height: 17; radius: 7; color: Theme.yellow; border.color: "#8e642e"
                        Text { anchors.centerIn: parent; text: Math.min(99, shell.social.account.unreadCount || 0); color: Theme.ink; font.bold: true; font.pixelSize: 11 }
                    }
                    MouseArea { anchors.fill: parent; onClicked: shell.goToPage(index) }
                }
            }
        }
        ChassisTopRim { z: 1.5 }
        PassiveNotice {
            objectName: "passive-notification"
            notice: shell.passiveNotice
            visible: !!notice.title && window.active && !sessionState.blocked && !shell.homeMenuOpen && (!adventureLaunch.active || adventureLaunch.minimized)
            z: 30; x: parent.width - width - 26; y: Theme.brandHeight + 21
        }
        Row {
            objectName: "navigation-chassis"; x: 28; y: Theme.brandHeight + Theme.screenBevel + 5; spacing: 12; z: 2
            Hint {button:"L1 R1";label:"Sections";tint:Theme.blue;labelColor:Theme.ink}
            Hint {button:"L2 R2";label:"";tint:Theme.green;visible:shell.faceNames.length>1;opacity:shell.pairedNavigationAvailable?1:.45}
            Text {
                visible: shell.faceNames.length > 6
                text: "‹   " + (shell.faceNames[shell.faceIndex] || "Collections") + "   ›   " + (shell.faceIndex + 1) + " / " + shell.faceNames.length
                color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 15; font.bold: true
            }
            Repeater {model:shell.faceNames.length > 6 ? [] : shell.faceNames
                Text {
                    required property int index;required property string modelData
                    text:modelData;color:shell.faceIndex===index?Theme.ink:Theme.muted
                    font.family:Theme.displayFamily;font.pixelSize:15;font.bold:shell.faceIndex===index
                    Rectangle {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.bottomMargin:-3;height:2;color:Theme.focusGlow;visible:shell.faceIndex===index}
                }
            }
        }
        Item {
            id: screen
            objectName: "primary-screen"
            x: Theme.screenBounds.x; y: Theme.screenBounds.y
            width: Theme.screenBounds.width; height: Theme.screenBounds.height
            Item {
                objectName: "page-viewport"
                enabled: !shell.drawerOpen && !shell.serviceOpen
                anchors.fill: parent; anchors.margins: Theme.panelInset
                anchors.topMargin: Theme.contentTopInset; clip: true
                anchors.bottomMargin: Theme.panelInset
                ExperienceHost { id: experienceHost; anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && (shell.page === 0 || shell.page === 2 || shell.page === 3) }
                SocialPage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 4 }
                SeriesPage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 1 && shell.collectionsRoot }
                WorldsPage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 1 && !shell.collectionsRoot && !shell.multiverseFace }
                MultiversePage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 1 && !shell.collectionsRoot && shell.multiverseFace }
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
            visible: !adventureLaunch.preparing
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
                if (shell.social.online.open) return shell.social.online.incoming ? [h("A","Accept"),h("B","Decline")] : [h("B","Cancel")]
                if (shell.moduleInvitation.open) return shell.moduleInvitation.incoming ? [h("A","Accept"),h("B","Decline")] : [h("B","Cancel")]
                if (shell.homeMenuOpen) {
                    if (!shell.notificationsOpen) return [h("↑↓","Choose"),h("A","Select"),h("B","Back")]
                    const selectedNotice = shell.notifications[shell.notificationFocus] || {}
                    return [h("↑↓","Choose"),h("A",selectedNotice.answerable ? "Answer" : "Open"),h("X",selectedNotice.ringing ? "Decline" : "Dismiss"),h("B","Back")]
                }
                if (shell.notice.length) return [h("A", shell.modeConfirmation ? "Continue" : "OK"), h("B","Cancel")]
                if (shell.downloads.open) return [h("↑↓","Choose"),h("←→","Actions"),h("A","Select"),h("B","Close")]
                if (shell.menuOpen) return shell.powerMenu ? [h("A","Select"),h("B","Back")] : [h("X","Quick controls"),h("←→",shell.focusIndex>=9 ? "Choose" : "Adjust"),h("A",shell.focusIndex>=9 && shell.focusIndex<=11 ? "Toggle" : "Select"),h("B","Close")]
                if (shell.keyboard.open) return [h("X","Case"),h("Y",shell.keyboard.nextLayout),h("A","Type"),h("Select",shell.keyboard.submitLabel),h("B",shell.keyboard.submitLabel === "Send" ? "Keep draft" : "Cancel")]
                if (shell.scraper.open) return shell.scraper.selectingSystems ? [h("↑↓","Systems"),h("←→","Actions"),h("A","Select"),h("B","Back")] : [h("A","Select"),h("B",shell.scraper.busy&&!shell.scraper.choosingMatch?"Stop":"Back")]
                if (shell.libraryTools.open) {
                    if(shell.libraryTools.route==="reviews")return shell.libraryTools.reviewReportAvailable
                        ? [h("A","Reveal"),h("← →","Read"),h("Select","Report"),h("B","Back")]
                        : [h("A","Select"),h("← →","Read"),h("B","Back")]
                    return [h("A","Select"),h("B","Back")]
                }
                if (shell.drawerOpen) return [h("A","Choose"),h("B","Close")]
                if (shell.hall.account.open) return [h("A","Select"),h("B","Back")]
                if (shell.service === "trainer-setup") {
                    let result = [h("A",shell.trainerSetup.keypad ? "Enter" : "Select"),h("B","Back")]
                    if (shell.trainerSetup.canRemove) result.unshift(h("X","Remove Trainer"))
                    return result
                }
                if (shell.service === "settings" && shell.settings.category===10 && shell.settings.controlsFocused) {
                    if(shell.network.prompt.length) return shell.network.confirmLabel==="Wait" ? [h("B","Cancel")] : [h("A",shell.network.confirmLabel),h("B","Cancel")]
                    if(shell.network.busy) return [h("B","Cancel")]
                    let controls = [h("←→","Wi-Fi / Bluetooth"),h("X","Radio"),h("Y","Search")]
                    if(shell.network.canForget) controls.push(h("Select","Forget"))
                    if(shell.network.rows.length) controls.push(h("A",shell.network.actionLabel))
                    controls.push(h("B","Categories")); return controls
                }
                if (shell.service === "settings" && shell.settings.category===11 && shell.settings.controlsFocused) return shell.settings.clock.busy ? [] : [h("←→",shell.settings.clock.mode==="zones" ? "Region" : "Adjust"),h("A",shell.settings.clock.mode==="manual" ? (shell.settings.clock.focusIndex===5 ? "Save" : "Next") : "Select"),h("B",shell.settings.clock.mode==="main" ? "Categories" : "Cancel")]
                if (shell.service === "settings" && shell.settings.storage.open) return shell.settings.storage.busy ? [] : [h("A","Use storage"),h("B","Library")]
                if (shell.service === "settings" && shell.settings.category===13 && shell.settings.controlsFocused) {
                    const flow = shell.settings.communication
                    return [h("←→","Adjust"),h("A","Select"),h("B",flow.choosingAvatar ? "Cancel" : "Categories")]
                }
                if (shell.service === "settings") return [h("←→","Adjust"),h("A",shell.settings.controlsFocused ? "Select" : "Open"),h("B",shell.settings.controlsFocused ? "Categories" : "Back")]
                if (shell.service === "device") return [h("←→","Adjust"),h("Y","Refresh"),h("A","Select"),h("B","Back")]
                if (shell.serviceOpen) return [h("A","Select"),h("B","Back")]
                if(shell.collectionManager.open) return [h("A","Choose"),h("B","Back")]
                if (shell.page === 1 && shell.collectionsRoot) return [h("A","Open collection"),h("Select","New / edit"),h("B","Home")]
                if (shell.page === 1) {
                    const list = shell.multiverseFace ? shell.multiverse.route === "games" : shell.worlds.route === "adventures"
                    let result = list ? [h("X","Search"),h("Y","Filter"),h("A","Play"),h("B",shell.multiverseFace ? (shell.multiverse.collection === "multiverse" ? "Systems" : "Collections") : "Regions")] : [h("A","Open"),h("B","Back")]
                    if(shell.canHoldConfirm) result.push(h("Hold A","Options"))
                    result.push(h("Select",list ? "Game actions" : "Collection actions"))
                    return result
                }
                const experienceHints = experienceHost.hints(h)
                if (experienceHints) return experienceHints
                if (shell.page === 0) return [h("A",shell.homeGame.id ? (shell.liveGame.id === shell.homeGame.id ? "Return" : shell.home.actionHint) : "Collections"),h("Select","Game actions"),h("B","Back")]
                if (shell.page === 4) return shell.social.hints
                return [h("A",shell.trainer.editing ? "Select" : "Edit Trainer"),h("B",shell.trainer.editing ? "Cancel" : "Back")]
            }
            Row {
                objectName: "shell-button-hints"
                anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
                spacing: 9
                transformOrigin: Item.Right


                Repeater { model: footer.actions
                    delegate: Hint {
                        required property var modelData
                        button: modelData.button; label: modelData.label
                        interactive: ["A","B","X","Y","Select"].includes(button)
                        onClicked: shell.pressButton(button)
                        tint: button === "B" ? Theme.pink : button === "X" || button === "←→" ? Theme.blue : button === "Y" ? Theme.yellow : Theme.green
                    }
                }
                Hint { button: "Home"; label: "Options"; interactive: true; onClicked: shell.pressButton("Home"); visible: !shell.moduleControlsBlocked && !shell.social.online.open }
                Hint { button: "Start"; label: "System"; tint: Theme.yellow; interactive: true; onClicked: shell.pressButton("Start"); visible: !shell.moduleControlsBlocked && !shell.social.online.open }
            }
        }
        LibraryPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "library" }
        TrainerSettingsPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "trainer-settings" }
        PlayerProfilePanel { anchors.fill: screen; anchors.margins: Theme.panelInset; anchors.topMargin: Theme.contentTopInset; shell: shellController; visible: shell.service === "profile" }
        TrainerSetupPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "trainer-setup" }
        DevicePanel { anchors.fill: screen; shell: shellController; visible: shell.service === "device" }
        DiagnosticsPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "diagnostics" }
        ExperienceHost { anchors.fill: screen; shell: shellController; overlay: true; visible: shell.page === 2 }
        ScrapePanel { anchors.fill: screen; shell: shellController; z: 3.1; visible: shell.scraper.open && !shell.keyboard.open }
        DownloadsPanel { anchors.fill: screen; flow: shell.downloads; z: 3.2; visible: shell.downloads.open }
        CollectionsPanel { anchors.fill: screen; shell: shellController; z: 3; visible: shell.collectionManager.open }
        LibraryToolsPanel { anchors.fill: screen; shell: shellController; z: 3; visible: shell.libraryTools.open }
        Item {
            id: settingsOverlay
            objectName: "settings-overlay"
            width: parent.width; height: Theme.footerTop; z: 2.2
            visible: shell.service === "settings"
            Rectangle { anchors.fill: parent; color: "#99102020" }
            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted = true }
            Rectangle {
                x: 37; y: 61; width: parent.width-64; height: parent.height-66
                radius: 16; color: "#60101a19"
            }
            Panel {
                id: settingsSurface
                objectName: "settings-surface"
                x: 32; y: 52; width: parent.width-64; height: parent.height-66
                SettingsPanel { anchors.fill: parent; shell: shellController }
            }
        }
        KeyboardPanel {
            anchors { left: screen.left; right: screen.right; top: screen.top; bottom: footer.top }
            z: 3; shell: shellController
        }
        SystemPanel {
            anchors { left: screen.left; right: parent.right; top: screen.top; bottom: footer.top }
            z: 4; shell: shellController
        }
        Item {
            objectName: "shell-home-menu"
            width: parent.width; height: Theme.footerTop; z: 5
            visible: shell.homeMenuOpen
            property var originFocus: null
            onVisibleChanged: {
                if (visible) originFocus = window.activeFocusItem
                else if (originFocus && originFocus.visible && originFocus.enabled) originFocus.forceActiveFocus()
            }
            Rectangle { anchors.fill: parent; color: "#88102324" }
            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted = true }
            ShellOptions {
                anchors.centerIn: parent
                width: parent.width * 0.92; height: parent.height * 0.90
                shell: shellController
            }
        }
        Item {
            id: nearbyInvitation
            objectName: "nearby-invitation"
            property var link: shell.moduleInvitation.controller || null
            readonly property bool online: !!shell.social.online.open
            readonly property bool runtime: false // Runtime requests open through their stable inbox IDs.
            readonly property bool incoming: runtime || (online ? !!shell.social.online.incoming : !!shell.moduleInvitation.incoming)
            function answer(accept) { if(runtime) runtimeMultiplayer.answer(accept); else if (online) shell.social.answerOnline(accept); else if(link) link.answerInvitation(accept) }
            width: parent.width; height: Theme.footerTop; z: 6
            visible: (runtime || online || !!shell.moduleInvitation.open) && !sessionState.blocked && !adventureLaunch.active
            Rectangle { anchors.fill: parent; color: "#77112323" }
            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted=true }
            Panel {
                anchors.centerIn: parent; width: 470; height: 225; surface: "#f6f0d7"
                Text { x: 27; y: 23; text: nearbyInvitation.incoming ? "INCOMING ACTIVITY" : "TOGETHER"; font.family: Theme.brandFamily; font.pixelSize: 16; color: "#4c8175" }
                Text { x: 27; y: 56; width: parent.width-54; height: 85; text: nearbyInvitation.runtime ? runtimeMultiplayer.invitation : nearbyInvitation.online ? shell.social.online.status : nearbyInvitation.link ? nearbyInvitation.link.invitationText : ""; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: 26; color: Theme.ink; wrapMode: Text.WordWrap }
                CapButton { x: 27; y: 154; width: 200; height: 45; label: "Accept"; centered: true; tint: Theme.green; selected: true; deferredFocus: true; visible: nearbyInvitation.incoming; onActivated: nearbyInvitation.answer(true) }
                CapButton { x: nearbyInvitation.incoming ? 242 : 135; y: 154; width: 200; height: 45; label: nearbyInvitation.incoming ? "Decline" : "Cancel"; centered: true; tint: Theme.blue; selected: !nearbyInvitation.incoming; deferredFocus: true; onActivated: nearbyInvitation.answer(false) }
            }
        }
        }
        Item {
            anchors.fill: parent; visible: (sessionState.entryGate || sessionState.firstRunPage) && !sessionState.access.active
            ChassisFrame { anchors.fill: parent }
            ChassisTopRim { }
            Text { x: 12; y: 0; width: Theme.brandWidth-24; height: Theme.brandHeight; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; text: "TRAINER OS"; color: "#edf5e9"; font.family: Theme.brandFamily; font.pixelSize: 25; font.bold: true }
            FirstRunPanel { anchors.fill: parent; shell: shellController; flow: sessionState.firstRun; visible: sessionState.firstRunPage }
            TrainerSetupPanel { entry: true; x: 0; y: 43; width: parent.width; height: parent.height-63; shell: shellController; visible: !sessionState.firstRunPage || sessionState.firstRun.stage === "trainer" }
            Text {
                x: 48; y: parent.height-62; width: parent.width-96; height: 24
                visible: sessionState.firstRunPage && sessionState.firstRun.stage === "trainer"
                text: sessionState.firstRun.error; textFormat: Text.PlainText; elide: Text.ElideRight
                color: "#853b24"; font.pixelSize: 14
            }
            KeyboardPanel { x: Theme.screenBounds.x; y: Theme.screenBounds.y; width: Theme.screenBounds.width; height: Theme.screenBounds.height; shell: shellController }
            Row { anchors.right: parent.right; anchors.rightMargin: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 9; spacing: 18
                Hint { visible: shell.keyboard.open; button: "X"; label: "Case" }
                Hint { visible: shell.keyboard.open; button: "Y"; label: "Symbols" }
                Hint { visible: sessionState.firstRun.connections && !shell.keyboard.open; button: "X"; label: "Radio" }
                Hint { visible: sessionState.firstRun.connections && !shell.keyboard.open; button: "Y"; label: "Search" }
                Hint { visible: sessionState.firstRun.connections && !shell.keyboard.open; button: "←→"; label: "Connections" }
                Hint { visible: sessionState.firstRun.stage==="clock" && !sessionState.firstRun.busy; button: "←→"; label: sessionState.firstRun.clock.mode==="zones" ? "Region" : "Adjust" }
                Hint { visible: !sessionState.firstRun.busy; button: "A"; label: shell.keyboard.open ? "Type" : sessionState.firstRunPage && sessionState.firstRun.stage==="controls" ? "Check" : sessionState.firstRunPage && sessionState.firstRun.stage==="storage" ? "Use storage" : "Select" }
                Hint { visible: !sessionState.firstRun.busy && (!sessionState.firstRunPage || shell.keyboard.open || sessionState.firstRun.connections || ["controls","network","clock","storage","trainer"].includes(sessionState.firstRun.stage)); button: "B"; label: sessionState.firstRunPage && sessionState.firstRun.stage==="controls" ? "Check" : "Back"; tint: Theme.pink }
            }
        }
        StoragePanel { anchors.fill: parent; visible: sessionState.blocked && !sessionState.entryGate && !sessionState.firstRunPage && !sessionState.access.active; stateController: sessionState }
        TrainerAccessPanel { anchors.fill: parent; access: sessionState.access; visible: sessionState.access.active }
        LaunchPanel { anchors.fill: parent; visible: adventureLaunch.preparing; launch: adventureLaunch }
    }
    readonly property var shell: shellController
}
