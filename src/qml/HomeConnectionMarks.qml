import QtQuick
import QtQuick.Controls
Row {
    id: root
    objectName: "home-connection-marks"
    property var modes: []
    spacing: 7
    height: 18
    Repeater {
        model: root.modes
        Item {
            id: mark
            required property var modelData
            readonly property string description: modelData.label+(modelData.nearby ? ", Nearby" : "")+(modelData.online ? ", Online" : "")
            width: 22; height: 18
            Image { anchors.fill: parent; source: "qrc:/home-icons/"+modelData.id+".svg"; opacity: .55; fillMode: Image.PreserveAspectFit }
            Accessible.role: Accessible.StaticText
            Accessible.name: description
            MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; onPressAndHold: mark.ToolTip.show(mark.description,2500) }
            ToolTip.visible: pointer.containsMouse
            ToolTip.delay: 650
            ToolTip.text: description
        }
    }
}
