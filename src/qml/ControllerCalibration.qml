import QtQuick

Item {
    id: root
    required property var input
    readonly property var values: visible && input ? input.live : ({})
    readonly property bool calibrating: ["center","range"].includes(input?.captureMode || "")
    Text { x: 8; y: 3; text: "Sticks & triggers"; color: Theme.ink; font.pixelSize: 18; font.weight: Font.DemiBold }
    Row { x: 8; y: 38; spacing: 24
        Repeater { model: 2
            Item { required property int index; width: 130; height: 160
                Rectangle { width: 120; height: 120; radius: 60; color: "transparent"; border.color: Theme.divider
                    Rectangle { anchors.centerIn: parent; width: 120*(root.input?.profile.deadzone || 20)/100; height: width; radius: width/2; color: Theme.selection }
                    Rectangle { x: 59; width: 1; height: 120; color: Theme.divider }
                    Rectangle { y: 59; height: 1; width: 120; color: Theme.divider }
                    Rectangle { x: 56+(root.values.raw?.[index*2] || 0)*55; y: 56+(root.values.raw?.[index*2+1] || 0)*55; width: 8; height: 8; radius: 4; color: Theme.muted }
                    Rectangle { x: 55+(root.values.axes?.[index*2] || 0)*55; y: 55+(root.values.axes?.[index*2+1] || 0)*55; width: 10; height: 10; radius: 5; color: "transparent"; border.width: 2; border.color: Theme.accent }
                }
                Text { y: 128; text: index===0?"Left stick":"Right stick"; color: Theme.ink; font.pixelSize: 12 }
                Text { y: 146; text: Math.round((root.values.raw?.[index*2] || 0)*100)+" / "+Math.round((root.values.raw?.[index*2+1] || 0)*100)+"%"; color: Theme.muted; font.pixelSize: 10 }
            }
        }
        Column { spacing: 14; y: 12
            Repeater { model: 2
                Column { required property int index; spacing: 8
                    Text { text: Theme.buttonLabel(index===0?"L2":"R2")+"  "+Math.round((root.values.axes?.[4+index] || 0)*100)+"%"; color: Theme.ink; font.pixelSize: 12 }
                    Rectangle { width: 100; height: 4; color: Theme.divider; Rectangle { width: parent.width*(root.values.axes?.[4+index] || 0); height: 4; color: Theme.accent } }
                }
            }
            Text { width: 145; text: (root.values.buttons || []).join(" · "); color: Theme.muted; font.pixelSize: 11; wrapMode: Text.Wrap }
        }
    }
    Text { x: 8; y: 211; width: parent.width-16; text: root.input?.captureStatus || "Dot: measured input · Outline: calibrated input"; color: Theme.muted; wrapMode: Text.WordWrap; font.pixelSize: 12 }
    Row { x: 8; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; spacing: 26
        Text { text: root.calibrating?(root.input.captureMode==="center"?"Start · Measure range":"Start · Save calibration"):"Calibrate"; color: Theme.accent; font.pixelSize: 13; TapHandler { onTapped: root.calibrating?root.input.advanceCalibration():root.input.beginCapture("center") } }
        Text { text: "Cancel"; visible: root.calibrating; color: Theme.muted; font.pixelSize: 13; TapHandler { onTapped: root.input.cancelCapture() } }
    }
}
