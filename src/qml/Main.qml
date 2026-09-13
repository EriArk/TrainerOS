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
            width: Theme.brandWidth; height: Theme.tabBaseline + Theme.activeTabOverlap
            z: 1
            Text {
                x: 16; width: parent.width - 32; anchors.verticalCenter: parent.verticalCenter
                text: "TRAINER OS"; color: "#edf5e9"; font.pixelSize: 24; font.weight: Font.DemiBold
                font.family: Theme.displayFamily; font.letterSpacing: 0.6
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
                            fillColor: shell.page === index ? "#24101e1d" : "#18101e1d"
                            PathSvg { path: tab.outline(6, 2) }
                        }
                        ShapePath {
                            strokeColor: "transparent"; fillColor: "#50101e1d"
                            PathSvg { path: tab.outline(4, 1) }
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
                    Text { anchors.centerIn: parent; text: modelData; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 19; font.weight: Font.DemiBold }
                    Rectangle { x: 18; y: parent.height - 8; width: parent.width - 36; height: 3; radius: 1.5; color: "#7a4a24"; visible: shell.page === index }
                    MouseArea { anchors.fill: parent; onClicked: shell.goToPage(index) }
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
                anchors.fill: parent; anchors.margins: Theme.panelInset
                anchors.topMargin: Theme.contentTopInset; clip: true
                HomePage { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 0 }
                HallOfFamePage { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 4 }
                TrainerPage { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 3 }
                WorldsPage { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 1 }
                PokedexPage { anchors.fill: parent; shell: shellController; visible: !shell.serviceOpen && shell.page === 2 }
            }
        }
        ContinueDrawer {
            id: drawer
            objectName: "continue-drawer"
            x: 0; anchors.bottom: footer.top; shell: shellController
            visible: !shell.serviceOpen && shell.page === 0
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
            Row {
                anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                spacing: 20
                Hint { button: "L1 R1"; label: "Sections"; tint: Theme.blue }
                Hint { button: "A"; label: shell.keyboard.open && !shell.menuOpen ? "Type" : shell.page === 0 && !shell.drawerOpen && !shell.menuOpen && !shell.serviceOpen && shell.notice.length === 0 ? shell.home.actionHint : "Select" }
                Hint { button: "B"; label: shell.keyboard.open && !shell.menuOpen ? "Cancel input" : "Back"; tint: Theme.pink }
                Hint { button: "Start"; label: "System"; tint: Theme.yellow }
            }
        }
        LibraryPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "library" }
        SettingsPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "settings" }
        DevicePanel { anchors.fill: screen; shell: shellController; visible: shell.service === "device" }
        DiagnosticsPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "diagnostics" }
        SaveCenterPanel { anchors.fill: screen; shell: shellController; visible: shell.service === "center" }
        KeyboardPanel {
            anchors { left: screen.left; right: screen.right; top: screen.top; bottom: footer.top }
            z: 2; shell: shellController
        }
        SystemPanel {
            anchors { left: screen.left; right: parent.right; top: screen.top; bottom: footer.top }
            z: 3; shell: shellController
        }
        }
        StoragePanel { anchors.fill: parent; visible: sessionState.blocked; stateController: sessionState }
        LaunchPanel { anchors.fill: parent; visible: adventureLaunch.preparing; launch: adventureLaunch }
    }
    readonly property var shell: shellController
}
