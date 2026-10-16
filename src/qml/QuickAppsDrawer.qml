import QtQuick

ChassisDrawer {
    id: root
    required property var shell
    objectName: "quick-apps-drawer"
    expanded: shell.quickAppsOpen
    expandedHeight: 76
    contentTop: 12
    showExpandedHeader: false
    button: "X"; label: "Quick Apps"; toggleName: "quick-apps-handle"
    onToggled: shell.toggleQuickApps()
    AppsDock {
        parent: root.contentItem
        anchors.horizontalCenter: parent.horizontalCenter
        y: 0; width: root.shell.apps.rail.length ? Math.min(parent.width-10,root.shell.apps.rail.length*62) : parent.width-10
        height: 54
        flow: root.shell.apps
        enabled: root.expanded && !root.shell.apps.menu.length
    }
}
