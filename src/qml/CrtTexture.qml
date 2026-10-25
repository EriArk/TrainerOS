import QtQuick

// Static background only: no shader, animation, input or effect on foreground text.
Image {
    visible: Theme.dark
    source: "qrc:/textures/crt.svg"
    fillMode: Image.Tile
    smooth: false
    opacity: 0.55
}
