import QtQuick

ChassisDrawer {
    id: root
    required property var shell
    objectName: "quick-apps-drawer"
    expanded: shell.quickAppsOpen
    expandedHeight: 178
    button: "X"; label: "Quick Apps"; legend: "FAVOURITES · RUNNING APPS"
    toggleName: "quick-apps-handle"
    onToggled: shell.toggleQuickApps()
    AppsDock {
        parent: root.contentItem; anchors.fill: parent
        flow: root.shell.apps
        enabled: root.expanded && !root.shell.apps.menu.length
    }
}
