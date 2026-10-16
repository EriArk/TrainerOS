import QtQuick

ChassisDrawer {
    id: root
    required property var shell
    objectName: "quick-apps-drawer"
    expanded: shell.quickAppsOpen
    expandedHeight: 106
    contentTop: 42
    button: "X"; label: "Quick Apps"; toggleName: "quick-apps-handle"
    onToggled: shell.toggleQuickApps()
    AppsDock {
        parent: root.contentItem
        x: 5; y: 0; width: parent.width-10; height: 54
        flow: root.shell.apps
        enabled: root.expanded && !root.shell.apps.menu.length
    }
}
