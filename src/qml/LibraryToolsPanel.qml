import QtQuick

Item {
    id: root
    required property var shell
    readonly property var tools: shell.libraryTools
    readonly property bool takesFocus: visible && !shell.keyboard.open && !shell.menuOpen && !shell.notice.length && !tools.busy
    readonly property bool reviews: tools.route === "reviews"
    Rectangle { anchors.fill: parent; color: "#660d2525" }
    TapHandler { onTapped: {} }
    Rectangle {
        anchors.centerIn: parent; width: root.reviews ? Math.min(970,parent.width-36) : tools.route==="properties" ? 660 : 480
        height: Math.min(parent.height-24, header.implicitHeight+(body.text.length?body.implicitHeight:0)+list.height+(error.text.length?error.implicitHeight:0)+44)
        radius: 18; color: "#e8edda"; border.color: "#35544f"; border.width: 3
        Rectangle { x: 3; y: 3; width: parent.width-6; height: parent.height-6; radius: 15; color: "transparent"; border.color: "#ffffff"; opacity: 0.6 }
        Column {
            x: 20; y: 18; width: parent.width-40; spacing: 8
            Text { id: header; width: parent.width; text: root.tools.title; textFormat: Text.PlainText; font.family: Theme.displayTypeface.name; font.pixelSize: 26; font.bold: true; color: Theme.ink; elide: Text.ElideRight }
            Text { id: body; width: parent.width; visible: text.length>0; text: root.tools.detail; textFormat: Text.PlainText; font.pixelSize: 16; color: Theme.muted; wrapMode: Text.Wrap; maximumLineCount: tools.route==="properties" ? 3 : 7; elide: Text.ElideMiddle }
            Item {
                width: parent.width; height: root.reviews ? 285 : Math.min(232,list.contentHeight)
            ListView {
                id: list; width: root.reviews ? parent.width*0.36 : parent.width; height: parent.height; clip: true; spacing: 6
                interactive: false; keyNavigationEnabled: false
                model: root.tools.rows; currentIndex: root.tools.focusIndex
                function reveal() {if(count)positionViewAtIndex(currentIndex,ListView.Contain)}
                onCurrentIndexChanged: Qt.callLater(reveal)
                onCountChanged: Qt.callLater(reveal)
                delegate: CapButton {
                    required property int index; required property var modelData
                    objectName: "library-tool-"+index
                    width: list.width; height: root.tools.route==="properties" ? 36 : 48; textSize: root.tools.route==="properties" ? 16 : 20; label: modelData.label
                    opacity: modelData.enabled ? 1 : 0.42
                    tint: root.tools.route==="remove" && index===1 ? Theme.pink : Theme.blue
                    selected: root.takesFocus && root.tools.focusIndex===index
                    onActivated: root.shell.activate(index)
                }
            }
            Rectangle {
                visible: root.reviews; anchors.left: list.right; anchors.right: parent.right; anchors.leftMargin: 18; height: parent.height
                color: "#fff5d7"; radius: 12; border.color: "#b9a985"
                Column {
                    x: 18; y: 16; width: parent.width-36; spacing: 10
                    Text { width: parent.width; text: root.tools.selectedReview.name || "Adventure Reviews"; textFormat: Text.PlainText; color: Theme.ink; font.family: Theme.displayTypeface.name; font.pixelSize: 24; font.bold: true; elide: Text.ElideRight }
                    Text { width: parent.width; text: (root.tools.selectedReview.date || "").slice(0,10); visible: text.length>0; color: Theme.muted; font.pixelSize: 15 }
                    Flickable {
                        id: reviewText; width: parent.width; height: 200; clip: true; contentHeight: reviewBody.implicitHeight
                        Text { id: reviewBody; width: parent.width; text: root.tools.selectedReview.text || "Share a little of your adventure."; textFormat: Text.PlainText; color: Theme.ink; font.pixelSize: 17; wrapMode: Text.Wrap; onTextChanged: reviewText.contentY=0 }
                    }
                }
            }
            }
            Text { id: error; width: parent.width; visible: text.length>0; text: root.tools.busy?"Saving…":root.tools.error; textFormat: Text.PlainText; font.pixelSize: 16; color: "#803724"; wrapMode: Text.WordWrap }
        }
    }
    Connections { target: root.tools; function onScrollReview(direction) { reviewText.contentY=Math.max(0,Math.min(Math.max(0,reviewText.contentHeight-reviewText.height),reviewText.contentY+direction*140)) } }
}
