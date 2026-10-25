import QtQuick
Item {
    id: root
    property bool flat: false
    required property var shell
    property bool onboarding: false
    property bool quick: false
    readonly property var network: shell.network
    readonly property bool focused: (quick || onboarding || shell.settings.controlsFocused) && (quick || !shell.menuOpen) && !shell.keyboard.open && !shell.notice.length
    function enter() { if(!quick && !onboarding && !shell.settings.controlsFocused) shell.settings.selectCategory(10,true) }
    Row {
        x: 0; y: 10; width: parent.width; spacing: 8
        Text { visible: root.quick; width: root.quick?root.width-132:0; height: 38; verticalAlignment: Text.AlignVCenter; text: "Available networks"; color: Theme.muted; font.pixelSize: 14 }
        Repeater { model: root.quick?[]:["Wi-Fi", "Bluetooth"]
            CapButton { flat: root.flat;
                required property int index; required property string modelData
                width: (root.width-140)/2; height: 43; label: modelData; textSize: 18
                tint: root.network.bluetooth===(index===1) ? Theme.yellow : Theme.blue
                onActivated: { root.enter();root.network.selectFace(index===1) }
            }
        }
        CapButton { flat: root.flat;
            width: 124; height: root.quick?32:43; textSize: 15
            readonly property var radio: root.shell.device.radios[!root.quick && root.network.bluetooth?1:0]
            label: radio.detail; tint: radio.level===1 ? Theme.green : Theme.pink
            enabled: radio.level>=0 && !root.network.busy
            onActivated: { root.enter();root.network.toggleRadio() }
        }
    }
    // One list and one inline question; no stack of device/detail/action menus.
    ListView {
        id: list; objectName: "connection-list"
        x: 0; y: root.quick?52:68; width: parent.width; height: parent.height-y-(root.quick?62:49)
        clip: true; spacing: 7; model: root.quick && root.network.bluetooth ? [] : root.network.rows
        visible: !root.network.prompt.length
        currentIndex: root.network.focusIndex
        onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
        onCountChanged: positionViewAtIndex(currentIndex,ListView.Contain)
        delegate: Item {
            id: entry
            required property int index; required property var modelData
            objectName: "connection-"+index
            width: list.width-5; height: root.quick?48:57
            enabled: !root.network.busy
            ActivityRow { anchors.fill: parent; visible: root.quick
                label: entry.modelData.title; detail: entry.modelData.detail; glyph: "wifi"
                accent: entry.modelData.connected?Theme.green:entry.modelData.saved?Theme.blue:Theme.paper
                selected: root.focused && root.network.focusIndex===entry.index
                onActivated: {root.enter();root.network.activate(entry.index)}
            }
            CapButton { flat: root.flat; anchors.fill: parent; visible: !root.quick; textSize: 18
                label: entry.modelData.title; detail: entry.modelData.detail
                tint: entry.modelData.connected?Theme.green:entry.modelData.saved?Theme.blue:"#e4eddf"
                selected: root.focused && root.network.focusIndex===entry.index
                onActivated: {root.enter();root.network.activate(entry.index)}
            }
        }
        Text {
            x: 18; y: 28; width: parent.width-36; wrapMode: Text.WordWrap
            visible: list.count===0 && !root.network.busy
            text: root.network.bluetooth ? "No devices found. Turn on pairing mode and search nearby." : "No networks found. Check Wi-Fi and search again."
            color: Theme.muted; font.pixelSize: 18
        }
    }
    Rectangle {
        x: 0; y: root.quick?52:68; width: parent.width; height: parent.height-y-(root.quick?62:49)
        visible: root.network.prompt.length>0; radius: 12; color: Theme.surfaceColor("#edf2e7"); border.color: Theme.surfaceColor("#bdcfbd")
        Text {
            x: 22; y: root.quick?12:24; width: parent.width-44; height: parent.height-(root.quick?70:110)
            text: root.network.prompt; textFormat: Text.PlainText; wrapMode: Text.WordWrap
            font.pixelSize: root.quick?18:23; color: Theme.ink
        }
        CapButton { flat: root.flat;
            x: 22; anchors.bottom: parent.bottom; anchors.bottomMargin: root.quick?12:20
            width: parent.width-44; height: root.quick?32:43; label: root.network.confirmLabel
            visible: root.network.confirmLabel!=="Wait"
            tint: Theme.yellow; selected: root.focused
            onActivated: root.network.activate(0)
        }
    }
    Text {
        x: 4; anchors.bottom: parent.bottom; anchors.bottomMargin: 8
        width: parent.width-(root.quick?112:8); height: root.quick?45:34; wrapMode: Text.WordWrap; elide: Text.ElideRight
        text: root.quick && root.network.bluetooth ? "Opening Wi-Fi…" : root.network.status || root.shell.device.error || (root.quick ? Theme.buttonLabel("A")+" Select · "+Theme.buttonLabel("X")+" Radio · "+Theme.buttonLabel("Y")+" Search" : ""); textFormat: Text.PlainText
        color: Theme.muted; font.pixelSize: 14
    }
    CapButton { flat: root.flat; visible: root.quick; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.bottomMargin: 12
        width: 100; height: 30; textSize: 13; label: "Search"; centered: true; tint: Theme.paper
        enabled: !root.network.busy && !root.network.prompt.length
        onActivated: root.network.search()
    }
}
