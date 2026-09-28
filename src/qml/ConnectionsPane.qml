import QtQuick
Item {
    id: root
    required property var shell
    property bool onboarding: false
    readonly property var network: shell.network
    readonly property bool focused: (onboarding || shell.settings.controlsFocused) && !shell.menuOpen && !shell.keyboard.open && !shell.notice.length
    function enter() { if(!onboarding && !shell.settings.controlsFocused) shell.settings.selectCategory(10,true) }
    Row {
        x: 0; y: 10; width: parent.width; spacing: 8
        Repeater { model: ["Wi-Fi", "Bluetooth"]
            CapButton {
                required property int index; required property string modelData
                width: (root.width-140)/2; height: 43; label: modelData; textSize: 18
                tint: root.network.bluetooth===(index===1) ? Theme.yellow : Theme.blue
                onActivated: { root.enter();root.network.selectFace(index===1) }
            }
        }
        CapButton {
            width: 124; height: 43; textSize: 15
            readonly property var radio: root.shell.device.radios[root.network.bluetooth?1:0]
            label: radio.detail; tint: radio.level===1 ? Theme.green : Theme.pink
            onActivated: { root.enter();root.network.toggleRadio() }
        }
    }
    // One list and one inline question; no stack of device/detail/action menus.
    ListView {
        id: list; objectName: "connection-list"
        x: 0; y: 68; width: parent.width; height: parent.height-y-49
        clip: true; spacing: 7; model: root.network.rows
        visible: !root.network.prompt.length
        currentIndex: root.network.focusIndex
        onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
        onCountChanged: positionViewAtIndex(currentIndex,ListView.Contain)
        delegate: CapButton {
            required property int index; required property var modelData
            objectName: "connection-"+index
            width: list.width-5; height: 57; textSize: 18
            label: modelData.title; detail: modelData.detail
            tint: modelData.connected ? Theme.green : modelData.saved ? Theme.blue : "#e4eddf"
            selected: root.focused && root.network.focusIndex===index
            onActivated: { root.enter();root.network.activate(index) }
        }
        Text {
            x: 18; y: 28; width: parent.width-36; wrapMode: Text.WordWrap
            visible: list.count===0 && !root.network.busy
            text: root.network.bluetooth ? "No devices found. Turn on pairing mode and search nearby." : "No networks found. Check Wi-Fi and search again."
            color: Theme.muted; font.pixelSize: 18
        }
    }
    Rectangle {
        x: 0; y: 68; width: parent.width; height: parent.height-y-49
        visible: root.network.prompt.length>0; radius: 12; color: "#edf2e7"; border.color: "#bdcfbd"
        Text {
            x: 22; y: 24; width: parent.width-44; height: parent.height-110
            text: root.network.prompt; textFormat: Text.PlainText; wrapMode: Text.WordWrap
            font.pixelSize: 23; color: Theme.ink
        }
        CapButton {
            x: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: 20
            width: parent.width-44; height: 43; label: root.network.confirmLabel
            visible: root.network.confirmLabel!=="Wait"
            tint: Theme.yellow; selected: root.focused
            onActivated: root.network.activate(0)
        }
    }
    Text {
        x: 4; anchors.bottom: parent.bottom; anchors.bottomMargin: 8
        width: parent.width-8; height: 34; wrapMode: Text.WordWrap; elide: Text.ElideRight
        text: root.network.status || root.shell.device.error; textFormat: Text.PlainText
        color: Theme.muted; font.pixelSize: 14
    }
}
