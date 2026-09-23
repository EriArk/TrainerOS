import QtQuick

Item {
    id: root

    required property var entries
    required property var entry
    required property int selectionIndex
    required property bool takesFocus
    property bool wheelFocused: true
    property bool showBack: false
    property bool platformBadges: false
    property bool idsInNames: false
    property string itemPrefix: "multiverse-game-"
    property string emptyName: "multiverse-empty"
    property string filterText: ""
    property string actionLabel: "Choose for Home"
    property bool actionVisible: true
    property string emptyTitle: "No matching titles"
    property string emptyDetail: "A · Reset search and filter"
    signal activated(int index)
    signal emptyActivated()
    signal backActivated()
    readonly property real split: Math.round(width * 0.38)
    Rectangle {
        width: root.split; height: parent.height
        gradient: Gradient {
            GradientStop { position: 0; color: "#c9d9d4" }
            GradientStop { position: 0.5; color: "#eef0da" }
            GradientStop { position: 1; color: "#b5cec8" }
        }
    }
    Rectangle { x: root.split; width: 2; height: parent.height; color: "#607e807a" }
    Rectangle { x: root.split + 2; width: 6; height: parent.height; opacity: 0.3
        gradient: Gradient { orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#547870" }
            GradientStop { position: 1; color: "transparent" }
        }
    }
    Text {
        x: 18; y: 12; width: root.split - 36
        text: root.filterText
        color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight; textFormat: Text.PlainText
    }
    // The wheel sits inside a continuous recessed rail. Only nearby
    // delegates exist; logos decode asynchronously at display size.
    Item {
        id: rail
        x: 0; y: 36; width: root.split; height: parent.height - 78; clip: true
        Rectangle {
            x: -8; y: parent.height / 2 - 43; width: parent.width - 6; height: 86; radius: 14
            color: "#30ffd961"; border.color: "#b3ce992c"; border.width: 2
            Rectangle { anchors.fill: parent; anchors.margins: 3; radius: 12; color: "transparent"; border.color: "#b8fff3bc"; border.width: 2 }
            Rectangle { x: 8; y: 4; width: 5; height: parent.height - 8; radius: 2; color: Theme.focus }
        }
        PathView {
            id: wheel
            enabled: root.takesFocus
            focus: root.takesFocus && root.wheelFocused
            anchors.fill: parent; model: root.entries
            currentIndex: root.selectionIndex
            interactive: false; pathItemCount: 5; cacheItemCount: 2
            preferredHighlightBegin: 0.5; preferredHighlightEnd: 0.5
            highlightRangeMode: PathView.StrictlyEnforceRange
            highlightMoveDuration: Theme.motion(130)
            function alignModel() {
                if (count > 0) {
                    currentIndex = root.selectionIndex
                    positionViewAtIndex(root.selectionIndex, PathView.SnapPosition)
                }
                reveal()
            }
            function reveal() {
                if (root.visible && root.takesFocus && root.wheelFocused && currentItem) currentItem.forceActiveFocus(Qt.OtherFocusReason)
            }
            onCurrentIndexChanged: Qt.callLater(reveal)
            onCurrentItemChanged: Qt.callLater(reveal)
            onModelChanged: Qt.callLater(alignModel)
            Component.onCompleted: Qt.callLater(alignModel)
            Connections { target: root; function onTakesFocusChanged() { Qt.callLater(wheel.reveal) }
                function onSelectionIndexChanged() { wheel.currentIndex = root.selectionIndex; Qt.callLater(wheel.reveal) }
                function onWheelFocusedChanged() { Qt.callLater(wheel.reveal) } }
            Connections { target: root; function onVisibleChanged() { if (root.visible) Qt.callLater(wheel.alignModel) } }
            path: Path {
                startX: wheel.width * 0.37; startY: 26
                PathAttribute { name: "logoScale"; value: 0.57 }
                PathAttribute { name: "logoOpacity"; value: 0.28 }
                PathQuad { x: wheel.width * 0.51; y: wheel.height / 2; controlX: wheel.width * 0.51; controlY: wheel.height * 0.16 }
                PathPercent { value: 0.5 }
                PathAttribute { name: "logoScale"; value: 1.0 }
                PathAttribute { name: "logoOpacity"; value: 1.0 }
                PathQuad { x: wheel.width * 0.37; y: wheel.height - 26; controlX: wheel.width * 0.51; controlY: wheel.height * 0.84 }
                PathAttribute { name: "logoScale"; value: 0.57 }
                PathAttribute { name: "logoOpacity"; value: 0.28 }
            }
            delegate: Item {
                required property int index
                required property var modelData
                objectName: root.itemPrefix + (root.idsInNames ? modelData.id : index)
                readonly property bool selected: root.takesFocus && root.wheelFocused && index === root.selectionIndex
                onSelectedChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
                onVisibleChanged: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
                Component.onCompleted: if (selected && visible) forceActiveFocus(Qt.OtherFocusReason)
                width: rail.width - 42; height: index === root.selectionIndex ? 74 : 40
                Behavior on height { NumberAnimation { duration: Theme.motion(130) } }
                scale: PathView.logoScale === undefined ? 1 : PathView.logoScale
                opacity: (PathView.logoOpacity === undefined ? 1 : PathView.logoOpacity) * (modelData.missing ? 0.55 : 1)
                Accessible.role: Accessible.ListItem; Accessible.name: modelData.title
                Image {
                    id: logo; anchors.fill: parent; anchors.margins: 3; anchors.rightMargin: root.platformBadges ? 82 : 3
                    source: root.visible ? (modelData.logo || "") : ""
                    asynchronous: true; sourceSize.width: 560; sourceSize.height: 148
                    fillMode: Image.PreserveAspectFit; mipmap: true
                }
                Text {
                    anchors.fill: parent; anchors.margins: 3; anchors.rightMargin: root.platformBadges ? 82 : 3
                    visible: logo.status !== Image.Ready
                    text: modelData.title; textFormat: Text.PlainText
                    color: Theme.ink; style: Text.Outline; styleColor: "#f8f1d5"
                    font.family: Theme.displayFamily; font.pixelSize: 23; font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                }
                PlatformBadge { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    visible: root.platformBadges; label: modelData.platform || ""; shape: modelData.platformShape || "console" }
                MouseArea { anchors.fill: parent; onClicked: root.activated(index) }
            }
        }
    }
    Text {
        x: 18; anchors.bottom: parent.bottom; anchors.bottomMargin: 14
        visible: !root.showBack
        text: root.entries.length ? (root.selectionIndex + 1) + " / " + root.entries.length : ""
        color: Theme.muted; font.pixelSize: 13
    }
    Item {
        id: info
        x: root.split + 20; y: 12; width: parent.width - x - 22; height: parent.height - 56
        visible: root.entries.length > 0
        Text {
            id: title; width: parent.width; height: 47
            text: root.entry.title || ""; color: Theme.ink; textFormat: Text.PlainText
            font.family: Theme.displayFamily; font.pixelSize: 23; font.bold: true
            wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
        }
        Rectangle {
            id: picture; x: 0; y: 54; width: parent.width * 0.54; height: Math.min(150, parent.height * 0.5)
            radius: 5; color: "#142e32"; border.color: "#526a66"; border.width: 2
            Image {
                id: screenshot; anchors.fill: parent; anchors.margins: 4
                source: root.visible ? (root.entry.screenshot || "") : ""
                asynchronous: true; sourceSize.width: 720; sourceSize.height: 420; fillMode: Image.PreserveAspectFit
            }
            SystemGlyph { anchors.centerIn: parent; width: 66; height: 70; shape: "handheld"; visible: screenshot.status !== Image.Ready; opacity: 0.45 }
        }
        Column {
            id: facts
            x: picture.width + 16; y: picture.y; width: parent.width - x; spacing: 7
            Text { width: parent.width; text: root.entry.system || ""; color: Theme.ink; font.pixelSize: 15; font.bold: true; wrapMode: Text.WordWrap; textFormat: Text.PlainText }
            Text { text: root.entry.year || ""; visible: text.length > 0; color: Theme.muted; font.pixelSize: 15 }
            Text { width: parent.width; text: root.entry.genre || ""; visible: text.length > 0; color: Theme.muted; font.pixelSize: 14; maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight; textFormat: Text.PlainText }
            Text { width: parent.width; text: root.entry.developer || root.entry.publisher || ""; visible: text.length > 0; color: Theme.muted; font.pixelSize: 13; maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight; textFormat: Text.PlainText }
            Text { text: root.entry.players ? root.entry.players + (root.entry.players === "1" ? " player" : " players") : ""; visible: text.length > 0; color: Theme.muted; font.pixelSize: 13; textFormat: Text.PlainText }
            Text { width: parent.width; text: root.entry.playable ? "" : root.entry.status || ""; visible: text.length > 0; color: Theme.muted; font.pixelSize: 13; wrapMode: Text.WordWrap }
        }
        Text {
            objectName: "multiverse-description"
            y: picture.y + Math.max(picture.height, facts.height) + 14; width: parent.width; height: Math.max(0, parent.height - y)
            text: root.entry.description || ""; textFormat: Text.PlainText
            color: Theme.ink; font.pixelSize: 14; wrapMode: Text.WordWrap
            maximumLineCount: Math.max(1, Math.floor(height / 17)); elide: Text.ElideRight
        }
    }
    CapButton {
        objectName: root.emptyName
        x: root.split + 24; y: 64; width: parent.width - x - 24; height: 95
        visible: root.entries.length === 0
        label: root.emptyTitle
        detail: root.emptyDetail
        selected: visible && root.visible && root.takesFocus
        onActivated: root.emptyActivated()
    }
    CapButton {
        objectName: "world-list-back"
        x: 18; anchors.bottom: parent.bottom; anchors.bottomMargin: 8; width: root.split - 36; height: 32
        visible: root.showBack && root.entries.length > 0
        label: "B  Back to Worlds"; textSize: 14; tint: Theme.blue
        selected: visible && root.takesFocus && !root.wheelFocused
        onActivated: root.backActivated()
    }
    Row { x: root.split + 20; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; spacing: 16
        Hint { button: "A"; label: root.actionLabel; labelColor: Theme.ink; visible: root.actionVisible }
        Hint { button: "X"; label: "Search"; labelColor: Theme.ink }
        Hint { button: "Y"; label: "Filter"; labelColor: Theme.ink }
    }
}
