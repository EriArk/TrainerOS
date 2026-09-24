import QtQuick

// Original pixel-room composition, inspired by the warm counters of Hoenn.
Item {
    id: root
    required property var shell
    readonly property var clinic: shell.center
    readonly property bool active: visible && !shell.menuOpen && !shell.drawerOpen && !shell.notice.length && Qt.application.state === Qt.ApplicationActive
    readonly property bool treating: clinic.treatment === "healing" || recovery.running
    property var team: []
    property int pulse: 0
    property string lastTreatment: "ready"
    onVisibleChanged: if (visible) { team = shell.party.activities.actors; pulse = 0 }
    Timer { interval: 210; running: root.active && root.treating && !Theme.reducedMotion; repeat: true; onTriggered: ++root.pulse }
    Timer { id: recovery; interval: 2300 }
    Connections {
        target: root.clinic
        function onChanged() {
            if(root.clinic.treatment === "healing" && root.lastTreatment !== "healing" && !Theme.reducedMotion)recovery.restart()
            if(root.clinic.treatment === "error" || !root.clinic.clinicOpen)recovery.stop()
            root.lastTreatment=root.clinic.treatment
        }
    }
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset; anchors.topMargin: Theme.contentTopInset
        PageHeader { id: heading; title: "Pokémon Center"; subtitle: root.clinic.title; compact: true }
        Item {
            id: room; objectName: "pokemon-clinic-room"
            y: heading.height; width: parent.width; height: parent.height-y; clip: true
            Canvas {
                anchors.fill: parent; onWidthChanged: requestPaint(); onHeightChanged: requestPaint()
                onPaint: {
                    const c=getContext("2d"), w=width/3, h=height/3
                    c.reset(); c.scale(3,3)
                    function box(x,y,a,b,color){c.fillStyle=color;c.fillRect(Math.round(x),Math.round(y),Math.round(a),Math.round(b))}
                    box(0,0,w,h,"#eedab8")
                    for(let y=30;y<h;y+=16)for(let x=0;x<w;x+=16){
                        box(x,y,15,15,(Math.floor(x/16)+Math.floor(y/16))%2?"#fff0cf":"#f6e6c2")
                        box(x+1,y+1,13,1,"#fff8e4")
                    }
                    box(0,0,w,26,"#cb6877");box(0,2,w,3,"#ef9b9d");box(0,23,w,5,"#884a60")
                    box(0,28,w,3,"#c1a483")
                    // Framed windows, wall lights and the unmistakable counter silhouette.
                    for(const x of [22,115]){
                        box(x,8,39,17,"#783f54");box(x+2,10,35,13,"#a0d5d6")
                        box(x+4,11,12,11,"#d1eeea");box(x+18,10,2,13,"#f6d8bf")
                    }
                    box(54,49,128,25,"#ad596d");box(53,48,130,9,"#ffe2c3")
                    box(57,58,121,2,"#e98a95");box(58,72,120,4,"#b3977c")
                    box(94,58,32,14,"#d77787");box(108,61,5,8,"#fff0d6");box(105,63,11,4,"#fff0d6")
                    // PC at the left wall, plant and a soft red entrance rug.
                    box(13,41,24,30,"#827798");box(15,43,20,14,"#eee6cf");box(17,45,16,10,"#5da8bd")
                    box(19,47,12,2,"#b6f3e7");box(14,59,22,4,"#b7b2bf");box(17,64,17,6,"#665b7b")
                    box(32,h-24,13,12,"#ba735f");box(29,h-35,19,13,"#5b9d70");box(34,h-40,10,14,"#85ba78")
                    box(71,h-27,98,27,"#c46678");box(74,h-25,92,22,"#e19899")
                    box(77,h-22,86,2,"#f2bfaa");box(0,h-3,w,3,"#b09980")
                }
            }
            Item {
                id: nurse; objectName: "center-nurse"; x: 270; y: 60; width: 99; height: 99
                property real greeting: 0
                SequentialAnimation on greeting {
                    running: root.active && !Theme.reducedMotion; loops: Animation.Infinite
                    PauseAnimation { duration: 1700 }
                    NumberAnimation { to: 1; duration: 230 }
                    NumberAnimation { to: 0; duration: 250 }
                    PauseAnimation { duration: 2800 }
                }
                Canvas {
                    width: 33; height: 33; scale: 3; smooth: false; transformOrigin: Item.TopLeft
                    y: nurse.greeting*2
                    onPaint: {
                        const c=getContext("2d");c.reset()
                        function p(x,y,w,h,color){c.fillStyle=color;c.fillRect(x,y,w,h)}
                        p(6,10,21,14,"#9c4466");p(3,13,5,10,"#d97292");p(25,13,5,10,"#d97292")
                        p(9,8,15,14,"#ffd5b5");p(7,8,19,5,"#d97292")
                        p(12,15,2,3,"#584158");p(20,15,2,3,"#584158");p(16,19,3,1,"#bb6c7b")
                        p(8,22,18,9,"#f5eee1");p(13,22,8,3,"#df8fa6");p(15,26,4,5,"#df8fa6")
                        p(5,25,4,5,"#ffd5b5");p(25,25,4,5,"#ffd5b5")
                        p(9,4,15,6,"#fff7e6");p(7,8,19,3,"#fff7e6")
                        p(14,5,2,2,"#df6c89");p(18,5,2,2,"#df6c89");p(15,7,4,2,"#df6c89")
                    }
                }
            }
            Rectangle {
                x: 392; y: 103; width: 142; height: 70; radius: 8; color: "#a8cbcb"; border.width: 3; border.color: "#527785"
                Rectangle { x: 6; y: 6; width: parent.width-12; height: parent.height-12; radius: 4; color: "#345c70" }
                Grid {
                    x: 23; y: 12; columns: 3; spacing: 12
                    Repeater {
                        model: 6
                        Rectangle {
                            required property int index
                            width: 24; height: 19; radius: 9
                            color: index>=root.clinic.partyCount ? "#638393" : root.treating && (root.pulse+index)%3===0 ? "#fff3ac" : root.clinic.treatment==="done" ? "#b9efa9" : "#f29094"
                            border.color: "#253e57"; border.width: 2
                            Rectangle { x: 3; y: 9; width: 18; height: 2; color: "#52707f" }
                            Rectangle { x: 10; y: 7; width: 5; height: 5; radius: 2; color: "#fff6e0" }
                        }
                    }
                }
            }
            Row {
                x: 115; y: Math.max(220,room.height-112); spacing: 4
                Repeater {
                    model: root.team
                    Item {
                        required property var modelData
                        width: 68; height: 73
                        SpritePreview { anchors.fill: parent; asset: modelData.sprite || ({}); playing: root.active; pixelScale: 3; trimTransparentMargins: true }
                    }
                }
            }
            MountedPanel {
                x: parent.width*0.64; y: 30; width: parent.width-x-22; height: parent.height-60; color: "#fff3da"
                Text { x: 22; y: 18; text: "Welcome!"; color: "#a34e68"; font.family: Theme.displayFamily; font.pixelSize: 28 }
                Text { objectName: "clinic-message"; x: 22; y: 63; width: parent.width-44; height: 104; text: root.clinic.clinicMessage; wrapMode: Text.WordWrap; color: Theme.ink; font.pixelSize: 18 }
                Text { x: 22; anchors.bottom: action.top; anchors.bottomMargin: 15; width: parent.width-44; text: root.clinic.treatment==="ready" && root.clinic.canHeal ? "HP · status · move PP\nA backup is kept before treatment." : ""; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                CapButton {
                    id: action; objectName: "clinic-action"; x: 18; anchors.bottom: parent.bottom; anchors.bottomMargin: 17; width: parent.width-36; height: 49
                    label: root.clinic.busy ? "One moment…" : root.clinic.treatment==="ready" && root.clinic.canHeal ? "Heal my team" : "Back to my team"
                    enabled: !root.clinic.busy; selected: root.active && enabled; tint: Theme.pink
                    onActivated: if(root.clinic.treatment==="ready" && root.clinic.canHeal)root.clinic.heal();else root.clinic.back()
                }
            }
        }
    }
}
