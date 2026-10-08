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
    Binding { target: shell.social; property: "surfaceAvailable"; value: window.active && !sessionState.blocked && !adventureLaunch.active && shell.unobstructed }
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
        Rectangle {
            objectName: "social-notification"
            visible: shell.social.surfaceAvailable && shell.social.toastTitle.length > 0
            z: 30; x: parent.width - width - 26; y: Theme.brandHeight + 21
            width: 310; height: 75; radius: 12; color: "#fff0bb"; border.color: "#8e713e"; border.width: 2
            Rectangle { x: 10; y: 13; width: 43; height: 43; radius: 12; color: Theme.blue; border.color: "#658b91"
                Text { anchors.centerIn: parent; text: ":)"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 23 }
            }
            Column { x: 64; y: 12; width: parent.width - 77; spacing: 4
                Text { width: parent.width; text: shell.social.toastTitle; textFormat: Text.PlainText; elide: Text.ElideRight; font.family: Theme.displayFamily; font.pixelSize: 18; color: Theme.ink }
                Text { width: parent.width; text: shell.social.toastText; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 14; color: Theme.ink }
            }
            MouseArea { anchors.fill: parent; onClicked: shell.openSocialNotification() }
        }
        Row {
            objectName: "navigation-chassis"; x: 28; y: Theme.brandHeight + Theme.screenBevel + 5; spacing: 12; z: 2
            Hint {button:"L1 R1";label:"Sections";tint:Theme.blue;labelColor:Theme.ink}
            Hint {button:"L2 R2";label:"";tint:Theme.green;visible:shell.faceNames.length>1;opacity:shell.pairedNavigationAvailable?1:.45}
            Text {
                visible: shell.faceNames.length > 6
                text: "‹   " + (shell.faceNames[shell.faceIndex] || "Worlds") + "   ›   " + (shell.faceIndex + 1) + " / " + shell.faceNames.length
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
                HomePage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 0 && !shell.multiverseHome }
                MultiverseHome { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 0 && shell.multiverseHome }
                HallOfFamePage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.trainerHistoryFace }
                TrainerPage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 3 && shell.trainerFace === "profile" }
                SocialPage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 4 }
                SeriesPage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 1 && shell.collectionsRoot }
                WorldsPage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 1 && !shell.collectionsRoot && !shell.multiverseFace }
                MultiversePage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 1 && !shell.collectionsRoot && shell.multiverseFace }
                PokedexPage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings") && shell.page === 2 && !shell.centerFace }
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
                if (shell.party.activities.link.invitationOpen) return shell.party.activities.link.invitationIncoming ? [h("A","Accept"),h("B","Decline")] : [h("B","Cancel")]
                if (shell.homeMenuOpen) {
                    if (!shell.notificationsOpen) return [h("↑↓","Choose"),h("A","Select"),h("B","Back")]
                    const selectedNotice = shell.social.notifications[shell.notificationFocus] || {}
                    return [h("↑↓","Choose"),h("A",selectedNotice.answerable ? "Answer" : "Open"),h("X",selectedNotice.ringing ? "Decline" : "Dismiss"),h("B","Back")]
                }
                if (shell.notice.length) return [h("A", shell.modeConfirmation ? "Continue" : "OK"), h("B","Cancel")]
                if (shell.downloads.open) return [h("↑↓","Choose"),h("←→","Actions"),h("A","Select"),h("B","Close")]
                if (shell.menuOpen) return shell.powerMenu ? [h("A","Select"),h("B","Back")] : [h("X","Quick controls"),h("←→",shell.focusIndex>=9 ? "Choose" : "Adjust"),h("A",shell.focusIndex>=9 && shell.focusIndex<=11 ? "Toggle" : "Select"),h("B","Close")]
                if (shell.keyboard.open) return [h("X","Case"),h("Y",shell.keyboard.nextLayout),h("A","Type"),h("Select",shell.keyboard.submitLabel),h("B",shell.keyboard.submitLabel === "Send" ? "Keep draft" : "Cancel")]
                if (shell.scraper.open) return [h("A","Select"),h("B",shell.scraper.busy?"Stop":"Back")]
                if (shell.libraryTools.open) {
                    if(shell.libraryTools.route==="reviews")return shell.libraryTools.reviewReportAvailable
                        ? [h("A","Reveal"),h("← →","Read"),h("Select","Report"),h("B","Back")]
                        : [h("A","Select"),h("← →","Read"),h("B","Back")]
                    return [h("A","Select"),h("B","Back")]
                }
                if (shell.drawerOpen) return [h("A","Choose"),h("B","Close")]
                if (shell.trainer.picker.open) return [h("X","Search"),h("Y","Clear"),h("←→","Jump 8"),h("A","Choose"),h("B","Cancel")]
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
                if (shell.page === 1 && shell.collectionsRoot) return [h("A","Open series"),h("B","Home")]
                if (shell.page === 1) {
                    const list = shell.multiverseFace ? shell.multiverse.route === "games" : shell.worlds.route === "adventures"
                    let result = list ? [h("X","Search"),h("Y","Filter"),h("A","Play"),h("B",shell.multiverseFace ? (shell.multiverse.collection === "multiverse" ? "Systems" : "Collections") : "Regions")] : [h("A","Open"),h("B","Back")]
                    if(shell.canHoldConfirm) result.push(h("Hold A","Options"))
                    if(shell.canEditWorld) result.push(h("Select","Edit World"))
                    return result
                }
                if (shell.page === 2 && !shell.centerFace) {
                    const dex = shell.pokedex
                    if (dex.zone === "art") return [h("←→","Browse"),h("A","Use image"),h("B","Cancel")]
                    if (dex.zone === "picker") return [h("A","Apply"),h("B","Cancel")]
                    let result = [h("X","Search"),h("Select","Filters")]
                    if (dex.zone === "list") result.push(h("←→","Jump 8"))
                    result.push(h("A",dex.zone === "list" ? (dex.detail.favorite ? "Unfavorite" : "Favorite") : "Select"),h(dex.zone === "rail" ? "B" : "↑",dex.zone === "rail" ? "Entries" : "Filters"))
                    return result
                }
                if (shell.page === 2 && shell.centerFace) {
                    const party = shell.party
                    if (shell.center.shopsOpen) {
                        const route = shell.center.shopRoute
                        if (shell.center.busy) return []
                        if (route === "merchants") return [h("←→","Categories"),h("X","Search"),h("Y","Place"),h("Select","Basket · "+shell.center.basketCount),h("A","Open"),h("B","Back")]
                        if (route === "basket") return shell.center.basketCount ? [h("←→","Quantity"),h("X","Remove"),h("A","Buy basket"),h("B","Back")] : [h("A","Back"),h("B","Back")]
                        if (route === "locations") return [h("A","Choose"),h("B","Back")]
                        if (route === "stock") return shell.center.shopSelection.lesson ? [h("A","Choose Pokémon"),h("B","Tutors")] : [h("←→","Quantity"),h("A","Add"),h("Select","Basket · "+shell.center.basketCount),h("B","Shops")]
                        return [h("A",route === "confirm" ? "Confirm" : "Select"),h("B","Back")]
                    }
                    if (shell.center.clinicOpen) return shell.center.busy ? [] : [h("Select","Backups"),h("X","Link"),h("A",shell.center.treatment === "ready" && shell.center.canHeal ? "Heal team" : "OK")]
                    if (party.section === "saves") return shell.center.confirming ? [h("A","Restore"),h("B","Cancel")] : [h("X",shell.center.route === "adventures" ? "Search" : "Refresh"),h("Select","Backup"),h("A","Open"),h("B","Back")]
                    if (party.section === "activities" && party.activities.route === "practice") {
                        const practice = party.activities.practice
                        if (practice.stage === "starting" || practice.stage === "waiting") return [h("B","Leave practice")]
                        if (practice.stage === "first" && !practice.ready) return [h("A","Back"),h("B","Back")]
                        return [h("A",practice.stage === "ready" ? "Begin" : practice.stage === "events" ? "Next" : practice.stage === "finished" ? "Again" : practice.stage === "moves" ? "Move" : "Choose"),h("B",practice.running ? "Leave practice" : "Back")]
                    }
                    if (party.section === "activities" && party.activities.route === "link") {
                        const link = party.activities.link
                        if (link.stage === "browse") return link.rows.length ? [h("A","Connect"),h("B","Back")] : [h("B","Back")]
                        if (link.stage === "lobby") return [h("X","Disconnect"),h("A","Invite"),h("B","Back")]
                        if (link.stage === "events") return []
                        if (link.stage === "price") return [h("↑↓","Price ±" + link.priceStep),h("←→","Step"),h("A","Offer"),h("B","Cancel")]
                        if (link.stage === "concede") return [h("A","Concede"),h("B","Keep battling")]
                        if (link.stage === "moves") return [h("X","Team"),h("Y","Bag"),h("A",link.battlePanel === "target" ? "Use" : "Choose"),h("B",link.battlePanel !== "moves" ? "Attacks" : "Concede")]
                        if (link.stage === "stake" || link.stage === "choose" && link.mode !== "battle") return [h("X Y","Party / Boxes"),h("A","Choose"),h("B","Back")]
                        if (link.canSetTerms) return (link.stakeText.indexOf("₽") >= 0 ? [h("↑↓","Amount ±" + link.priceStep),h("←→","Step")] : []).concat([h("Select","Stake"),h("A","Ready"),h("B","Back")])
                        return [h("A",link.stage === "pair" ? "Connect" : link.stage === "review" ? "Confirm" : link.stage === "moves" ? "Move" : "Choose"),h("B",link.pending ? "Pause" : "Back")]
                    }
                    if (party.section === "activities") return party.activities.route === "playroom" && party.activities.hasParty ? [h("↑","Practice"),h("←→","Partner"),h("Select","Play"),h("X","Greet"),h("A","Call")] : [h("A","Select"),h("B","Back")]
                    if (party.moveStage === "release-confirm") return [h("X","Release"),h("B","Keep Pokemon")]
                    if (party.moveOpen) return party.moveStage === "writing" || party.moveStage === "checking" ? [] : [h("A",party.moveStage === "name-confirm" ? "Rename" : party.moveStage === "name-error" ? "Edit name" : party.moveStage === "item-confirm" ? "Confirm" : party.moveStage === "confirm" ? "Confirm" : party.moveStage === "result" ? "OK" : "Choose"),h("B",party.moveStage === "places" ? "Cancel" : "Back")]
                    if (party.detailOpen) return [h("A","Select"),h("B","Close")]
                    if (party.boxFocused) return (party.canRenameBox ? [h("X","Rename box")] : []).concat([h("←→","Box"),h("↓","Slots"),h("B","Back")])
                    const actions = [h("Select","Backups"),h("A",party.available && !party.activitiesFocused ? "Actions" : "Open"),h("B","Back")]
                    if (party.canRenameBox) actions.unshift(h("X","Rename box"))
                    if (party.section === "storage" && party.available && !party.activitiesFocused && party.focusIndex < 6) actions.unshift(h("↑","Boxes"))
                    return actions
                }
                if (shell.trainerHistoryFace) {
                    const hall = shell.hall
                    if (hall.editor.open) return hall.editor.route === "form" ? [h("Y","Save"),h("A","Edit"),h("B","Discard")] : [h("X",hall.editor.route === "team" ? "Level" : "Search"),h("A",hall.editor.route === "team" ? "Name" : "Choose"),h("B","Back")]
                    let result = []
                    if (hall.archive && hall.editable) result.push(h("Select","New memory"))
                    if (!hall.archive) result.push(h("Select","Refresh"))
                    if (!hall.archive && hall.account.available) result.push(h("X","Account"))
                    else if (hall.route === "archive-journey") result.push(h("X","Champions"))
                    else if (hall.archive && hall.editable && hall.rows.length && !hall.overview) result.push(h("X","Edit"))
                    if (hall.route === "archive-champions" || hall.route === "archive-champion-detail") result.push(h("← →","Records"))
                    result.push(h("A",hall.overview ? (hall.route === "archive-journey" ? "Champions" : "Journey") : "Open"),h("B","Back")); return result
                }
                if (shell.page === 0) return [h("A",shell.multiverseHome ? (shell.multiverse.selected.id ? "Play" : "Explore") : shell.home.actionHint),h("B","Back")]
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
                        tint: button === "B" ? Theme.pink : button === "X" || button === "←→" ? Theme.blue : button === "Y" ? Theme.yellow : Theme.green
                    }
                }
                Hint { button: "Start"; label: "System"; tint: Theme.yellow; visible: !shell.party.moveOpen && !shell.party.activities.link.invitationOpen && !shell.social.online.open }
            }
        }
        LibraryPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "library" }
        TrainerSettingsPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "trainer-settings" }
        TrainerSetupPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "trainer-setup" }
        DevicePanel { anchors.fill: screen; shell: shellController; visible: shell.service === "device" }
        DiagnosticsPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "diagnostics" }
        PartyPanel { anchors.fill: screen; shell: shellController; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.centerFace && !shell.center.clinicOpen && !shell.center.shopsOpen && shell.party.section !== "saves" && shell.party.section !== "activities" }
        PokemonShop { anchors.fill: screen; shell: shellController; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.centerFace && shell.center.shopsOpen }
        PokemonClinic { anchors.fill: screen; shell: shellController; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.centerFace && shell.center.clinicOpen }
        CenterActivitiesPanel { anchors.fill: screen; shell: shellController; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.centerFace && !shell.center.clinicOpen && !shell.center.shopsOpen && shell.party.section === "activities" }
        SaveCenterPanel { anchors.fill: screen; shell: shellController; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.centerFace && !shell.center.clinicOpen && !shell.center.shopsOpen && shell.party.section === "saves" }
        Rectangle {
            id: merchantToast; property string message: ""
            anchors.right: screen.right; anchors.bottom: screen.bottom; anchors.margins: 12
            width: Math.min(440, screen.width-24); height: 54; radius: 8; color: "#f8e5a9"; border.color: "#b28b42"; z: 1
            visible: toastTimer.running && shell.centerFace && !shell.menuOpen && !shell.drawerOpen && !shell.serviceOpen
            Text { anchors.fill: parent; anchors.margins: 10; text: merchantToast.message; font.pixelSize: 16; color: Theme.ink; wrapMode: Text.WordWrap; verticalAlignment: Text.AlignVCenter }
            Timer { id: toastTimer; interval: 4500 }
            Connections { target: shell.center; function onMerchantDiscovered(message) { merchantToast.message=message; toastTimer.restart() } }
        }
        ScrapePanel { anchors.fill: screen; shell: shellController; z: 3.1; visible: shell.scraper.open && !shell.keyboard.open }
        DownloadsPanel { anchors.fill: screen; flow: shell.downloads; z: 3.2; visible: shell.downloads.open }
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
            HomeMenuCard {
                visible: !shell.notificationsOpen
                anchors.centerIn: parent
                caption: shell.homeMenuCaption
                actions: shell.homeMenuActions
                currentIndex: shell.homeMenuFocus
                onChosen: index => shell.activateHomeMenu(index)
            }
            Panel {
                visible: shell.notificationsOpen
                anchors.centerIn: parent; width: 550
                height: shell.social.notifications.length ? 120 + Math.min(3,shell.social.notifications.length)*71 : 200
                patterned: false
                Text { x: 23; y: 17; text: "Notifications"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 26 }
                Text { x: 23; y: 52; text: "Your updates"; color: Theme.muted; font.pixelSize: 15 }
                MountedPanel {
                    x: 10; y: 82; width: parent.width-20; height: parent.height-94; color: "#dce9d9"
                    ListView {
                        id: notificationList
                        anchors.fill: parent; anchors.margins: 12; clip: true; spacing: 9
                        model: shell.social.notifications; currentIndex: Math.min(shell.notificationFocus, count-1)
                        onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                        delegate: CapButton {
                            required property var modelData
                            required property int index
                            width: notificationList.width; height: 62; claimsFocus: false
                            label: modelData.name; detail: modelData.detail
                            tint: modelData.request ? Theme.pink : Theme.blue
                            selected: index===notificationList.currentIndex
                            onActivated: shell.activateNotification(index)
                        }
                    }
                    Text { anchors.centerIn: parent; visible: !notificationList.count; text: "You're all caught up!"; font.family: Theme.displayFamily; font.pixelSize: 23; color: Theme.ink }
                }
            }
        }
        Item {
            id: nearbyInvitation
            objectName: "nearby-invitation"
            property var link: shell.party.activities.link
            readonly property bool online: !!shell.social.online.open
            readonly property bool runtime: runtimeMultiplayer.incoming
            readonly property bool incoming: runtime || (online ? !!shell.social.online.incoming : link.invitationIncoming)
            function answer(accept) { if(runtime) runtimeMultiplayer.answer(accept); else if (online) shell.social.answerOnline(accept); else link.answerInvitation(accept) }
            width: parent.width; height: Theme.footerTop; z: 6
            visible: (runtime || online || link.invitationOpen) && !sessionState.blocked && !adventureLaunch.active
            Rectangle { anchors.fill: parent; color: "#77112323" }
            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted=true }
            Panel {
                anchors.centerIn: parent; width: 470; height: 225; surface: "#f6f0d7"
                Text { x: 27; y: 23; text: nearbyInvitation.incoming ? "INCOMING ACTIVITY" : "TOGETHER"; font.family: Theme.brandFamily; font.pixelSize: 16; color: "#4c8175" }
                Text { x: 27; y: 56; width: parent.width-54; height: 85; text: nearbyInvitation.runtime ? runtimeMultiplayer.invitation : nearbyInvitation.online ? shell.social.online.status : nearbyInvitation.link.invitationText; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: 26; color: Theme.ink; wrapMode: Text.WordWrap }
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
        Rectangle {
            anchors.right: parent.right; anchors.rightMargin: 28
            anchors.bottom: parent.bottom; anchors.bottomMargin: 62
            width: 340; height: message.implicitHeight+28; radius: 12; z: 8
            color: "#ffefad"; border.color: "#a87927"; border.width: 2
            visible: shell.achievementToast.length>0 && !adventureLaunch.active && !sessionState.blocked
            Text { id: message; x: 14; y: 14; width: parent.width-28; text: shell.achievementToast
                color: "#294440"; font.family: Theme.displayFamily; font.pixelSize: 17; wrapMode: Text.WordWrap }
        }
        LaunchPanel { anchors.fill: parent; visible: adventureLaunch.preparing; launch: adventureLaunch }
    }
    readonly property var shell: shellController
}
