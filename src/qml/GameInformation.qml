import QtQuick

// Shared selected-game vitrine; selection and launch belong to the browser.
Item {
    id: root
    required property var entry
    property bool active: false
    property bool previewsEnabled: false
    property bool previewSound: false
    property int previewDelay: 1800
    property bool favoritesAvailable: false
    signal favoriteRequested(string gameId)
    readonly property var artwork: entry.artwork || ({})
    readonly property var playInfo: entry.playInfo || ({})
    readonly property string gameId: entry.id || ""
    readonly property var modes: typeof runtimeMultiplayer !== "undefined" ? runtimeMultiplayer.gameModes[gameId] || [] : []
    readonly property var facts: artwork.displayFacts === false ? [] : [
        {label: "Players", value: playInfo.label || ""},
        {label: "Year", value: entry.year || ""},
        {label: "Genre", value: entry.genre || ""},
        {label: "Developer", value: entry.developer || ""},
        {label: "Publisher", value: entry.publisher && entry.publisher !== entry.developer ? entry.publisher : ""}
    ].filter(f => f.value.length > 0)
    readonly property string synopsis: artwork.displayDescription === false ? "" : entry.synopsis || entry.description || ""
    readonly property real mediaWidth: width * 0.56
    function scrollDescription(direction) {
        if (active) descriptionViewport.contentY = Math.max(0, Math.min(Math.max(0, descriptionViewport.contentHeight-descriptionViewport.height), descriptionViewport.contentY + direction * 38))
    }
    onGameIdChanged: { descriptionViewport.contentY = 0; title.restart() }
    Item {
        id: title; objectName: "library-detail-title"
        width: parent.width - platformBadge.width - (root.favoritesAvailable ? 52 : 12); height: 36; clip: true
        readonly property real travel: Math.max(0, heading.implicitWidth-width)
        readonly property bool moving: root.active && root.visible && !Theme.reducedMotion && Qt.application.state === Qt.ApplicationActive && travel > 1
        function restart() { ticker.stop();heading.x=0;if(moving)ticker.start() }
        onMovingChanged: restart()
        onTravelChanged: restart()
        Text {
            id: heading; objectName: "library-title-text"
            text: root.entry.title || ""; textFormat: Text.PlainText
            width: Theme.reducedMotion ? title.width : implicitWidth; height: parent.height
            verticalAlignment: Text.AlignVCenter
            font.family: Theme.displayFamily; font.pixelSize: 25; font.bold: true
            color: Theme.ink; wrapMode: Text.NoWrap; elide: Theme.reducedMotion ? Text.ElideRight : Text.ElideNone
        }
        SequentialAnimation {
            id: ticker; loops: Animation.Infinite
            PauseAnimation { duration: 4500 }
            NumberAnimation { target: heading; property: "x"; to: -title.travel; duration: Math.max(800,title.travel/32*1000) }
            PauseAnimation { duration: 1800 }
            PropertyAction { target: heading; property: "x"; value: 0 }
        }
    }
    PlatformBadge {
        id: platformBadge; objectName: "library-detail-platform"
        anchors.right: parent.right; y: 0; frameless: true
        label: root.entry.platformShort || ""; shape: root.entry.platformShape || "console"
    }
    FavoriteStar {
        id: favorite; objectName: "library-detail-favorite"
        visible: root.favoritesAvailable
        anchors.right: platformBadge.left; anchors.rightMargin: 4; y: 0
        marked: root.entry.favorite === true; active: root.active
        onActivated: root.favoriteRequested(root.gameId)
    }
    Flickable {
        id: descriptionViewport; objectName: "library-detail-scroll"
        y: Math.max(title.height,platformBadge.height)+8; width: parent.width; height: Math.max(0,parent.height-y)
        contentWidth: width; contentHeight: Math.max(picture.height,description.y+description.height,factsColumn.height)+10
        clip: true; boundsBehavior: Flickable.StopAtBounds; interactive: root.active
        Rectangle {
            x: picture.x + 1; y: picture.y + 4; width: picture.width; height: picture.height
            radius: 12; color: Theme.surfaceColor("#30192e2b")
        }
        Rectangle {
            id: picture; objectName: "library-detail-picture"
            readonly property real aspect: Math.max(0.45,Math.min(3,preview.imageAspect))
            x: root.width - width - 6; y: 0
            width: Math.min(root.mediaWidth, root.height * 0.56 * aspect + 14)
            height: (width - 14) / aspect + 14
            radius: 11; border.color: Qt.darker(Theme.rim, 1.2); border.width: 1
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.surfaceColor("#fff9df") }
                GradientStop { position: 0.45; color: Theme.surfaceColor("#e9dfbc") }
                GradientStop { position: 1; color: Theme.surfaceColor("#cabd98") }
            }
            Rectangle {
                anchors.fill: parent; anchors.margins: 2; radius: 9; color: "transparent"
                border.width: 1; border.color: Theme.surfaceColor("#fffdf0")
            }
            Rectangle { anchors.fill: parent; anchors.margins: 5; radius: 5; color: Theme.chassisDark }
            GamePreview {
                id: preview; anchors.fill: parent; anchors.margins: 7
                picture: root.artwork.displayPicture || root.entry.screenshot || ""
                video: root.artwork.video || ""
                previewSound: root.previewSound
                previewDelay: root.previewDelay
                playbackAllowed: root.previewsEnabled && root.active && !Theme.reducedMotion
                    && preview.parent.y + preview.parent.height > descriptionViewport.contentY
                    && preview.parent.y < descriptionViewport.contentY + descriptionViewport.height
                themedFallback: true; fallbackTitle: root.entry.title || ""
                fallbackArt: root.entry.logo || root.artwork.cover || ""
            }
        }
        Column {
            id: factsColumn; objectName: "library-detail-facts"
            x: 0; y: 0; width: root.width-root.mediaWidth-22; spacing: 7
            HomeConnectionMarks { modes: root.modes; visible: root.modes.length > 0 }
            Repeater {
                model: root.facts
                Item {
                    required property var modelData
                    width: factsColumn.width; height: Math.max(23,value.implicitHeight+7)
                    Text {
                        width: parent.width * 0.42; text: modelData.label; textFormat: Text.PlainText
                        font.pixelSize: 13; color: Theme.muted; elide: Text.ElideRight
                    }
                    Text {
                        id: value; x: parent.width * 0.44; width: parent.width-x
                        text: modelData.value; textFormat: Text.PlainText; font.pixelSize: 14; font.weight: Font.Medium; color: Theme.ink
                        wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.ink; opacity: 0.13 }
                }
            }
        }
        Text {
            id: description; objectName: "multiverse-description"
            y: factsColumn.height + 12; width: parent.width - 10
            readonly property real exclusionBottom: picture.height + 12 - y
            readonly property real narrowWidth: factsColumn.width
            onExclusionBottomChanged: Qt.callLater(forceLayout)
            onNarrowWidthChanged: Qt.callLater(forceLayout)
            // Qt's native plain-text line layout flows around the image without
            // HTML parsing, duplicated text, or manual substring measurement.
            onLineLaidOut: function(line) {
                line.x = 0
                line.width = line.y < exclusionBottom ? narrowWidth : width
            }
            text: (!root.entry.playable && root.entry.status ? root.entry.status + "\n\n" : "") + root.synopsis
            visible: text.length > 0; textFormat: Text.PlainText
            color: Theme.ink; font.pixelSize: 14; lineHeight: 1.12; wrapMode: Text.WordWrap
        }
    }
    Rectangle {
        x: parent.width - width; width: 3; radius: 1.5; color: Theme.rim; opacity: 0.7
        visible: descriptionViewport.contentHeight > descriptionViewport.height + 1
        height: Math.max(12, descriptionViewport.height * descriptionViewport.height / Math.max(1,descriptionViewport.contentHeight))
        y: descriptionViewport.y + Math.max(0,Math.min(1,descriptionViewport.contentY / Math.max(1,descriptionViewport.contentHeight-descriptionViewport.height))) * (descriptionViewport.height-height)
    }
}
