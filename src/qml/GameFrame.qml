import QtQuick
import QtQuick.Effects

Item {
    id: root
    required property var presentation
    property real maximumWidth: 280
    property real maximumHeight: 158
    readonly property real ratio: picture.implicitHeight > 0 ? picture.implicitWidth / picture.implicitHeight : 16/9
    width: Math.min(maximumWidth, (maximumHeight-16)*ratio+16)
    height: Math.min(maximumHeight, (width-16)/ratio+16)
    Rectangle { x: -2; y: 5; width: parent.width+4; height: parent.height; radius: 22; color: "#22342722" }
    Rectangle { anchors.fill: parent; radius: 20
        gradient: Gradient { GradientStop { position: 0; color: Qt.lighter(Theme.rim,1.4) } GradientStop { position: .5; color: Theme.chassis } GradientStop { position: 1; color: Theme.chassisDark } }
        border.color: Theme.edgeShadow
        Rectangle { anchors.fill: parent; anchors.margins: 2; radius: 18; color: "transparent"; border.color: "#96fff8d9" }
    }
    Image { id: picture; x: 8; y: 8; width: parent.width-16; height: parent.height-16; visible: false
        source: root.presentation.hasFrame ? "image://exit-frame/"+root.presentation.frameKey : ""; fillMode: Image.PreserveAspectFit; cache: false }
    Rectangle { id: mask; x: 8; y: 8; width: parent.width-16; height: parent.height-16; radius: 13; color: "white"; visible: false; layer.enabled: true }
    MultiEffect { x: 8; y: 8; width: parent.width-16; height: parent.height-16; source: picture; maskEnabled: true; maskSource: mask }
    MenuGlyph { anchors.centerIn: parent; width: 40; height: 40; glyph: "play"; ink: Theme.paper; visible: !root.presentation.hasFrame }
}
