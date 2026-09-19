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
    // The shell is hidden during gameplay. A transient child of that window
    // would also stay hidden instead of taking focus above the live game.
    transientParent: null
    visible: presentation.visible
    onVisibleChanged: if (visible) requestActivate()
    onActiveChanged: presentation.setWindowFocused(active)
    onClosing: function(event) {
        event.accepted = false
        presentation.cancel()
    }
    Item {
        x: (window.width - width) / 2; y: (window.height - height) / 2
        width: Theme.viewportWidth; height: Theme.viewportHeight
        scale: Math.min(window.width / width, window.height / height)
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.chassisTop }
                GradientStop { position: 1; color: Theme.chassisDark }
            }
        }
        Text {
            x: 35; y: 25; text: "TRAINER OS"
            color: Theme.paper; font.family: Theme.brandFamily; font.pixelSize: 25
        }
        Panel {
            x: 24; y: 76; width: 912; height: 440
            Text {
                objectName: "exit-heading"
                x: 34; y: 35; width: 840
                text: !window.presentation.confirming ? "Returning to TrainerOS…"
                    : window.presentation.autosave ? "Close this game?" : "Have you saved your game?"
                color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 37
                wrapMode: Text.WordWrap
            }
            Text {
                x: 36; y: 109; width: 820
                text: window.presentation.confirming
                    ? window.presentation.autosave
                        ? "Wait for any saving indicator to finish.\nYou can also go back and keep playing."
                        : "Save inside your game before leaving.\nNot ready? Go back and keep playing."
                    : window.presentation.slowClose
                        ? "Your game is taking longer to close.\nTrainerOS is waiting; it has not forced the game to stop."
                        : "Waiting for your game to close normally."
                color: Theme.ink; font.pixelSize: 22; lineHeight: 1.3; wrapMode: Text.WordWrap
            }
            Text {
                objectName: "exit-capture-warning"
                x: 36; y: 207; width: 820
                visible: window.presentation.confirming && window.presentation.captureFailed
                text: "No new picture was captured. You can still go back or exit."
                color: Theme.muted; font.pixelSize: 18; wrapMode: Text.WordWrap
            }
            MountedPanel {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 9 }
                height: 147; color: Theme.chassis
                Text {
                    x: 27; y: 15; width: 820
                    text: !window.presentation.confirming ? "Your Adventure is still running until it closes."
                        : window.presentation.ready ? "Choose with A or B — no selection needed."
                        : "Release the controls to choose."
                    color: Theme.paper; font.pixelSize: 17
                }
                Row {
                    x: 27; y: 56; spacing: 22
                    visible: window.presentation.confirming
                    CapButton {
                        objectName: "exit-back"
                        width: 364; height: 60; tint: Theme.blue; textSize: 23
                        label: window.presentation.autosave ? "B   Keep playing" : "B   Not yet — back to game"
                        enabled: window.presentation.ready
                        onActivated: window.presentation.cancel()
                    }
                    CapButton {
                        objectName: "exit-confirm"
                        width: 448; height: 60; tint: Theme.yellow; textSize: 23
                        label: window.presentation.autosave ? "A   Close game" : "A   I've saved — exit Adventure"
                        enabled: window.presentation.ready
                        onActivated: window.presentation.confirm()
                    }
                }
            }
        }
    }
}
