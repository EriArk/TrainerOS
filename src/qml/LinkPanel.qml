import QtQuick

Item {
    id: root
    required property var shell
    required property var controller
    readonly property string stage: controller.stage
    readonly property bool takesFocus: visible && !controller.invitationOpen && !shell.menuOpen && !shell.drawerOpen && !shell.notice.length
    readonly property bool arena: controller.mode === "battle" && ["starting","saving","moves","events","waiting","concede","finished"].indexOf(stage) >= 0 && controller.battleStarted
    readonly property bool choosing: stage === "choose" || stage === "stake"
    readonly property bool meeting: ["browse","pair","error"].indexOf(stage) < 0 && !arena
    readonly property var fighters: controller.fighters
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset; anchors.topMargin: Theme.contentTopInset
        PageHeader { id: heading; compact: true; title: root.arena ? "Link Battle" : root.controller.online ? "Together" : "Nearby play"; trailing: root.arena ? root.controller.stakeText : root.controller.partner || "Together, wherever you are" }
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
                        model: root.controller.activityModes
                        CapButton {
                            required property int index; required property string modelData
                            width: parent.width; height: 47; label: ({battle:"Battle",trade:"Trade",sale:"Sell",gift:"Give"})[modelData]; contentInset: 43
                            tint: [Theme.yellow, Theme.blue, Theme.pink, Theme.green][index]
                            selected: root.takesFocus && root.stage === "lobby" && root.controller.focusIndex === index
                            opacity: root.controller.mode.length && root.controller.mode !== modelData ? 0.55 : 1
                            deferredFocus: true
                            onActivated: if (root.stage === "lobby") root.controller.activate(index)
                            Text { x: 13; anchors.verticalCenter: parent.verticalCenter; text: ({battle:"⚔",trade:"⇄",sale:"₽",gift:"♥"})[modelData]; font.pixelSize: 23; color: Theme.ink }
                        }
                    }
                }
            }
            Item {
                x: rail.width + 22; width: parent.width-x-22; height: parent.height
                visible: !root.arena
                Text { id: status; y: 12; width: parent.width; text: root.stage === "browse" ? "Trainers nearby" : root.controller.message; font.family: Theme.displayFamily; font.pixelSize: 23; font.bold: true; color: Theme.ink; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
                Item {
                    id: terminal
                    anchors.horizontalCenter: parent.horizontalCenter; y: 63; width: 280; height: 195
                    visible: root.stage === "browse" && !root.controller.rows.length
                    Rectangle { x: -120; y: 155; width: 520; height: 80; color: "#e7e2c6" }
                    Repeater { model: 5
                        Rectangle { required property int index; x: -120+index*104; y: 157; width: 2; height: 78; color: "#d6d2b9" }
                    }
                    Rectangle { x: 54; y: 166; width: 184; height: 20; radius: 10; color: "#33466554" }
                    Rectangle { x: 71; y: 33; width: 144; height: 144; radius: 9; color: "#48776d"; border.width: 3; border.color: "#315a55" }
                    Rectangle { x: 60; y: 18; width: 160; height: 108; radius: 11; color: "#8cc1b0"; border.width: 4; border.color: "#315a55"
                        Rectangle { x: 12; y: 12; width: parent.width-24; height: 78; radius: 5; color: "#183e41"
                            Repeater { model: 3
                                Rectangle {
                                    required property int index
                                    anchors.centerIn: parent; width: 28+index*23; height: width; radius: width/2
                                    color: "transparent"; border.width: 2; border.color: "#8ce3c4"; opacity: 0.55
                                    SequentialAnimation on opacity {
                                        running: terminal.visible && root.takesFocus && root.controller.searching && !Theme.reducedMotion; loops: Animation.Infinite
                                        PauseAnimation { duration: index*190 }
                                        NumberAnimation { to: 0.15; duration: 550 }
                                        NumberAnimation { to: 0.8; duration: 650 }
                                        PauseAnimation { duration: (2-index)*190 }
                                    }
                                }
                            }
                            Rectangle { anchors.centerIn: parent; width: 10; height: 10; radius: 5; color: root.controller.searching ? "#ffe58d" : "#70958b" }
                        }
                    }
                    Rectangle { x: 59; y: 127; width: 162; height: 27; radius: 5; color: "#b2d5bd"; border.width: 3; border.color: "#315a55"
                        Row { x: 17; y: 9; spacing: 9
                            Repeater { model: ["#f5cb66", "#a3d4e4", "#ec9696", "#a6c771"]
                                Rectangle { required property string modelData; width: 24; height: 7; color: modelData; radius: 2 }
                            }
                        }
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 23
                    width: parent.width; horizontalAlignment: Text.AlignHCenter; textFormat: Text.PlainText
                    visible: terminal.visible; font.pixelSize: 18; color: Theme.ink
                    text: !root.controller.visibleNearby ? "Nearby visibility is off" : root.controller.searching ? "Looking for Trainers…" : "Looking on your local network…"
                }
                ListView {
                    y: 65; width: parent.width; height: parent.height-y-18; spacing: 12; clip: true
                    visible: root.stage === "browse" && root.controller.rows.length > 0
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
                    visible: root.stage === "error"
                    CapButton { width: parent.width; height: 53; label: "Return"; centered: true; selected: root.takesFocus; deferredFocus: true; onActivated: root.controller.activate(0) }
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
                    Text { y: 151; width: parent.width; text: root.controller.online ? "A new partner,\nor a gift for someone special." : "A friendly match, a new partner,\nor a gift for someone special."; font.family: Theme.displayFamily; font.pixelSize: 24; color: "#517d6a"; lineHeight: 1.2 }
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
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter; y: 18
                        width: 96; height: 96; radius: 48
                        visible: root.stage === "finished" && !root.fighters.some(function(member) { return !!member.name || !!member.payment })
                        color: Theme.green; border.width: 3; border.color: "#73965e"
                        Text { anchors.centerIn: parent; text: "✓"; font.pixelSize: 58; color: Theme.ink }
                    }
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter; spacing: 38; y: 5
                        Repeater { model: root.fighters
                            Column {
                                required property int index; required property var modelData
                                visible: root.stage !== "finished" || !!modelData.name || !!modelData.payment
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
                        label: root.stage === "price" ? "Offer this amount" : root.stage === "finished" ? "Back to friends" : root.controller.mode === "battle" ? "Ready to battle" : "Confirm " + (root.controller.mode === "sale" ? "sale" : root.controller.mode === "gift" ? "gift" : "trade")
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
