import QtQuick
import QtQuick.Window

Window {
    id: root
    objectName: "achievement-celebration-window"
    required property var celebration
    required property var bridge
    required property bool permitted
    readonly property bool canPresent: permitted && bridge.ready
    readonly property var award: celebration.current
    property var displayedAward: ({})
    onAwardChanged: if (award.title) displayedAward = award
    readonly property bool showing: !!award.title
    title: "TrainerOS — Achievement"
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput
    transientParent: null
    x: Screen.virtualX; y: Screen.virtualY; width: Screen.width; height: Screen.height
    visible: canPresent && (showing || panel.opacity > 0)
    onCanPresentChanged: celebration.setPresentable(canPresent)
    Component.onCompleted: { bridge.attach(root); bridge.setRegions([]); celebration.setPresentable(canPresent) }
    onClosing: event => event.accepted = false
    // No hit regions, active focus, timers on the gameplay thread, or input handlers.
    Item {
        id: panel
        objectName: "achievement-celebration"
        width: Math.min(360,root.width / Theme.uiScale - 32); height: 78
        scale: Theme.uiScale; transformOrigin: Item.Bottom
        x: (root.width-width)/2; y: root.height-height-23*Theme.uiScale + (root.showing ? 0 : 14*Theme.uiScale)
        opacity: root.showing ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.motion(210) } }
        Behavior on y { NumberAnimation { duration: Theme.motion(300); easing.type: Easing.OutCubic } }
        Rectangle { x: 2; y: 6; width: parent.width; height: parent.height-4; radius: 37; color: Theme.surfaceColor("#500c1615") }
        Rectangle {
            anchors.fill: parent; anchors.bottomMargin: 4; radius: 37
            border.width: 1; border.color: Qt.darker(Theme.chassisDark,1.35)
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.lighter(Theme.chassisTop,1.65) }
                GradientStop { position: .16; color: Theme.chassisTop }
                GradientStop { position: .78; color: Theme.chassis }
                GradientStop { position: 1; color: Theme.chassisDark }
            }
        }
        Rectangle { x: 2; y: 2; width: parent.width-4; height: parent.height-9; radius: 34; color: "transparent"; border.color: Qt.alpha(Theme.rim,.7) }
        Rectangle {
            id: medal; x: 7; y: 5; width: 64; height: 64; radius: 32
            border.width: 1; border.color: Qt.darker(Theme.yellow,1.8)
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.surfaceColor("#fff3b9") }
                GradientStop { position: .45; color: Theme.yellow }
                GradientStop { position: 1; color: Theme.surfaceColor("#ba8d38") }
            }
            Rectangle { anchors.fill: parent; anchors.margins: 3; radius: 29; color: Theme.chassisDark; border.color: Theme.surfaceColor("#ffffdc") }
            Image { id: badge; anchors.fill: parent; anchors.margins: 10; source: root.displayedAward.icon || ""; sourceSize: Qt.size(128,128); fillMode: Image.PreserveAspectFit; asynchronous: true }
            Text { anchors.centerIn: parent; visible: badge.status !== Image.Ready; text: "★"; color: Theme.yellow; font.pixelSize: 34 }
        }
        Text {
            x: 84; y: 12; width: parent.width-x-19; height: 18
            text: "Achievement unlocked"; color: Theme.textColor("#fbe7ad"); font.pixelSize: 12; font.family: Theme.displayFamily
        }
        Text {
            x: 84; y: 31; width: parent.width-x-19; height: 24
            text: root.displayedAward.title || ""; textFormat: Text.PlainText; elide: Text.ElideRight
            color: Theme.accentInk; font.pixelSize: 18; font.family: Theme.displayFamily
            fontSizeMode: Text.HorizontalFit; minimumPixelSize: 14
        }
        Text {
            x: 85; y: 54; width: parent.width-x-20; height: 14
            text: (root.displayedAward.points >= 0 ? root.displayedAward.points + " points  ·  " : "") + (root.displayedAward.mode || "")
            color: Qt.alpha(Theme.accentInk,.76); font.pixelSize: 10
        }
    }
}
