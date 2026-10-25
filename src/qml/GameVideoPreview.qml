import QtQuick
import QtMultimedia
import QtQuick.Effects

// Created only after the wheel settles; destroying the loader releases the
// decoder on navigation, overlays, launch and loss of application focus.
Item {
    id: root
    required property url source
    property bool failed: false
    property bool sound: false
    readonly property bool playing: !failed && player.hasVideo && player.playbackState === MediaPlayer.PlayingState
    MediaPlayer {
        id: player
        objectName: "game-preview-player"
        source: root.source
        loops: MediaPlayer.Infinite
        videoOutput: output
        audioOutput: AudioOutput { muted: !root.sound }
        onErrorOccurred: root.failed = true
        Component.onCompleted: play()
        Component.onDestruction: stop()
    }
    VideoOutput {
        id: output
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        visible: false
    }
    Rectangle { id: mask; anchors.fill: parent; radius: 5; color: "white"; visible: false; layer.enabled: true }
    MultiEffect { anchors.fill: parent; source: output; maskEnabled: true; maskSource: mask; visible: root.playing }
}
