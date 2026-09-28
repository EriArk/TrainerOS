import QtQuick

Item {
    id: root
    required property var shell
    required property var controller
    readonly property string stage: controller.stage
    readonly property bool takesFocus: visible && !shell.menuOpen && !shell.drawerOpen && !shell.notice.length
    readonly property bool arena: controller.mode === "battle" && ["starting","saving","moves","events","waiting","concede","finished"].indexOf(stage) >= 0 && controller.battleStarted
    readonly property bool choosing: stage === "choose" || stage === "stake"
    readonly property bool meeting: ["browse","pair","error"].indexOf(stage) < 0 && !arena
    readonly property var fighters: controller.fighters
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset; anchors.topMargin: Theme.contentTopInset
        PageHeader { id: heading; compact: true; title: root.arena ? "Link Battle" : "Link Counter"; trailing: root.arena ? root.controller.stakeText : root.controller.partner || "Nearby friends" }
        Rectangle {
            id: desk
            anchors.top: heading.bottom; anchors.bottom: parent.bottom; width: parent.width
            color: "#f8f3dd"; clip: true
            // A fixed counter: activities on the left, partners and terms on the
            // right. Choosing a Pokemon or price never replaces the whole room.
            Rectangle {
                id: rail; width: 205; height: parent.height; visible: !root.arena
                color: "#c4d9cd"
                Rectangle { anchors.right: parent.right; width: 3; height: parent.height; color: "#839e8b" }
                Text { x: 17; y: 16; text: "TOGETHER"; font.family: Theme.brandFamily; font.pixelSize: 17; color: Theme.ink }
                Column {
                    x: 13; y: 52; width: parent.width-28; spacing: 12
                    Repeater {
                        model: ["Battle", "Trade", "Sell", "Give"]
                        CapButton {
                            required property int index; required property string modelData
                            width: parent.width; height: 47; label: modelData; contentInset: 43
                            tint: [Theme.yellow, Theme.blue, Theme.pink, Theme.green][index]
                            selected: root.takesFocus && root.stage === "lobby" && root.controller.focusIndex === index
                            opacity: root.controller.mode.length && root.controller.mode !== ["battle","trade","sale","gift"][index] ? 0.55 : 1
                            deferredFocus: true
                            onActivated: if (root.stage === "lobby") root.controller.activate(index)
                            Text { x: 13; anchors.verticalCenter: parent.verticalCenter; text: ["⚔", "⇄", "₽", "♥"][index]; font.pixelSize: 23; color: Theme.ink }
                        }
                    }
                }
            }
            Item {
                x: rail.width + 22; width: parent.width-x-22; height: parent.height
                visible: !root.arena
                Text { id: status; y: 12; width: parent.width; text: root.controller.message; font.pixelSize: 17; font.bold: true; color: Theme.ink; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
                ListView {
                    y: 65; width: parent.width; height: parent.height-y-18; spacing: 12; clip: true
                    visible: root.stage === "browse"
                    model: visible ? root.controller.rows : []; currentIndex: root.controller.focusIndex
                    delegate: CapButton {
                        required property int index; required property var modelData
                        width: ListView.view.width-8; height: 67; label: modelData.name || "Friend"; detail: modelData.detail || "Nearby console"; tint: Theme.blue
                        selected: root.takesFocus && root.controller.focusIndex === index; deferredFocus: true
                        onActivated: root.controller.activate(index)
                    }
                }
                Column {
                    anchors.centerIn: parent; width: parent.width-40; spacing: 22
                    visible: root.stage === "pair" || root.stage === "error"
                    Text { width: parent.width; text: root.controller.code; font.family: Theme.brandFamily; font.pixelSize: 58; font.letterSpacing: 5; color: Theme.ink; horizontalAlignment: Text.AlignHCenter }
                    CapButton { width: parent.width; height: 53; label: root.stage === "pair" ? "Connect" : "Return"; centered: true; selected: root.takesFocus; deferredFocus: true; onActivated: root.controller.activate(0) }
                }
                Item {
                    y: 64; width: parent.width; height: parent.height-y-14
                    visible: root.stage === "lobby"
                    Text { y: 4; text: "Your partners"; font.family: Theme.displayFamily; font.pixelSize: 27; color: Theme.ink }
                    Row {
                        y: 52; spacing: 10
                        Repeater { model: root.controller.previewTeam
                            LinkPortrait { required property int index; required property var modelData; width: Math.min(72,(parent.parent.width-50)/6); height: width; member: modelData; tint: [Theme.blue,Theme.green,Theme.yellow][index%3] }
                        }
                    }
                    Text { y: 151; width: parent.width; text: "A friendly match, a new partner,\nor a gift for someone special."; font.family: Theme.displayFamily; font.pixelSize: 24; color: "#517d6a"; lineHeight: 1.2 }
                }
                Item {
                    y: 59; width: parent.width; height: parent.height-y-12
                    visible: root.choosing
                    Text { id: collection; text: root.stage === "choose" && root.controller.mode === "battle" ? "First into battle" : root.controller.collectionName; font.family: Theme.displayFamily; font.pixelSize: 21; color: Theme.ink }
                    GridView {
                        y: 34; width: parent.width; height: parent.height-y; clip: true
                        cellWidth: width/2; cellHeight: 70; model: visible ? root.controller.rows : []
                        currentIndex: root.controller.focusIndex; highlightRangeMode: GridView.ApplyRange
                        preferredHighlightBegin: 0; preferredHighlightEnd: height-cellHeight
                        delegate: CapButton {
                            required property int index; required property var modelData
                            width: GridView.view.cellWidth-10; height: 59; contentInset: 66; textSize: 15
                            label: modelData.name; detail: modelData.detail; tint: [Theme.blue,Theme.green,Theme.pink,Theme.yellow][index%4]
                            selected: root.takesFocus && root.controller.focusIndex === index; deferredFocus: true
                            LinkPortrait { x: 5; y: 5; width: 49; height: 49; member: modelData; tint: parent.tint }
                            onActivated: root.controller.activate(index)
                        }
                    }
                }
                Item {
                    y: 76; width: parent.width; height: parent.height-y-14
                    visible: !root.choosing && ["review","price","waiting","saving","starting","finished"].indexOf(root.stage)>=0
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter; spacing: 38; y: 5
                        Repeater { model: root.fighters
                            Column {
                                required property int index; required property var modelData
                                spacing: 8; width: 170
                                LinkPortrait { anchors.horizontalCenter: parent.horizontalCenter; width: 96; height: 96; member: modelData; tint: index ? Theme.pink : Theme.blue; visible: !modelData.payment }
                                Text { width: parent.width; text: modelData.name || "Choosing…"; font.family: Theme.displayFamily; font.pixelSize: 21; color: Theme.ink; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
                                Text { width: parent.width; visible: !!modelData.payment; text: modelData.detail || ""; font.pixelSize: 15; color: Theme.muted; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
                            }
                        }
                    }
                    Text { y: 156; width: parent.width; visible: root.stage === "price"; text: "₽ " + root.controller.priceText; font.family: Theme.brandFamily; font.pixelSize: 36; color: "#946218"; horizontalAlignment: Text.AlignHCenter }
                    Text { y: 153; width: parent.width; visible: root.controller.mode === "battle" && root.stage !== "price"; text: root.controller.stakeText; font.pixelSize: 19; font.bold: true; color: "#946218"; horizontalAlignment: Text.AlignHCenter }
                    CapButton {
                        anchors.bottom: parent.bottom; width: parent.width; height: 43; centered: true; deferredFocus: true
                        visible: ["review","price","finished"].indexOf(root.stage)>=0
                        label: root.stage === "price" ? "Offer this amount" : root.stage === "finished" ? "Back to Center" : root.controller.mode === "battle" ? "Ready to battle" : "Confirm " + (root.controller.mode === "sale" ? "sale" : root.controller.mode === "gift" ? "gift" : "trade")
                        selected: root.takesFocus && visible; onActivated: root.controller.activate(0)
                    }
                }
            }
            Item {
                id: arena; anchors.fill: parent; visible: root.arena
                Rectangle {
                    width: parent.width; height: parent.height-137
                    gradient: Gradient { GradientStop { position: 0; color: "#cee5b6" } GradientStop { position: 1; color: "#93be8b" } }
                    Rectangle { anchors.centerIn: parent; width: parent.width*0.57; height: parent.height*0.84; radius: height/2; color: "transparent"; border.width: 3; border.color: "#79ffffff" }
                    Rectangle { anchors.centerIn: parent; width: 3; height: parent.height; color: "#65ffffff" }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; y: 14; text: root.controller.battleTurn ? "TURN " + root.controller.battleTurn : "LINK"; font.family: Theme.brandFamily; font.pixelSize: 18; color: Theme.ink }
                    PracticeFighter { x: 18; y: 17; width: parent.width*0.40; height: parent.height-27; fighter: root.fighters[0] || ({}); playing: root.takesFocus }
                    PracticeFighter { anchors.right: parent.right; anchors.rightMargin: 18; y: 17; width: parent.width*0.40; height: parent.height-27; fighter: root.fighters[1] || ({}); opponent: true; playing: root.takesFocus }
                }
                Rectangle {
                    id: controls; anchors.bottom: parent.bottom; width: parent.width; height: 137; color: "#f6efd7"; border.color: "#7a9784"; border.width: 2
                    Column {
                        x: 14; y: 9; width: parent.width*0.40; spacing: 7
                        Text { width: parent.width; text: root.stage === "concede" ? root.controller.message : root.controller.turnSummary || "Make your move!"; color: Theme.ink; font.pixelSize: 14; wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight }
                        Row { spacing: 5
                            Repeater { model: root.controller.team
                                LinkPortrait { required property var modelData; width: 39; height: 39; member: modelData; selected: !!modelData.active; opacity: modelData.fainted ? 0.3 : 1; tint: Theme.blue }
                            }
                        }
                    }
                    Item {
                        x: parent.width*0.44; y: 8; width: parent.width-x-12; height: parent.height-16
                        Text { id: actionTitle; text: root.stage === "events" ? "Turn in progress" : root.stage === "finished" ? "Result" : root.controller.battlePanel === "team" ? "Change partner" : root.controller.battlePanel === "target" ? "Use on…" : root.controller.battlePanel === "bag" ? "Medicine" : "Attacks"; font.family: Theme.displayFamily; font.pixelSize: 18; color: Theme.ink }
                        GridView {
                            y: 24; width: parent.width; height: parent.height-y; cellWidth: width/2; cellHeight: 45; clip: true
                            visible: root.stage === "moves"; model: visible ? root.controller.rows : []; currentIndex: root.controller.focusIndex
                            highlightRangeMode: GridView.ApplyRange; preferredHighlightBegin: 0; preferredHighlightEnd: height-cellHeight
                            delegate: CapButton {
                                required property int index; required property var modelData
                                width: GridView.view.cellWidth-7; height: 37; textSize: 14; label: modelData.name + "   " + modelData.detail; detail: ""
                                contentInset: modelData.portraits ? 43 : 10; tint: [Theme.blue,Theme.green,Theme.yellow,Theme.pink][index%4]
                                selected: root.takesFocus && root.controller.focusIndex === index; deferredFocus: true
                                LinkPortrait { x: 4; y: 3; width: 30; height: 30; visible: !!modelData.portraits; member: modelData; tint: parent.tint }
                                onActivated: root.controller.activate(index)
                            }
                        }
                        CapButton { y: 40; width: parent.width; height: 49; visible: root.stage === "finished" || root.stage === "concede"; label: root.stage === "concede" ? "Concede this battle" : root.controller.message; textSize: 15; tint: Theme.yellow; selected: root.takesFocus && visible; deferredFocus: true; onActivated: root.controller.activate(0) }
                        Text { y: 43; width: parent.width; visible: root.stage !== "events" && root.stage !== "finished" && root.stage !== "concede" && (root.stage !== "moves" || root.controller.rows.length === 0); text: root.stage === "moves" ? (root.controller.battlePanel === "bag" ? "No medicine can help right now." : "No available partner.") : root.controller.message; color: Theme.muted; font.pixelSize: 16; wrapMode: Text.WordWrap }
                    }
                }
            }
        }
    }
}
