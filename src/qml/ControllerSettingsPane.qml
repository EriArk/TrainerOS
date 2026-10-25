import QtQuick

Item {
    id: root
    required property var settings
    required property bool takesFocus
    readonly property var input: Theme.controller
    property bool calibrationOpen: false
    onVisibleChanged: if(!visible){calibrationOpen=false;input?.cancelCapture()}
    readonly property string family: input?.family || "generic"
    Item { anchors.fill: parent; visible: !root.calibrationOpen
    Text { x: 10; y: 10; width: parent.width-20; text: input?.deviceName || "Connect a controller"; color: Theme.textColor("#365346"); font.pixelSize: 18; font.weight: Font.DemiBold; elide: Text.ElideRight }
    Item { x: 3; y: 43; width: parent.width*.46; height: parent.height-68
        ControllerDiagram { objectName: "settings-controller-diagram"; x: 0; y: 14; width: parent.width; height: width*.72; interactive: false }
        Text { x: 4; y: parent.width*.72+12; width: parent.width-8; text: {const p=root.input?.profile;return Theme.buttonLabel("A")+"  Select     "+Theme.buttonLabel("B")+"  Back"} color: Theme.textColor("#879781"); font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
        Text { objectName: "settings-control-0"; readonly property bool selected: root.takesFocus&&settings.rowFocus===0; onSelectedChanged: if(selected)forceActiveFocus(); x: 6; anchors.bottom: parent.bottom; anchors.bottomMargin: 3; text: "Calibrate sticks & triggers →"; color: root.takesFocus&&settings.rowFocus===0?Theme.textColor("#3d7553"):Theme.textColor("#8b9c85"); font.pixelSize: 12; font.weight: root.takesFocus&&settings.rowFocus===0?Font.DemiBold:Font.Normal; TapHandler { onTapped: {root.calibrationOpen=true;root.input.beginCapture("center")} } }
    }
    Column { x: parent.width*.47; y: 39; width: parent.width-x-10; spacing: 0
        Repeater { model: 5
            SettingsRow { objectName: "settings-control-"+(index+1); required property int index; width: parent.width; height: 40; compact: true; title: settings.controls[index+1]?.title || ""; detail: settings.controls[index+1]?.detail || ""; kind: index>=3&&root.input?.connected?"toggle":settings.controls[index+1]?.kind || "status"; checked: index===3?root.input?.profile.invertX || false:root.input?.profile.invertY || false; selected: root.takesFocus&&settings.rowFocus===index+1
                onActivated: settings.activateRow(index+1)
                onAdjustmentRequested: direction => settings.adjustDisplay(index+1,direction)
            }
        }
        Text { x: 13; height: 28; verticalAlignment: Text.AlignVCenter; text: "Reset controller settings"; color: root.takesFocus&&settings.rowFocus===6?Theme.textColor("#3e7351"):Theme.textColor("#8e9786"); font.pixelSize: 11; TapHandler { onTapped: settings.activateRow(6) } }
    }
    Text { x: 12; anchors.bottom: parent.bottom; anchors.bottomMargin: 8; width: parent.width-24; text: input?.profileError || "Saved for this device · Touch and keyboard remain available"; color: Theme.textColor("#8e9987"); font.pixelSize: 10; elide: Text.ElideRight }
    }
    ControllerCalibration { anchors.fill: parent; input: root.input; visible: root.calibrationOpen }
    Text { anchors.right: parent.right; y: 5; visible: root.calibrationOpen; text: "Back"; color: Theme.muted; font.pixelSize: 12; TapHandler { onTapped: {root.input.cancelCapture();root.calibrationOpen=false} } }
    Connections { target: root.input; function onCaptureChanged(){root.calibrationOpen=["center","range"].includes(root.input.captureMode)} }
}
