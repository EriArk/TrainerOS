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
    Binding {
        target: root.shell.apps; property: "favoriteCapacity"
        value: Math.max(0,Math.floor((root.expandedWidth-38-20)/62)-2)
    }
    AppsDock {
        parent: root.contentItem
        anchors.horizontalCenter: parent.horizontalCenter
        y: 0; width: parent.width-10
        height: 54
        flow: root.shell.apps
        enabled: root.expanded && !root.shell.apps.menu.length
    }
}
