import QtQuick
import QtQuick3D
import QtQuick3D.AssetUtils

View3D {
    id: root
    objectName: "avatar-preview"
    readonly property bool ready: avatar.status===RuntimeLoader.Success
    readonly property string loadError: avatar.status===RuntimeLoader.Error ? avatar.errorString : ""
    property url modelSource
    property real yaw: 0
    property var poseHost: null
    property bool closeup: false
    property real admiration: 0
    property real turn: 0
    property int posedJoints: 0
    function admire() { celebration.restart() }
    onModelSourceChanged: { celebration.stop(); admiration=0; turn=0 }
    onAdmirationChanged: if(poseHost)posedJoints=poseHost.posePreview(avatar,admiration)
    onVisibleChanged: if(!visible){celebration.stop();admiration=0;turn=0}
    SequentialAnimation {
        id:celebration
        ParallelAnimation {
            NumberAnimation { target:root;property:"admiration";to:1;duration:350;easing.type:Easing.InOutSine }
            NumberAnimation { target:root;property:"turn";to:-24;duration:350;easing.type:Easing.InOutSine }
        }
        NumberAnimation { target:root;property:"turn";to:24;duration:700;easing.type:Easing.InOutSine }
        ParallelAnimation {
            NumberAnimation { target:root;property:"admiration";to:0;duration:350;easing.type:Easing.InOutSine }
            NumberAnimation { target:root;property:"turn";to:0;duration:350;easing.type:Easing.InOutSine }
        }
    }
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
    camera: PerspectiveCamera { y:root.closeup?.84:.55;z:root.closeup?1.12:2.25;clipNear:0.01;clipFar:20;fieldOfView:31
        Behavior on y {NumberAnimation {duration:220}} Behavior on z {NumberAnimation {duration:220}}
    }
    DirectionalLight { eulerRotation: Qt.vector3d(-25, -25, 0); brightness: 1.4; ambientColor: "#888888" }
    DirectionalLight { eulerRotation: Qt.vector3d(-10, 140, 0); brightness: 0.7 }
    RuntimeLoader { id: avatar; objectName:"wardrobe-preview-rig";source: root.modelSource; eulerRotation.y: root.yaw+root.turn }
    Text { anchors.centerIn: parent; width: parent.width; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; visible: avatar.status===RuntimeLoader.Error; text: "Preview unavailable"; color: Theme.textColor("#853b24") }
    MouseArea {
        anchors.fill: parent
        property real previousX
        onPressed: mouse => {celebration.stop();root.admiration=0;root.turn=0;previousX=mouse.x}
        onPositionChanged: mouse => { if(pressed){root.yaw+=(mouse.x-previousX)*0.7;previousX=mouse.x;} }
    }
}
