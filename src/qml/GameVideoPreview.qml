import QtQuick
import QtMultimedia

// Created only after the wheel settles; destroying the loader releases the
// decoder on navigation, overlays, launch and loss of application focus.
Item {
    id: root
    required property url source
    property bool failed: false
    readonly property bool playing: !failed && player.hasVideo && player.playbackState === MediaPlayer.PlayingState
    MediaPlayer {
        id: player
        objectName: "game-preview-player"
        source: root.source
        loops: MediaPlayer.Infinite
        videoOutput: output
        audioOutput: AudioOutput { muted: true }
        onErrorOccurred: root.failed = true
        Component.onCompleted: play()
        Component.onDestruction: stop()
    }
    VideoOutput {
        id: output
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        visible: root.playing
    }
}
