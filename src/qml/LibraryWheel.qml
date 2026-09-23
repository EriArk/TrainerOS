import QtQuick

Item {
    id: root

    required property var entries
    required property var entry
    required property int selectionIndex
    required property bool takesFocus
    property bool wheelFocused: true
    property bool showBack: false
    property bool idsInNames: false
    property string itemPrefix: "multiverse-game-"
    property string emptyName: "multiverse-empty"
    property string filterText: ""
    property string actionLabel: "Choose for Home"
    property bool actionVisible: true
    property string emptyTitle: "No matching titles"
    property string emptyDetail: "Reset search and filter"
    signal activated(int index)
    signal emptyActivated()
    signal backActivated()
    readonly property real split: Math.round(width * 0.38)
    function titleColor(key) {
        const colors = ["#963c55", "#35699c", "#67429b", "#a14d25", "#386c40", "#965522"]
        let hash = 0
        for (let i = 0; i < key.length; ++i) hash = (hash * 31 + key.charCodeAt(i)) | 0
        return colors[(hash >>> 0) % colors.length]
    }
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
        Canvas {
            id: crystal
            objectName: "library-selection-crystal"
            x: 0; y: parent.height / 2 - 15; width: 27; height: 30; z: 2
            visible: root.entries.length > 0
            opacity: root.wheelFocused ? 1 : 0.45
            onPaint: {
                const c = getContext("2d"); c.reset()
                c.scale(width / 47, height / 54)
                function facet(points, color) {
                    c.beginPath(); c.moveTo(points[0][0], points[0][1])
                    for (let i = 1; i < points.length; ++i) c.lineTo(points[i][0], points[i][1])
                    c.closePath(); c.fillStyle = color; c.fill()
                }
                facet([[0,3],[43,28],[0,54]], "#653b273d")
                facet([[0,0],[41,25],[0,50]], "#88521e")
                facet([[0,2],[38,25],[8,23]], "#fff2af")
                facet([[0,2],[8,23],[0,47]], "#edac2e")
                facet([[8,23],[38,25],[0,47]], "#f1c142")
                facet([[8,23],[38,25],[1,28]], "#ffe679")
            }
            NumberAnimation { id: crystalSlide; target: crystal; property: "x"; from: -24; to: 0; duration: Theme.motion(180); easing.type: Easing.OutCubic }
            Connections { target: root
                function onSelectionIndexChanged() { crystalSlide.restart() }
                function onVisibleChanged() { if (root.visible) crystalSlide.restart() }
            }
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
                startX: wheel.width * 0.37; startY: 10
                PathAttribute { name: "logoScale"; value: 0.57 }
                PathAttribute { name: "logoOpacity"; value: 0.28 }
                PathQuad { x: wheel.width * 0.51; y: wheel.height / 2; controlX: wheel.width * 0.51; controlY: wheel.height * 0.16 }
                PathPercent { value: 0.5 }
                PathAttribute { name: "logoScale"; value: 1.0 }
                PathAttribute { name: "logoOpacity"; value: 1.0 }
                PathQuad { x: wheel.width * 0.37; y: wheel.height - 10; controlX: wheel.width * 0.51; controlY: wheel.height * 0.84 }
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
                width: rail.width - 44; height: index === root.selectionIndex ? 104 : 42
                Behavior on height { NumberAnimation { duration: Theme.motion(130) } }
                scale: PathView.logoScale === undefined ? 1 : PathView.logoScale
                opacity: (PathView.logoOpacity === undefined ? 1 : PathView.logoOpacity) * (modelData.missing ? 0.55 : 1)
                Accessible.role: Accessible.ListItem; Accessible.name: modelData.title
                Image {
                    id: logo; anchors.fill: parent; anchors.margins: 3
                    source: root.visible ? (modelData.logo || "") : ""
                    asynchronous: true; sourceSize.width: 640; sourceSize.height: 220
                    fillMode: Image.PreserveAspectFit; mipmap: true
                }
                Text {
                    x: fallback.x; y: fallback.y + 3; width: fallback.width; height: fallback.height
                    visible: fallback.visible
                    text: fallback.text; textFormat: Text.PlainText
                    color: "#463d39"; style: Text.Outline; styleColor: "#463d39"
                    font: fallback.font
                    fontSizeMode: Text.Fit; minimumPixelSize: 15
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                }
                Text {
                    id: fallback
                    anchors.fill: parent; anchors.margins: 3
                    visible: logo.status !== Image.Ready
                    text: modelData.title; textFormat: Text.PlainText
                    color: root.titleColor(modelData.id || modelData.title); style: Text.Outline; styleColor: "#fff6d9"
                    font.family: Theme.gameFamily; font.pixelSize: index === root.selectionIndex ? 31 : 23
                    fontSizeMode: Text.Fit; minimumPixelSize: 15
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                }
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
            id: title; width: parent.width - 84; height: 47
            text: root.entry.title || ""; color: Theme.ink; textFormat: Text.PlainText
            font.family: Theme.displayFamily; font.pixelSize: 23; font.bold: true
            wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
        }
        PlatformBadge {
            objectName: "library-detail-platform"
            anchors.right: parent.right; y: 0
            label: root.entry.platformShort || ""; shape: root.entry.platformShape || "console"
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
}
