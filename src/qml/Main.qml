import QtQuick
import QtQuick.Window
import QtQuick.Shapes

Window {
    id: window
    objectName: "trainer-window"
    width: 960; height: 540
    minimumWidth: 800; minimumHeight: 450
    visible: true
    title: "TrainerOS"
    color: Theme.chassisDark
    readonly property real uiScale: shell.settings.displayDensity(Screen.width, Screen.height,
        width, height, shell.settings.displayProfile, shell.settings.interfaceScale)
    Binding { target: controllerInput; property: "pointerMode"; value: !shell.social.incomingCall.id && (filesPane.takesFocus || browserPane.takesFocus) && (!adventureLaunch.active || adventureLaunch.minimized) && !sessionState.blocked }
    Binding { target: controllerInput; property: "nativeKeyboard"; value: !shell.social.incomingCall.id && !browserPane.popup && (browserPane.typing || (browserPane.takesFocus && !browserPane.toolsFocused)) }
    Binding { target: controllerInput; property: "browserMode"; value: !shell.social.incomingCall.id && browserPane.takesFocus }
    RadialInputWindow { flow: shell.settings.gameInput; bridge: radialOverlay; permitted: adventureLaunch.active && !adventureLaunch.minimized && !sessionState.blocked && !adventureExitPresentation.visible }
    AppControlsWindow { flow: shell.apps; bridge: appsOverlay }
    ExternalKeyboardWindow { keyboard: externalKeyboard; bridge: keyboardOverlay }
    AdventureExitWindow {
        presentation: adventureExitPresentation
        width: window.width; height: window.height
        x: window.x; y: window.y
    }
    InvitationBadge {
        bridge: invitationOverlay
        players: playerOverlay
        playing: adventureLaunch.active && !adventureLaunch.minimized
        permitted: !sessionState.blocked && !adventureExitPresentation.visible && !adventureExitPresentation.capturing
    }
    AchievementCelebrationWindow {
        celebration: achievementCelebration
        bridge: achievementOverlay
        permitted: adventureLaunch.active && !adventureLaunch.minimized && !sessionState.blocked
            && !adventureExitPresentation.visible && !adventureExitPresentation.capturing
    }
    Binding { target: Theme; property: "themeId"; value: shell.settings.theme }
    Binding { target: Theme; property: "controller"; value: controllerInput }
    Binding { target: Theme; property: "reducedMotion"; value: shell.settings.reducedMotion }
    Binding { target: Theme; property: "television"; value: shell.settings.displayProfile === "tv" }
    Binding { target: Theme; property: "uiScale"; value: window.uiScale }
    Binding { target: Theme; property: "viewportWidth"; value: window.width / window.uiScale }
    Binding { target: Theme; property: "viewportHeight"; value: window.height / window.uiScale }
    Binding { target: shell.social; property: "surfaceAvailable"; value: !sessionState.blocked && ((window.active && (!adventureLaunch.active || adventureLaunch.minimized) && shell.unobstructed) || (playerOverlay.chatOpen && playerOverlay.conversationReady && adventureExitPresentation.ready)) }
    Binding { target: shell.social; property: "conversationVisible"; value: playerOverlay.chatOpen && playerOverlay.conversationReady || ((shell.primaryId === "social" || (shell.primaryId === "gamehub" && shell.hubFace === "lobby")) && !shell.serviceOpen
            && (shell.socialSurface === "chats" || shell.socialSurface === "communities" || shell.social.lobbyChatVisible)) }
    Binding { target: shell.social; property: "gameActive"; value: adventureLaunch.active }
    Connections {
        target: adventureLaunch
        function onChanged() {
            if (adventureLaunch.active && !adventureLaunch.minimized && shell.social.stream.active)
                shell.social.streamAction("stop-watch")
        }
    }
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
        // A live scene graph, rasterized at native resolution. Bounds change with
        // window/aspect/user density; there is no fixed canvas or offscreen layer.
        scale: window.uiScale
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
            AccountAvatar {
                id: headerAvatar
                x: 14; y: (parent.height-height)/2; width: 44
                name: shell.social.account.name || "Player"
                source: (shell.social.account.profile || {}).avatar || ""
                presence: shell.social.account.state === "connected" ? shell.social.account.presence || "" : ""
            }
            Text {
                id: headerName; objectName: "header-account-name"
                readonly property string fullName: (shell.social.account.userId ? (shell.social.account.profile || {}).name || shell.social.account.name || "Your profile" : "Sign in").trim()
                readonly property var words: fullName.split(/\s+/)
                readonly property bool twoLines: words.length === 2
                x: headerAvatar.x + headerAvatar.width + 14
                y: headerAvatar.y + (headerAvatar.height-height)/2
                width: parent.width-x-16; height: twoLines ? 40 : 36
                text: twoLines ? words.join("\n") : fullName
                color: "#fffef9"; font.family: Theme.displayFamily; font.pixelSize: 26
                minimumPixelSize: 16; fontSizeMode: Text.Fit
                maximumLineCount: twoLines ? 2 : 1
                elide: Text.ElideRight; textFormat: Text.PlainText; verticalAlignment: Text.AlignVCenter
            }
            MouseArea {
                objectName: "account-profile-toggle"; anchors.fill: parent
                enabled: shell.unobstructed && !shell.quickAppsOpen
                onClicked: shell.openAccountProfile()
                Accessible.role: Accessible.Button; Accessible.name: "Your Fluxer profile"
                Accessible.onPressAction: shell.openAccountProfile()
            }
            Text {
                x: headerName.x; y: 51; width: parent.width-x-16
                visible: !!shell.liveGame.id; text: "Still running · Home to return"
                horizontalAlignment: Text.AlignHCenter; color: Theme.textColor("#ffe3a4"); font.pixelSize: 9
                fontSizeMode: Text.HorizontalFit; minimumPixelSize: 7
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
                        const l = -spread, w = width + (index === shell.primaryNames.length - 1 ? 0 : spread), h = height + offset;
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
                            fillColor: Theme.surfaceColor("#08101e1d")
                            PathSvg { path: tab.outline(10, 3) }
                        }
                        ShapePath {
                            strokeColor: "transparent"; fillColor: Theme.surfaceColor("#10101e1d")
                            PathSvg { path: tab.outline(8, 2.5) }
                        }
                        ShapePath {
                            strokeColor: "transparent"
                            fillColor: shell.page === index ? Theme.surfaceColor("#28101e1d") : Theme.surfaceColor("#1c101e1d")
                            PathSvg { path: tab.outline(6, 2) }
                        }
                        ShapePath {
                            strokeColor: "transparent"; fillColor: Theme.surfaceColor("#50101e1d")
                            PathSvg { path: tab.outline(4, 1) }
                        }
                        ShapePath {
                            strokeColor: "transparent"; fillColor: Theme.surfaceColor("#60101e1d")
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
                            GradientStop { position: 0; color: Theme.surfaceColor("#90101e1d") }
                            GradientStop { position: 0.7; color: Theme.surfaceColor("#70101e1d") }
                            GradientStop { position: 1; color: Theme.surfaceColor("#00101e1d") }
                        }
                    }
                    Rectangle {
                        // Deep seams between adjacent keys, and a small contact
                        // seam inside the final key, clear of the chassis slope.
                        x: parent.width - (index === shell.primaryNames.length - 1 ? 1 : 0); y: 0
                        width: index === shell.primaryNames.length - 1 ? 1 : Theme.tabSpacing
                        height: index === shell.primaryNames.length - 1 ? parent.height - Theme.tabBevel : Theme.tabBaseline
                        gradient: Gradient {
                            GradientStop { position: 0; color: Theme.surfaceColor("#a0101e1d") }
                            GradientStop { position: 0.6; color: Theme.surfaceColor("#90101e1d") }
                            GradientStop { position: 1; color: Theme.surfaceColor("#00101e1d") }
                        }
                    }
                    Rectangle { x: 2; y: 1; width: parent.width - 4; height: 2; color: Theme.surfaceColor("#90ffffff") }
                    Rectangle { x: 2; y: 3; width: 1; height: parent.height - 17; color: Theme.surfaceColor("#55ffffff") }
                    Text {
                        anchors.centerIn: parent; anchors.verticalCenterOffset: Theme.topRimHeight / 2
                        width: parent.width - 10; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
                        text: modelData; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: shell.page === index ? 18 : 17; font.weight: Font.DemiBold
                    }
                    Rectangle { x: 18; y: parent.height - 8; width: parent.width - 36; height: 3; radius: 1.5; color: Theme.surfaceColor("#7a4a24"); visible: shell.page === index }
                    Rectangle {
                        visible: modelData === "Social" && (shell.social.account.unreadCount || 0) > 0
                        anchors.right: parent.right; anchors.rightMargin: 6; y: Theme.topRimHeight + 3
                        width: 23; height: 17; radius: 7; color: Theme.yellow; border.color: Theme.surfaceColor("#8e642e")
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
            onDismissed: shell.dismissPassiveNotice()
            visible: !!notice.title && window.active && !sessionState.blocked && !shell.homeMenuOpen && (!adventureLaunch.active || adventureLaunch.minimized)
            z: 30; x: parent.width - width - 26; y: Theme.brandHeight + 21
        }
        Row {
            id: faceNavigation
            readonly property bool gameBrowser: shell.page === 1 && !shell.collectionsRoot
            objectName: "navigation-chassis"; x: 28; y: Theme.brandHeight + Theme.screenBevel + 5; spacing: 12; z: 2
            visible: shell.faceNames.length > 0 && !browserPane.expanded
            FaceStep { objectName: "face-previous"; visible: shell.faceNames.length>1; enabled: shell.pairedNavigationAvailable; onActivated: shell.pressButton("L2") }
            Row {
                visible: faceNavigation.gameBrowser
                spacing: 18
                Repeater {
                    model: faceNavigation.gameBrowser ? (shell.faceNames.length > 1 ? [-1,0,1] : [0]) : []
                    Text {
                        required property int modelData
                        readonly property int targetIndex: (shell.faceIndex + modelData + shell.faceNames.length) % Math.max(1,shell.faceNames.length)
                        objectName: modelData < 0 ? "browser-context-previous" : modelData > 0 ? "browser-context-next" : "browser-context-current"
                        text: shell.faceNames[targetIndex] || ""
                        width: Math.min(implicitWidth, Math.max(70,(Theme.screenBounds.width-180)/3))
                        height: 26; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                        color: modelData === 0 ? Theme.ink : Theme.muted
                        opacity: modelData === 0 ? 1 : .72
                        font.family: Theme.displayFamily; font.pixelSize: modelData === 0 ? 15 : 13
                        font.bold: modelData === 0
                        Accessible.role: modelData === 0 ? Accessible.StaticText : Accessible.Button
                        Accessible.name: text
                        Accessible.onPressAction: if (shell.pairedNavigationAvailable && modelData !== 0) shell.selectFace(targetIndex)
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.bottomMargin: -2; height: 2; color: Theme.focusGlow; visible: modelData === 0 }
                        MouseArea { anchors.fill: parent; anchors.topMargin: -5; anchors.bottomMargin: -5; enabled: shell.pairedNavigationAvailable && modelData !== 0; onClicked: shell.selectFace(parent.targetIndex) }
                    }
                }
            }
            Repeater {model: faceNavigation.gameBrowser ? [] : shell.faceNames
                Text {
                    required property int index;required property string modelData
                    objectName: "face-tab-" + index
                    text:modelData;color:shell.faceIndex===index?Theme.ink:Theme.muted
                    height: 26; verticalAlignment: Text.AlignVCenter
                    width: Math.min(implicitWidth,Math.max(65,(Theme.screenBounds.width-160)/Math.max(1,shell.faceNames.length)-12)); elide: Text.ElideRight
                    font.family:Theme.displayFamily;font.pixelSize:15;font.bold:shell.faceIndex===index
                    Rectangle {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.bottomMargin:-3;height:2;color:Theme.focusGlow;visible:shell.faceIndex===index}
                    MouseArea { anchors.fill: parent; anchors.topMargin: -5; anchors.bottomMargin: -8; enabled: shell.pairedNavigationAvailable; onClicked: shell.selectFace(index) }
                }
            }
            FaceStep { objectName: "face-next"; forward: true; visible: shell.faceNames.length>1; enabled: shell.pairedNavigationAvailable; onActivated: shell.pressButton("R2") }
        }
        Item {
            id: screen
            objectName: "primary-screen"
            x: Theme.screenBounds.x; y: Theme.screenBounds.y
            width: Theme.screenBounds.width; height: Theme.screenBounds.height
            Item {
                objectName: "page-viewport"
                enabled: !shell.drawerOpen && !shell.quickAppsOpen && !shell.serviceOpen
                anchors.fill: parent; anchors.margins: Theme.panelInset
                // Home has no face row; its content starts below the active tab.
                anchors.topMargin: shell.primaryId === "home" || (shell.page === 1 && shell.collectionsRoot) ? Theme.brandHeight + Theme.screenBevel - Theme.screenTop : Theme.contentTopInset
                clip: true
                anchors.bottomMargin: Theme.panelInset
                HomeDashboard { id: experienceHost; anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings" || shell.service === "account-profile") && shell.primaryId === "home" }
                GameHubPage { id: gameHubPage; anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings" || shell.service === "account-profile") && shell.primaryId === "gamehub" }
                AppsPage { anchors.fill: parent; flow: shell.apps; active: shell.primaryId === "apps" && shell.appsFace === "apps"; visible: (!shell.serviceOpen || shell.service === "settings" || shell.service === "account-profile") && active }
                FilesPane { id: filesPane; anchors.fill: parent; flow: shell.files; applicationStatus: shell.apps.status; visible: !shell.serviceOpen && shell.primaryId === "apps" && shell.appsFace === "files"; takesFocus: visible && !shell.notice.length && !shell.drawerOpen && !shell.quickAppsOpen && !shell.keyboard.open && !shell.menuOpen && !shell.homeMenuOpen && !shell.files.view.menuOpen && !Object.keys(shell.files.dialog).length }
                CratesPage {
                    anchors.fill: parent
                    visible: (!shell.serviceOpen || shell.service === "settings" || shell.service === "account-profile") && shell.primaryId === "apps" && shell.appsFace === "crates"
                    flow: shell.crates
                    active: visible && !shell.serviceOpen && !shell.drawerOpen && !shell.quickAppsOpen && !shell.menuOpen && !shell.homeMenuOpen && !shell.keyboard.open && !shell.notice.length
                }
                SocialPage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings" || shell.service === "account-profile") && shell.primaryId === "social" }
                SeriesPage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings" || shell.service === "account-profile") && shell.page === 1 && shell.collectionsRoot }
                WorldsPage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings" || shell.service === "account-profile") && shell.page === 1 && !shell.collectionsRoot && !shell.multiverseFace }
                MultiversePage { anchors.fill: parent; shell: shellController; visible: (!shell.serviceOpen || shell.service === "settings" || shell.service === "account-profile") && shell.page === 1 && !shell.collectionsRoot && shell.multiverseFace }
            }
        }
        BrowserPane {
            id: browserPane
            flow: shell.browser
            active: !shell.serviceOpen && shell.primaryId === "apps" && shell.appsFace === "browser"
            mediaAllowed: active && window.active && (!adventureLaunch.active || adventureLaunch.minimized)
            visible: active
            takesFocus: active && !shell.notice.length && !shell.drawerOpen && !shell.quickAppsOpen && !shell.keyboard.open && !shell.menuOpen && !shell.homeMenuOpen && !shell.downloads.open && !sessionState.blocked && (!adventureLaunch.active || adventureLaunch.minimized)
            typing: active && !shell.notice.length && !shell.drawerOpen && !shell.quickAppsOpen && shell.keyboard.live && !shell.menuOpen && !shell.homeMenuOpen && !shell.downloads.open && !sessionState.blocked && (!adventureLaunch.active || adventureLaunch.minimized)
            x: expanded ? 0 : Theme.screenBounds.x + Theme.panelInset
            y: expanded ? 0 : Theme.screenBounds.y + Theme.contentTopInset
            width: expanded ? parent.width : Theme.screenBounds.width - 2*Theme.panelInset
            height: shell.keyboard.open && shell.keyboard.live
                ? Math.max(80,keyboardPanel.y+keyboardPanel.height-keyboardPanel.occupiedHeight-y)
                : expanded ? parent.height : Theme.screenBounds.y+Theme.screenBounds.height-Theme.panelInset-y
            z: expanded ? 1.8 : .5
            Behavior on x { NumberAnimation {duration:Theme.motion(180);easing.type:Easing.InOutCubic} }
            Behavior on y { NumberAnimation {duration:Theme.motion(180);easing.type:Easing.InOutCubic} }
            Behavior on width { NumberAnimation {duration:Theme.motion(180);easing.type:Easing.InOutCubic} }
            Behavior on height { NumberAnimation {duration:Theme.motion(180);easing.type:Easing.InOutCubic} }
        }
        FilesControlTab {
            id: browserTab
            flow: browserPane.toolFlow
            visible: browserPane.active
            x: browserPane.expanded ? (parent.width-width)/2 : Theme.screenBounds.x + 2*Theme.adventureTabWidth - Theme.drawerHandleOverlap + 8
            y: (browserPane.expanded ? parent.height : Theme.footerTop)-height
            width: browserPane.expanded ? Math.min(430,parent.width-30) : Theme.screenBounds.x+Theme.screenBounds.width-x
            z: browserPane.expanded ? 1.9 : 1
            takesFocus: browserPane.takesFocus
            recessed: !browserPane.takesFocus || shell.drawerOpen || shell.quickAppsOpen || (!!browserPane.popup && !browserPane.listPopover)
            Behavior on x { NumberAnimation {duration:Theme.motion(180);easing.type:Easing.InOutCubic} }
            Behavior on y { NumberAnimation {duration:Theme.motion(180);easing.type:Easing.InOutCubic} }
        }
        MouseArea {
            anchors.fill: parent; z: browserPane.expanded ? 1.95 : 1
            enabled: (shell.drawerOpen || shell.quickAppsOpen) && !shell.apps.menu.length && !shell.menuOpen && !shell.keyboard.open && !shell.homeMenuOpen && !shell.notice.length
            onClicked: shell.closeDrawers()
        }
        QuickAppsDrawer {
            id: quickDrawer
            x: Theme.screenBounds.x
            anchors.bottom: footer.top
            shell: shellController; z: 3
            visible: !shell.serviceOpen && (!browserPane.expanded || shell.quickAppsOpen || shell.drawerOpen)
            handlesRecessed: shell.quickAppsOpen || shell.drawerOpen || panelVisible || drawer.panelVisible
            otherPanelVisible: drawer.panelVisible
        }
        ContinueDrawer {
            id: drawer
            objectName: "continue-drawer"
            x: Theme.screenBounds.x + (expandedWidth-width)/(expandedWidth-closedWidth)*(closedWidth-Theme.drawerHandleOverlap)
            anchors.bottom: footer.top; shell: shellController
            z: 2
            visible: !shell.serviceOpen && (shell.chooseAdventureAvailable || shell.drawerOpen || panelVisible) && (!browserPane.expanded || shell.quickAppsOpen || shell.drawerOpen)
            handlesRecessed: shell.quickAppsOpen || shell.drawerOpen || panelVisible || quickDrawer.panelVisible
            otherPanelVisible: quickDrawer.panelVisible
        }
        FilesControlTab {
            id: filesTab
            x: Theme.screenBounds.x + 2*Theme.adventureTabWidth - Theme.drawerHandleOverlap + 8
            width: Theme.screenBounds.x + Theme.screenBounds.width - x
            anchors.bottom: footer.top; flow: shell.files; z: 1
            visible: !shell.serviceOpen && shell.primaryId === "apps" && shell.appsFace === "files"
            takesFocus: filesPane.takesFocus
            recessed: shell.drawerOpen || shell.quickAppsOpen || drawer.panelVisible || quickDrawer.panelVisible
        }
        FilesPopup { anchors.fill: parent; z: 7; flow: shell.files }
        Item {
            id: filesPointer; z: 6; width: 14; height: 18; visible: pointerTimer.running && filesPane.takesFocus
            x: 500; y: 260
            Shape { anchors.fill: parent; ShapePath { strokeColor: Theme.paper; strokeWidth: 1; fillColor: Theme.ink; PathSvg { path: "M 1 1 L 1 16 L 5 12 L 8 18 L 11 16 L 8 10 L 14 10 Z" } } }
            Timer { id: pointerTimer; interval: 2500 }
            Connections { target: controllerInput; function onPointerMoved(dx,dy) {
                if(!filesPane.takesFocus)return
                filesPointer.x=Math.max(Theme.screenBounds.x+3,Math.min(Theme.screenBounds.x+Theme.screenBounds.width-12,filesPointer.x+dx))
                filesPointer.y=Math.max(Theme.screenBounds.y+Theme.contentTopInset,Math.min(Theme.footerTop-12,filesPointer.y+dy))
                pointerTimer.restart()
                const point=filesPane.mapFromItem(filesPointer.parent,filesPointer.x,filesPointer.y)
                if(filesPointer.y >= Theme.footerTop-filesTab.height && filesPointer.x>=filesTab.x) filesTab.point(filesPointer.x-filesTab.x)
                else filesPane.point(point.x,point.y)
            } }
        }
        Item {
            id: footer
            visible: !adventureLaunch.preparing
            x: 0; y: Theme.footerTop; width: parent.width; height: Theme.footerHeight
            BatteryGauge { x: 18; y: 5; status: powerStatus }
            Text {
                objectName: "controller-warning"
                x: 132; y: 10; visible: !controllerInput.connected; text: "!  Connect a controller"
                color: Theme.textColor("#ffe29c"); font.pixelSize: 11
                Accessible.role: Accessible.AlertMessage; Accessible.name: text
            }
            function hint(button, label) { return {button:button, label:label} }
            readonly property var localActions: {
                const h = hint
                if (shell.social.incomingCall.id) return [h("A","Answer"),h("B","Decline")]
                if (shell.social.lobbyCreateOpen || shell.social.lobbyPickerOpen || shell.social.lobbyVoteOpen) return [h("↑↓","Choose"),h("A","Select"),h("B","Close")]
                if (shell.social.myLobbiesOpen) return [h("\u2191\u2193","Choose"),h("A","Open"),h("B","Back")]
                if (shell.social.online.open) return shell.social.online.incoming ? [h("A","Accept"),h("B","Decline")] : [h("B","Cancel")]
                if (shell.moduleInvitation.open) return shell.moduleInvitation.incoming ? [h("A","Accept"),h("B","Decline")] : [h("B","Cancel")]
                if (shell.homeMenuOpen) {
                    if (!shell.notificationsOpen) return [h("↑↓","Choose"),h("A","Select"),h("B","Back")]
                    const selectedNotice = shell.notifications[shell.notificationFocus] || {}
                    const noticeHints = [h("↑↓","Choose"),h("A","Open")]
                    if (!selectedNotice.pending) noticeHints.push(h("X",selectedNotice.ringing ? "Decline" : "Remove"))
                    return noticeHints.concat([h("Select","Clear updates"),h("B","Back")])
                }
                if (shell.notice.length) return [h("A", shell.modeConfirmation ? "Continue" : "OK"), h("B","Cancel")]
                if (shell.downloads.open) return [h("↑↓","Choose"),h("←→","Actions"),h("A","Select"),h("B","Close")]
                if (shell.menuOpen) return shell.powerMenu ? [h("A","Select"),h("B","Back")] : [] // Controls owns its contextual hints.
                if (shell.keyboard.open) return shell.keyboard.optionsOpen ? [h("↑↓","Option"),h("A",shell.keyboard.optionIndex===0?"Layout":"On / Off"),h("B","Back")] : [h("X","Case"),h("Y",shell.keyboard.nextLayout),h("A","Type"),h("Select",shell.keyboard.submitLabel),h("B",shell.keyboard.live ? "Hide" : shell.keyboard.submitLabel === "Send" ? "Keep draft" : "Cancel")]
                if (shell.scraper.open) return shell.scraper.selectingSystems&&!shell.scraper.overview ? [h("↑↓","Systems"),h("←→","Actions"),h("A","Select"),h("B","Back")] : [h("A","Select"),h("B",shell.scraper.busy&&!shell.scraper.choosingMatch&&!shell.scraper.overview?"Stop":"Back")]
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
                if (shell.service === "settings" && shell.settings.categoryId==="game-input" && shell.settings.controlsFocused) return [h("↑↓","Choose"),h("L2/R2","Tabs"),h("A","Select"),h("B",shell.settings.gameInput.choosing?"Cancel":"Back")]
                if (shell.service === "settings") return [h(shell.settings.controlsFocused?"←→":"↑↓",shell.settings.controlsFocused?"Adjust":"Section"),h("A",shell.settings.controlsFocused ? "Select" : "Open"),h("B",shell.settings.controlsFocused ? "Categories" : "Back")]
                if (shell.service === "device") return [h("←→","Adjust"),h("Y","Refresh"),h("A","Select"),h("B","Back")]
                if (shell.serviceOpen) return [h("A","Select"),h("B","Back")]
                if (shell.gamePerformance.open) return [h("A","Adjust"),h("B","Close")]
                if (shell.pcGames.open) return [h("A","Choose"),h("B",shell.pcGames.picker.open ? "Back" : shell.pcGames.busy ? "Hide" : "Close")]
                if(shell.collectionManager.open) return [h("A","Choose"),h("B","Back")]
                if (shell.page === 1 && shell.collectionsRoot) return shell.gamesRootZone === "collections" ? [h("A","Open"),h("Select","Collection actions"),h("B","Home")] : [h("A","Open platform"),h("B","Home")]
                if (shell.page === 1) {
                    if(shell.multiverseFace && shell.multiverse.systemId === "pc" && (shell.pcAddFocused || !shell.multiverse.games.length)) return [h("A","Add game"),h("B","Games")]
                    const list = shell.multiverseFace ? shell.multiverse.route === "games" : shell.worlds.route === "adventures"
                    let result = list ? [h("X","Search"),h("Y","Filter"),h("A","Play"),h("B","Games")] : [h("A","Open"),h("B","Back")]
                    if(list && shell.multiverseFace) { result.push(h("L3","Favorite")); result.push(h("R3","View")) }
                    if(shell.canHoldConfirm) result.push(h("Hold A","Options"))
                    result.push(h("Select",list ? "Game actions" : "Collection actions"))
                    return result
                }
                const experienceHints = experienceHost.hints(h)
                if (experienceHints) return experienceHints
                if (shell.page === 0) return [h("A",shell.homeGame.id ? (shell.liveGame.id === shell.homeGame.id ? "Return" : shell.home.actionHint) : "Collections"),h("Select","Game actions"),h("B","Back")]
                if (shell.primaryId === "apps" && shell.appsFace === "files") return shell.files.view.menuOpen || Object.keys(shell.files.dialog).length ? [h("A","Select"),h("B","Close")] : [h("A","Open"),h("Hold A","Actions"),h("Select","Controls"),h("R3","View"),h("B","Back")]
                if (shell.primaryId === "apps" && shell.appsFace === "browser") return browserPane.popup ? [h("A","Select"),h("B","Close")] : [h("A","Select"),h("Hold A","Actions"),h("Select","Controls"),h("B","Back")]
                if (shell.primaryId === "apps" && shell.appsFace === "crates") {
                    if (shell.crates.detailOpen) {
                        if (shell.crates.actionsOpen) return [h("A","Choose"),h("B","Back")]
                        return (shell.crates.primaryEnabled ? [h("A",shell.crates.primaryLabel)] : []).concat([h("B","Close")],shell.crates.canRemove ? [h("Select","Remove")] : [])
                    }
                    return shell.crates.opened ? (shell.crates.projectCount ? [h("A","Details"),h("B","Crates")] : [h("B","Crates")]) : [h("A",shell.crates.opening ? "Skip" : "Open"),h("B","Apps")]
                }
                if (shell.primaryId === "apps") return shell.apps.menu.length ? [h("↑↓","Choose"),h("A","Select"),h("B","Back")] : [h("A","Open"),h("Select","Options"),h("X","Quick Apps"),h("Y","Recent games"),h("B","Home")]
                if (shell.primaryId === "gamehub") return gameHubPage.hints(h)
                if (shell.primaryId === "social") return shell.social.hints
                return [h("A",shell.trainer.editing ? "Select" : "Edit Trainer"),h("B",shell.trainer.editing ? "Cancel" : "Back")]
            }
            readonly property var actions: {
                if(shell.apps.menu.length) return [hint("??","Choose"),hint("A","Select"),hint("B","Back")]
                if(shell.quickAppsOpen && !shell.apps.menu.length) return [hint("←→","Apps"),hint("A","Open"),hint("Select","Options"),hint("X","Close"),hint("Y","Recent games")]
                if(shell.keyboard.open || shell.menuOpen || shell.homeMenuOpen || shell.notice.length || !shell.chooseAdventureAvailable) return localActions
                let result = localActions.filter(function(item) { return item.button !== "X" && item.button !== "Y" })
                result.push(hint("X","Quick Apps")); result.push(hint("Y","Recent games"))
                return result
            }
            Row {
                objectName: "shell-button-hints"
                anchors { right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
                spacing: 9
                transformOrigin: Item.Right


                Repeater { model: footer.actions.filter(function(item) { return item.button !== "X" && item.button !== "Y" })
                    delegate: Hint {
                        required property var modelData
                        objectName: "footer-hint-" + modelData.button
                        subtle: true
                        button: modelData.button; label: modelData.label
                        interactive: ["A","B","X","Y","Select","R3","L3"].includes(button)
                        onClicked: shell.pressButton(button)
                        tint: button === "B" ? Theme.pink : button === "X" || button === "←→" ? Theme.blue : button === "Y" ? Theme.yellow : Theme.green
                    }
                }
                Hint { button: "Home"; label: "Options"; subtle: true; interactive: true; onClicked: shell.pressButton("Home"); visible: !shell.moduleControlsBlocked && !shell.social.online.open }
                Hint { button: "Start"; label: "System"; subtle: true; tint: Theme.yellow; interactive: true; onClicked: shell.pressButton("Start"); visible: !shell.moduleControlsBlocked && !shell.social.online.open }
            }
        }
        Loader { anchors.fill: screen; active: shell.service === "library"; visible: active; sourceComponent: LibraryPanel { shell: shellController } }
        Loader { anchors.fill: screen; active: shell.service === "trainer-settings"; visible: active; sourceComponent: TrainerSettingsPanel { shell: shellController } }
        Loader { anchors.fill: screen; anchors.margins: Theme.panelInset; anchors.topMargin: Theme.contentTopInset; active: shell.service === "profile"; visible: active; sourceComponent: PlayerProfilePanel { shell: shellController } }
        Loader { anchors.fill: screen; active: shell.service === "trainer-setup"; visible: active; sourceComponent: TrainerSetupPanel { shell: shellController } }
        Loader { anchors.fill: screen; active: shell.service === "device"; visible: active; sourceComponent: DevicePanel { shell: shellController } }
        Loader { anchors.fill: screen; active: shell.service === "diagnostics"; visible: active; sourceComponent: DiagnosticsPanel { shell: shellController } }
        Loader { anchors.fill: screen; z: 2.8; active: shell.primaryId === "apps" && shell.appsFace === "crates" && shell.crates.detailOpen && !shell.serviceOpen && !shell.quickAppsOpen && !shell.drawerOpen; visible: active; sourceComponent: CratesProjectPopup { flow: shell.crates } }
        Loader { anchors.fill: screen; z: shell.menuOpen ? 4.2 : 3.1; active: shell.scraper.open && !shell.keyboard.open; visible: active; sourceComponent: ScrapePanel { shell: shellController } }
        Loader { anchors.fill: screen; z: shell.menuOpen ? 4.2 : 3.2; active: shell.downloads.open; visible: active; sourceComponent: DownloadsPanel { flow: shell.downloads } }
        MyLobbiesPopup { anchors.fill: screen; social: shell.social; z: 3.5 }
        ChatFilePicker { anchors.fill: screen; picker: shell.social.files.picker; z: 4 }
        Loader { anchors.fill: screen; z: 3.5; active: shell.gamePerformance.open && !shell.keyboard.open && !shell.menuOpen && !shell.homeMenuOpen; visible: active; sourceComponent: GamePerformancePopup { profile: shell.gamePerformance } }
        Loader { anchors.fill: screen; z: 3.4; active: shell.pcGames.open && !shell.keyboard.open && !shell.menuOpen && !shell.homeMenuOpen && !shell.serviceOpen; visible: active; sourceComponent: PcGamePopup { flow: shell.pcGames } }
        LobbyCreatePopup { anchors.fill: screen; social: shell.social; z: 3.6 }
        Loader { anchors.fill: screen; z: 3.7; active: shell.social.lobbyPickerOpen; sourceComponent: LobbyGamePicker { social: shell.social } }
        Loader { anchors.fill: screen; z: 3.7; active: shell.social.lobbyVoteOpen; sourceComponent: LobbyVotePopup { social: shell.social } }
        Loader { anchors.fill: screen; z: 3; active: shell.collectionManager.open; visible: active; sourceComponent: CollectionsPanel { shell: shellController } }
        Loader { anchors.fill: screen; z: 3; active: shell.libraryTools.open; visible: active; sourceComponent: LibraryToolsPanel { shell: shellController } }
        Item {
            id: settingsOverlay
            property bool created: false
            onVisibleChanged: if (visible) created = true
            objectName: "settings-overlay"
            width: parent.width; height: Theme.footerTop; z: 2.2
            visible: shell.service === "settings"
            Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#fafbf7") }
            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted = true }
            Item {
                id: settingsSurface
                objectName: "settings-surface"
                x: 12; y: 0; width: parent.width-24; height: parent.height
                Loader { anchors.fill: parent; active: settingsOverlay.created; sourceComponent: SettingsPanel { shell: shellController } }
            }
        }
        AccountProfilePanel { width: parent.width; height: Theme.footerTop; shell: shellController; z: 2.5 }
        AppsMenu {
            x: screen.x; y: screen.y; width: screen.width; height: screen.height
            flow: shell.apps; z: 2
            visible: shell.apps.menu.length > 0 && !shell.serviceOpen && !shell.menuOpen
        }
        KeyboardPanel {
            id: keyboardPanel
            anchors { left: screen.left; right: screen.right; top: screen.top; bottom: footer.top }
            z: shell.controlsNetworkOpen || shell.scraper.open ? 4.5 : shell.social.lobbyCreateOpen || shell.social.lobbyPickerOpen ? 3.9 : 3; takesFocus: shell.keyboard.open && (shell.controlsNetworkOpen || shell.scraper.open || !shell.menuOpen) && !shell.notice.length; shell: shellController
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
            Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#88102324") }
            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted = true }
            Loader {
                anchors.centerIn: parent
                width: parent.width * 0.92; height: parent.height * 0.90
                active: shell.homeMenuOpen
                sourceComponent: ShellOptions { shell: shellController }
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
            Rectangle { anchors.fill: parent; color: Theme.surfaceColor("#77112323") }
            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted=true }
            Panel {
                anchors.centerIn: parent; width: 470; height: 225; surface: "#f6f0d7"
                Text { x: 27; y: 23; text: nearbyInvitation.incoming ? "INCOMING ACTIVITY" : "TOGETHER"; font.family: Theme.brandFamily; font.pixelSize: 16; color: Theme.textColor("#4c8175") }
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
            Text { x: 12; y: 0; width: Theme.brandWidth-24; height: Theme.brandHeight; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; text: "TRAINER OS"; color: Theme.textColor("#edf5e9"); font.family: Theme.brandFamily; font.pixelSize: 25; font.bold: true }
            FirstRunPanel { anchors.fill: parent; shell: shellController; flow: sessionState.firstRun; visible: sessionState.firstRunPage }
            Loader { x: 0; y: 43; width: parent.width; height: parent.height-63; active: parent.visible && (!sessionState.firstRunPage || sessionState.firstRun.stage === "trainer"); visible: active; sourceComponent: TrainerSetupPanel { entry: true; shell: shellController } }
            Text {
                x: 48; y: parent.height-62; width: parent.width-96; height: 24
                visible: sessionState.firstRunPage && sessionState.firstRun.stage === "trainer"
                text: sessionState.firstRun.error; textFormat: Text.PlainText; elide: Text.ElideRight
                color: Theme.textColor("#853b24"); font.pixelSize: 14
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
        LobbyImportPanel { anchors.fill: parent; visible: lobbyImport.open; importer: lobbyImport }
        LaunchPanel { anchors.fill: parent; visible: launchPresentation.state.visible; launch: launchPresentation }
        IncomingCall {
            anchors.fill: parent; z: 100; social: shell.social
            visible: !!call.id && !sessionState.blocked && (!adventureLaunch.active || adventureLaunch.minimized)
        }
    }
    readonly property var shell: shellController
}
