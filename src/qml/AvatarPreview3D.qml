import QtQuick
import QtQuick3D
import QtQuick3D.AssetUtils

View3D {
    id: root
    objectName: "avatar-preview"
    readonly property bool ready: avatar.status===RuntimeLoader.Success
    property url modelSource
    property real yaw: 0
    readonly property real displayScale: Math.max(1, Window.window ? Window.window.uiScale || 1 : 1)
    Component.onCompleted: {
        // The shell scales its logical canvas. Render at that physical size too;
        // older Qt versions keep their supported default texture allocation.
        if ("explicitTextureWidth" in root) {
            root.explicitTextureWidth = Qt.binding(function() { return Math.ceil(root.width * root.displayScale) })
            root.explicitTextureHeight = Qt.binding(function() { return Math.ceil(root.height * root.displayScale) })
        }
    }
    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Transparent
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
    }
    camera: PerspectiveCamera { position: Qt.vector3d(0, 0.55, 2.25); clipNear: 0.01; clipFar: 20; fieldOfView: 31 }
    DirectionalLight { eulerRotation: Qt.vector3d(-25, -25, 0); brightness: 1.4; ambientColor: "#888888" }
    DirectionalLight { eulerRotation: Qt.vector3d(-10, 140, 0); brightness: 0.7 }
    RuntimeLoader { id: avatar; source: root.modelSource; eulerRotation.y: root.yaw }
    Text { anchors.centerIn: parent; width: parent.width; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; visible: avatar.status===RuntimeLoader.Error; text: "Preview unavailable"; color: "#853b24" }
    MouseArea {
        anchors.fill: parent
        property real previousX
        onPressed: mouse => previousX=mouse.x
        onPositionChanged: mouse => { if(pressed){root.yaw+=(mouse.x-previousX)*0.7;previousX=mouse.x;} }
    }
}
