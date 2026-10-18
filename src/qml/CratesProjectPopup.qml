import QtQuick
import QtQuick.Controls

Item {
    id: root
    required property var flow
    readonly property var project: flow.project
    readonly property var releases: project.releases || []
    readonly property var release: releases[flow.releaseIndex] || ({})
    readonly property var screenshots: project.screenshots || []
    readonly property string selectedImage: screenshots[flow.screenshotIndex] || project.hero || ""
    readonly property color accent: flow.current.color || Theme.rim
    function compatibilityText() {
        return release.compatibility === "supported" ? "Supported" : release.compatibility === "incompatible" ? "Not compatible" : "Not tested"
    }
    function sizeText(bytes) { return bytes < 0 || bytes === undefined ? "" : bytes < 1048576 ? Math.ceil(bytes/1024)+" KB" : (bytes/1048576).toFixed(bytes<104857600 ? 1 : 0)+" MB" }
    Rectangle { anchors.fill: parent; color: Qt.alpha(Theme.ink,.36) }
    // Leave the shared L1/R1 and L2/R2 touch targets reachable while browsing details.
    MouseArea { x: 0; y: Theme.contentTopInset-4; width: parent.width; height: parent.height-y; onClicked: root.flow.closeDetail() }
    Item {
        id: card; objectName: "crates-project-popup"
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(770,parent.width*.88); height: Math.min(374,parent.height-Theme.contentTopInset-18)
        y: Math.max(Theme.contentTopInset+4,(parent.height-height)/2)
        Rectangle { x: 3; y: 6; width: parent.width; height: parent.height; radius: 19; color: "#50302232" }
        Rectangle { anchors.fill: parent; radius: 18; color: Theme.chassis; border.color: Theme.rim; border.width: 2 }
        Rectangle {
            anchors.fill: parent; anchors.margins: 7; radius: 12; border.color: "#b0ffffff"
            gradient: Gradient { GradientStop { position: 0; color: Theme.paper } GradientStop { position: 1; color: Qt.tint(Theme.paper,Qt.alpha(root.accent,.13)) } }
        }
        MouseArea { anchors.fill: parent }
        Text {
            objectName: "crates-project-close"; x: parent.width-38; y: 9; width: 25; height: 28
            text: "×"; font.pixelSize: 27; horizontalAlignment: Text.AlignHCenter; color: Theme.ink
            MouseArea { anchors.fill: parent; onClicked: root.flow.closeDetail(); cursorShape: Qt.PointingHandCursor }
        }
        Item {
            id: gallery; x: 22; y: 27; width: (card.width-65)*.52; height: card.height-52
            Rectangle { id: photo; width: parent.width; height: root.screenshots.length>1 ? parent.height*.71 : parent.height
                radius: 9; color: Theme.paper; border.color: Qt.alpha(Theme.rim,.65)
                CratesProjectArt { anchors.fill: parent; anchors.margins: 5; source: root.selectedImage; kind: root.project.kind || "game"; accent: root.accent; crop: false }
                Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 16; visible: !root.selectedImage
                    text: "No screenshots yet"; color: Theme.muted; font.pixelSize: 12 }
            }
            ListView {
                id: strip; x: 0; y: photo.height+9; width: parent.width; height: parent.height-y
                visible: root.screenshots.length>1; orientation: ListView.Horizontal; spacing: 8; clip: true
                boundsBehavior: Flickable.StopAtBounds; model: root.screenshots
                currentIndex: root.flow.screenshotIndex
                onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                delegate: Rectangle {
                    required property string modelData; required property int index
                    width: (strip.width-8)/2; height: strip.height-2; radius: 6; color: Theme.paper
                    border.width: 2; border.color: index===root.flow.screenshotIndex ? Theme.focusGlow : Qt.alpha(Theme.rim,.6)
                    CratesProjectArt { anchors.fill: parent; anchors.margins: 4; source: modelData; accent: root.accent }
                    MouseArea { anchors.fill: parent; onClicked: root.flow.selectScreenshot(index) }
                }
            }
        }
        Item {
            id: info; x: gallery.x+gallery.width+21; y: 30; width: card.width-x-26; height: card.height-y-24
            Text { id: title; width: parent.width-17; height: Math.min(58,implicitHeight); text: root.project.title || ""; textFormat: Text.PlainText
                font.family: Theme.displayFamily; font.pixelSize: 24; maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight; color: Theme.ink }
            Text { id: creator; y: title.height+3; width: parent.width; text: root.project.creator ? "by "+root.project.creator : ""; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.muted; font.pixelSize: 13 }
            Text { y: creator.y+22; width: parent.width; text: (root.project.kind === "module" ? "TrainerOS module" : root.project.kind === "app" ? "Application" : "Game")+(root.project.license ? " · "+root.project.license : ""); elide: Text.ElideRight; color: Theme.muted; font.pixelSize: 12 }
            Flickable {
                id: description; y: creator.y+47; width: parent.width; height: metadata.y-y-10; clip: true
                contentHeight: body.height; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                Column {
                    id: body; width: description.width-7; spacing: 12
                    Text { width: parent.width; text: root.project.description || root.project.summary || ""; textFormat: Text.PlainText; wrapMode: Text.WordWrap; font.pixelSize: 14; color: Theme.ink }
                    Text { visible: text.length>0; width: parent.width; text: root.release.compatibilityNote || ""; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 12 }
                }
                Connections { target: root.flow; function onDetailScrollRequested(delta) { const target=root.flow.versionsOpen ? versions : description; target.contentY=Math.max(0,Math.min(target.contentHeight-target.height,target.contentY+delta)) } }
            }
            Column {
                id: metadata; anchors.bottom: parent.bottom; anchors.bottomMargin: root.releases.length ? 43 : 0; width: parent.width; spacing: 7
                Rectangle { width: parent.width; height: 1; color: Qt.alpha(Theme.rim,.5) }
                Text { width: parent.width; text: root.release.version ? "Version "+root.release.version+(root.release.channel ? " · "+root.release.channel : "") : "No Crates package yet"; textFormat: Text.PlainText; elide: Text.ElideRight; color: Theme.ink; font.pixelSize: 13 }
                Text { width: parent.width; text: (root.release.architecture ? root.release.architecture+" · " : "")+root.compatibilityText(); textFormat: Text.PlainText; elide: Text.ElideRight; color: root.release.compatibility === "incompatible" ? "#984b51" : Theme.muted; font.pixelSize: 13 }
                Text { visible: text.length>0; text: root.sizeText(root.release.sizeBytes); color: Theme.muted; font.pixelSize: 12 }
            }
            CapButton { anchors.right: parent.right; anchors.bottom: parent.bottom; width: 126; height: 30; label: "Versions"; textSize: 13; centered: true; claimsFocus: false; tint: Theme.blue
                visible: root.releases.length>0; onActivated: root.flow.showVersions() }
            Rectangle {
                anchors.fill: parent; visible: root.flow.versionsOpen; color: Theme.paper
                Text { text: "Versions"; font.family: Theme.displayFamily; font.pixelSize: 24; color: Theme.ink }
                Text { anchors.right: parent.right; y: 0; text: "‹ Back"; font.pixelSize: 14; color: Theme.muted
                    MouseArea { anchors.fill: parent; anchors.margins: -4; onClicked: root.flow.showVersions() } }
                ListView {
                    id: versions; y: 35; width: parent.width; height: parent.height-35; clip: true
                    model: root.releases; spacing: 3; currentIndex: root.flow.releaseIndex; boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                    delegate: Rectangle {
                        required property var modelData; required property int index
                        width: versions.width-5; height: row.implicitHeight+20; radius: 7
                        color: index===root.flow.releaseIndex ? Qt.alpha(Theme.rim,.18) : "transparent"
                        border.color: index===root.flow.releaseIndex ? Theme.rim : "transparent"
                        Column { id: row; x: 9; y: 10; width: parent.width-18; spacing: 5
                            Text { width: parent.width; text: "v"+modelData.version+(modelData.channel ? " · "+modelData.channel : ""); textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.ink; font.pixelSize: 14; font.bold: true }
                            Text { width: parent.width; text: modelData.releasedOn || ""; textFormat: Text.PlainText; color: Theme.muted; font.pixelSize: 11 }
                            Text { width: parent.width; visible: index===root.flow.releaseIndex; text: modelData.notes || "No release notes"; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 12 }
                        }
                        MouseArea { anchors.fill: parent; onClicked: root.flow.selectRelease(index) }
                    }
                }
            }
        }
    }
}
