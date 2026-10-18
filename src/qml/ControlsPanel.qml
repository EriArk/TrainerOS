import QtQuick

Panel {
    id: root
    required property var shell
    property var board: shell.controls
    radius: 30
    surface: Qt.tint(Theme.paper,"#09c29948")
    patterned: true
    Text { x: 28; y: 23; text: "Controls"; font.family: Theme.displayFamily; font.pixelSize: 30; color: Theme.ink }
    CapButton { x: 190; y: 23; width: 88; height: 32; label: root.board.editing?"Done":"Edit"; textSize: 15; centered: true; tint: Theme.paper; selected: root.board.selected === "edit"; onActivated: root.board.edit() }
    Row { anchors.right: parent.right; anchors.rightMargin: 70; y: 18; spacing: 14
        Repeater { model: [{id:"settings",title:"Settings",icon:"settings"},{id:"power",title:"Power",icon:"power"},{id:"switch",title:"Switch",icon:"desktop"}]
            Item { required property var modelData; width: 56; height: 54
                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 34; height: 34; radius: 17; color: Theme.paper; border.color: root.board.selected === modelData.id?Theme.focusGlow:Theme.screenContact; border.width: root.board.selected===modelData.id?2:1 }
                MenuGlyph { anchors.horizontalCenter: parent.horizontalCenter; y: 6; width: 22; height: 22; glyph: modelData.icon }
                Text { anchors.bottom: parent.bottom; width: parent.width; text: modelData.title; color: Theme.ink; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
                MouseArea { anchors.fill: parent; onClicked: root.board.activate(modelData.id) }
            }
        }
    }
    SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 20; y: 22; icon: "x"; label: "Close Controls"; onClicked: root.shell.closeControls() }
    Item {
        id: grid; x: 27; y: 88; width: parent.width-54; height: parent.height-y-62
        readonly property real cellW: (width+9)/12
        readonly property real cellH: (height+9)/4
        Repeater { model: root.board.widgets
            ControlWidget { shell: root.shell
                x: modelData.x*grid.cellW; y: modelData.y*grid.cellH
                width: modelData.w*grid.cellW-9; height: modelData.h*grid.cellH-9
                onMoved: (dx,dy) => {
                    const x=Math.round(modelData.x+dx/grid.cellW)
                    if(x<0 || x+modelData.w>12){root.board.activate(modelData.id);root.board.changePage(x<0?-1:1);root.board.activate(modelData.id)}
                    else root.board.drop(modelData.id,x,Math.round(modelData.y+dy/grid.cellH))
                }
            }
        }
        Text { anchors.centerIn: parent; visible: root.board.widgets.length === 0; text: "Add the controls you use"; color: Theme.muted; font.pixelSize: 20 }
    }
    Row { anchors.bottom: parent.bottom; anchors.bottomMargin: 21; x: 28; spacing: 9; visible: root.board.editing
        Repeater { model: [{id:"add",name:"+ Add"},{id:"resize",name:"X Size"},{id:"hide",name:"Y Hide"}]
            CapButton { required property var modelData; width: 87; height: 28; textSize: 13; label: modelData.name; centered: true; tint: Theme.paper; onActivated: root.board.activate(modelData.id) }
        }
    }
    Row { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 22; spacing: 15; visible: !root.board.editing && root.board.pages > 1
        Text { text: "‹ L1"; color: Theme.muted; font.pixelSize: 13; MouseArea { anchors.fill: parent; onClicked: root.board.changePage(-1) } }
        Repeater { model: root.board.pages; Rectangle { required property int index; width: 7; height: 7; y: 5; radius: 4; color: index===root.board.page?Theme.chassis:Theme.screenContact } }
        Text { text: "R1 ›"; color: Theme.muted; font.pixelSize: 13; MouseArea { anchors.fill: parent; onClicked: root.board.changePage(1) } }
    }
    Text { anchors.right: parent.right; anchors.rightMargin: 28; anchors.bottom: parent.bottom; anchors.bottomMargin: 22; color: Theme.muted; font.pixelSize: 12
        text: root.board.moving?"A Place · B Cancel · L1/R1 Page":root.board.editing?"A Move · L2 Add":root.board.adjusting?(root.board.selected==="volume"?"↑↓ Adjust · X Mute · B Done":"↑↓ Adjust · B Done"):"A Select · B Close" }
    Text { x: 30; y: grid.y+grid.height+4; width: parent.width-60; color: Theme.muted; text: root.board.status || root.shell.device.error; font.pixelSize: 11; elide: Text.ElideRight }
    Rectangle { anchors.fill: parent; radius: 15; color: "#99182b29"; visible: root.board.picking
        MouseArea { anchors.fill: parent; onClicked: {} }
        Panel { anchors.centerIn: parent; width: 360; height: Math.min(root.height-40,105+root.board.available.length*40)
            Text { x: 24; y: 22; text: "Add a control"; color: Theme.ink; font.family: Theme.displayFamily; font.pixelSize: 23 }
            SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 18; y: 17; icon: "x"; label: "Close"; onClicked: root.board.back() }
            Text { anchors.centerIn: parent; visible: !root.board.available.length; text: "All controls are already placed"; font.pixelSize: 16; color: Theme.muted }
            ListView { x: 23; y: 65; width: parent.width-46; height: parent.height-y-18; clip: true; spacing: 4; model: root.board.available
                delegate: CapButton { required property var modelData; width: ListView.view.width; height: 36; label: modelData.title; selected: modelData.selected; tint: Theme.paper; textSize: 16; onActivated: root.board.restore(modelData.id) }
            }
        }
    }
    Rectangle { anchors.fill: parent; radius: 15; color: "#99182b29"; visible: root.board.switching
        MouseArea { anchors.fill: parent }
        Panel { anchors.centerIn: parent; width: 390; height: 220
            Text { x: 25; y: 23; text: "Switch environment"; font.family: Theme.displayFamily; font.pixelSize: 24; color: Theme.ink }
            SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 17; y: 17; icon: "x"; label: "Cancel"; onClicked: root.board.back() }
            ActivityRow { x: 24; y: 76; width: parent.width-48; label: "Desktop"; detail: "Maintenance and desktop apps"; glyph: "desktop"; selected: root.board.switchIndex===0; onActivated: root.board.activate("desktop") }
            ActivityRow { x: 24; y: 140; width: parent.width-48; label: "Steam Gaming Mode"; glyph: "game-controller"; selected: root.board.switchIndex===1; onActivated: root.board.activate("steam") }
        }
    }

}
