import QtQuick
CapButton {
    id: root
    required property var shell
    objectName: "home-launch"
    textSize: 23; centered: true; tint: Theme.yellow
    label: "A  ·  " + (shell.liveGame.id ? "Return to game" : shell.homeGame.id ? shell.home.actionHint : "Choose a game")
    selected: shell.page === 0 && shell.dashboard.focus === "play" && shell.unobstructed && !shell.drawerOpen && !shell.quickAppsOpen
    deferredFocus: true
    onActivated: shell.activateDashboard("play")
}
