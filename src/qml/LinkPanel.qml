import QtQuick

Item {
    id: root
    required property var shell
    required property var controller
    readonly property var fighters: controller.fighters
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.drawerOpen && !shell.notice.length
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset
        anchors.topMargin: Theme.contentTopInset
        PageHeader {
            id: heading; compact: true
            title: root.controller.mode === "battle" ? "Friendly battle" : root.controller.mode === "sale" ? "Pokemon market" : root.controller.mode === "gift" ? "A gift for a friend" : root.controller.mode === "trade" ? "Trade Counter" : "Link Counter"
            trailing: root.controller.partner || "Nearby friends"
        }
        MountedPanel {
            anchors.top: heading.bottom; anchors.bottom: parent.bottom; width: parent.width; color: "#e0edcf"
            Rectangle {
                anchors.fill: parent
                gradient: Gradient { GradientStop {position: 0; color: "#d6eddd"} GradientStop {position: 1; color: "#f5efd4"} }
                opacity: 0.7
            }
            Text { id: message; x: 24; y: 16; width: parent.width-48; text: root.controller.message; color: Theme.ink; font.pixelSize: 19; font.bold: true; wrapMode: Text.WordWrap }
            Text {
                x: 24; y: message.y + message.height + 5; width: parent.width-48
                visible: root.controller.mode === "battle" && root.controller.turnSummary.length > 0
                text: root.controller.turnSummary; color: Theme.muted; font.pixelSize: 13
                wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
            }
            Item {
                x: 24; y: message.y + Math.max(34,message.height) + 16; width: parent.width - 48; height: parent.height-y-20
                visible: ["browse","pair","lobby","error"].indexOf(root.controller.stage)>=0
                ListView {
                    width: parent.width * 0.48; height: parent.height; spacing: 14; clip: true
                    model: root.controller.rows; currentIndex: root.controller.focusIndex
                    highlightMoveDuration: Theme.motion(120); preferredHighlightBegin: 0; preferredHighlightEnd: height-80; highlightRangeMode: ListView.ApplyRange
                    delegate: CapButton {
                        required property int index; required property var modelData
                        width: ListView.view.width; height: root.controller.stage === "lobby" ? (ListView.view.height-42)/4 : 80; tint: index%2 ? Theme.blue : Theme.green
                        label: modelData.name || "Friend"; detail: modelData.detail || "TrainerOS · local Wi-Fi"
                        selected: root.takesFocus && root.controller.focusIndex===index
                        onActivated: root.controller.activate(index)
                    }
                }
                Rectangle {
                    x: parent.width*(root.controller.stage === "pair" || root.controller.stage === "error" ? 0.25 : 0.55)
                    y: 15; width: parent.width*(root.controller.stage === "pair" || root.controller.stage === "error" ? 0.50 : 0.42); height: Math.min(240,parent.height-28)
                    radius: 24; color: "#edf7e0"; border.color: "#71998a"; border.width: 3
                    Rectangle {x: -15; y: parent.height*0.40; width: 22; height: parent.height*0.20; radius: 5; color: "#4c8e80"}
                    Rectangle {anchors.right: parent.right; anchors.rightMargin: -15; y: parent.height*0.40; width: 22; height: parent.height*0.20; radius: 5; color: "#4c8e80"}
                    Column {
                        anchors.centerIn: parent; width: parent.width-32; spacing: 18
                        Text {width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.controller.code || "LINK"; color: "#246858"; font.bold: true; font.pixelSize: root.controller.code ? 46 : 58; font.letterSpacing: 4}
                        Text {width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.controller.code ? "Same code on both consoles" : "Emerald ↔ Emerald"; wrapMode: Text.WordWrap; color: Theme.muted; font.pixelSize: 16}
                        CapButton {
                            width: parent.width; height: 46; centered: true; tint: Theme.yellow
                            visible: root.controller.stage === "pair" || root.controller.stage === "error"
                            label: root.controller.stage === "pair" ? "Connect with this friend" : "Return"
                            selected: root.takesFocus && visible; onActivated: root.controller.activate(0)
                        }
                    }
                }
            }
            Column {
                anchors.centerIn: parent; width: parent.width * 0.6; spacing: 20
                visible: root.controller.stage === "price"
                Text {width: parent.width; text: "₽ " + root.controller.priceText; horizontalAlignment: Text.AlignHCenter; color: "#aa7117"; font.pixelSize: 64; font.bold: true}
                CapButton {width: parent.width; height: 56; label: "Offer this price"; centered: true; tint: Theme.yellow; selected: root.takesFocus && visible; onActivated: root.controller.activate(0)}
            }
            Grid {
                x: 24; y: message.y + Math.max(34,message.height)+18; width: parent.width-48
                height: parent.height-y-16
                columns: 2; spacing: 16; visible: root.controller.stage === "choose"
                Repeater {
                    model: root.controller.rows
                    CapButton {
                        required property int index; required property var modelData
                        width: (parent.width-16)/2; height: Math.max(54,(parent.height-32)/3); tint: [Theme.blue,Theme.green,Theme.yellow,Theme.pink][index%4]
                        contentInset: 78
                        label: modelData.name; detail: modelData.detail
                        Image {x: 12; y: 6; width: 57; height: parent.height-12; source: modelData.art && modelData.art.url ? modelData.art.url : ""; fillMode: Image.PreserveAspectFit; asynchronous: true}
                        selected: root.takesFocus && root.controller.focusIndex===index
                        onActivated: root.controller.activate(index)
                    }
                }
            }
            Item {
                x: 24; y: message.y+Math.max(34,message.height)+(root.controller.turnSummary.length ? 30 : 12); width: parent.width-48; height: parent.height-y-(root.controller.stage === "moves" ? 175 : 105)
                visible: ["review","waiting","starting","saving","moves","finished"].indexOf(root.controller.stage)>=0
                PracticeFighter {x: parent.width*0.06; y: 12; width: parent.width*0.35; height: parent.height-12; fighter: root.fighters[0] || ({}); visible: !fighter.payment; showHealth: root.controller.mode === "battle" && root.controller.stage !== "review"; playing: root.takesFocus}
                PracticeFighter {x: parent.width*0.60; width: parent.width*0.33; height: parent.height-12; fighter: root.fighters[1] || ({}); visible: !fighter.payment; opponent: true; showHealth: root.controller.mode === "battle" && root.controller.stage !== "review"; playing: root.takesFocus}
                Repeater {
                    model: root.fighters
                    delegate: Rectangle {
                        required property int index; required property var modelData
                        x: parent.width*(index===0?0.06:0.60); y: 20; width: parent.width*0.34; height: parent.height-36
                        visible: !!modelData.payment; color: "#ffe5a0"; radius: 18; border.width: 2; border.color: "#bc9346"
                        Column {anchors.centerIn: parent; width: parent.width-24; spacing: 12
                            Text {width: parent.width; text: modelData.name || ""; color: Theme.ink; font.pixelSize: 25; font.bold: true; horizontalAlignment: Text.AlignHCenter}
                            Text {width: parent.width; text: modelData.detail || ""; color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter}
                        }
                    }
                }
                Text {anchors.centerIn: parent; text: root.controller.mode === "battle" ? "VS" : "↔"; font.pixelSize: 44; font.bold: true; color: "#b69038"}
            }
            Grid {
                anchors.bottom: parent.bottom; anchors.bottomMargin: 14; x: 24; width: parent.width-48; columns: 2; spacing: 10
                visible: root.controller.stage === "moves"
                Repeater {
                    model: root.controller.rows
                    CapButton {
                        required property int index; required property var modelData
                        width: (parent.width-10)/2; height: 44; tint: [Theme.blue,Theme.green,Theme.yellow,Theme.pink][index%4]
                        label: modelData.name + "   " + modelData.detail; textSize: 16
                        selected: root.takesFocus && root.controller.focusIndex===index
                        onActivated: root.controller.activate(index)
                    }
                }
            }
            CapButton {
                anchors.bottom: parent.bottom; anchors.bottomMargin: 22; anchors.horizontalCenter: parent.horizontalCenter
                width: 340; height: 52; centered: true; tint: Theme.yellow
                visible: root.controller.stage === "review" || root.controller.stage === "finished"
                label: root.controller.stage === "finished" ? "Back to Center" : root.controller.mode === "battle" ? "Ready to battle" : root.controller.mode === "sale" ? "Confirm this sale" : root.controller.mode === "gift" ? "Confirm this gift" : "Confirm this exchange"
                selected: root.takesFocus && visible; onActivated: root.controller.activate(0)
            }
        }
    }
}
