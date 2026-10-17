import QtQuick
Item {
    id: root
    required property var social
    required property bool takesFocus
    signal menuAnchored(var item)
    readonly property var selected: social.lobbyRows[social.lobbyFocus] || ({})
    readonly property bool inside: selected.kind === "own" && social.lobbyDetailFocused
    objectName: "social-lobby"
    Rectangle { anchors.fill: parent; color: Theme.paper }
    LobbyCatalogue { anchors.fill: parent; visible: !root.inside; social: root.social; takesFocus: root.takesFocus }
    LobbyRoom { anchors.fill: parent; visible: root.inside; social: root.social; takesFocus: root.takesFocus
        onMenuAnchored: item => root.menuAnchored(item) }
}
