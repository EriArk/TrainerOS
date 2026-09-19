import QtQuick
import QtQuick.Window

Window {
    id: window
    required property var presentation
    objectName: "adventure-exit-window"
    title: "TrainerOS — Return from Adventure"
    width: 960; height: 540
    color: Theme.chassisDark
    flags: Qt.Window | Qt.FramelessWindowHint
    transientParent: null
    visible: presentation.visible
    onVisibleChanged: if (visible) requestActivate()
    onActiveChanged: presentation.setWindowFocused(active)
    onClosing: function(event) {
        event.accepted = false
        presentation.cancel()
    }
    // Gamescope retains full-screen focus/input isolation. The captured game
    // makes the presentation a small overlay without compositor alpha tricks.
    Image {
        objectName: "exit-game-backdrop"
        anchors.fill: parent
        source: window.visible && !window.presentation.captureFailed
            ? "image://exit-frame/" + window.presentation.frameKey : ""
        fillMode: Image.PreserveAspectFit
        asynchronous: false
        cache: false
    }
    Rectangle { anchors.fill: parent; color: "#38090f14" }
    Item {
        x: (window.width - width) / 2; y: (window.height - height) / 2
        width: Theme.viewportWidth; height: Theme.viewportHeight
        scale: Math.min(window.width / width, window.height / height)
        Item {
            anchors.centerIn: parent
            width: 540; height: 236
            scale: window.visible ? 1 : .97
            Behavior on scale { NumberAnimation { duration: Theme.motion(130) } }
            Rectangle {
                x: -8; y: 9; width: parent.width + 16; height: parent.height + 7
                radius: 24; color: "#24000000"
            }
            Rectangle {
                x: -3; y: 6; width: parent.width + 6; height: parent.height + 2
                radius: 19; color: "#48000000"
            }
            Panel {
                objectName: "exit-card"
                anchors.fill: parent
                patterned: false
                Text {
                    objectName: "exit-heading"
                    x: 27; y: 28; width: parent.width - 54
                    text: !window.presentation.confirming ? "Leaving Adventure…"
                        : window.presentation.autosave ? "Leave this Adventure?" : "Saved your progress?"
                    color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 31
                    horizontalAlignment: Text.AlignHCenter
                }
                Text {
                    x: 27; y: 77; width: parent.width - 54
                    text: window.presentation.confirming
                        ? window.presentation.autosave ? "Wait for saving to finish." : "Save in-game before you leave."
                        : window.presentation.slowClose ? "Taking a little longer…" : "Just a moment…"
                    color: Theme.muted; font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                }
                Text {
                    x: 27; y: 108; width: parent.width - 54
                    text: window.presentation.confirming && !window.presentation.ready ? "Release the controls" : ""
                    color: Theme.muted; font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                }
                MountedPanel {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 9 }
                    height: 88; color: Theme.chassis
                    Row {
                        anchors.centerIn: parent
                        spacing: 14
                        visible: window.presentation.confirming
                        CapButton {
                            objectName: "exit-back"
                            width: 232; height: 50; tint: Theme.blue; textSize: 20; centered: true
                            label: "B   Keep playing"
                            enabled: window.presentation.ready
                            onActivated: window.presentation.cancel()
                        }
                        CapButton {
                            objectName: "exit-confirm"
                            width: 232; height: 50; tint: Theme.yellow; textSize: 20; centered: true
                            label: "A   Leave"
                            enabled: window.presentation.ready
                            onActivated: window.presentation.confirm()
                        }
                    }
                }
            }
        }
    }
}
