import QtQuick

Item {
    id: root
    required property var shell
    readonly property var controller: shell.outfits
    readonly property var view: controller.view
    readonly property bool focused: !shell.menuOpen && !shell.keyboard.open
    readonly property bool compact: height < 390
    readonly property var selected: view.selected || ({})
    readonly property var review: view.review || ({})
    readonly property color accent: Theme.dark ? "#b9cfa5" : "#527b59"
    property bool pendingAdmire: false
    objectName: "profile-outfits"
    Connections {
        target: root.controller
        function onAdmired() { root.pendingAdmire=true; admireTimer.restart() }
        function onRotateRequested(degrees) { if (portrait.item) portrait.item.yaw+=degrees }
    }
    Timer { id: admireTimer; interval: 180; repeat: true; onTriggered: {
        if (!root.view.previewBusy && portrait.item && portrait.item.ready) {
            if(root.pendingAdmire) portrait.item.admire()
            root.pendingAdmire=false;stop()
        }
    } }

    Item {
        id: stage
        width: Math.round(parent.width*.49); height: parent.height
        ListView {
            id: sets; objectName: "wardrobe-sets"
            width: parent.width; height: 46; orientation: ListView.Horizontal; spacing: 5; clip: true
            model: root.view.rows.length; currentIndex: root.view.index
            onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Rectangle {
                required property int index
                readonly property var record: root.view.rows[index]
                width: record.buy ? 42 : Math.min(126,sets.width*.44); height: 40; radius: 12
                color: index===root.view.index ? Theme.selection : Theme.surface
                border.width: index===root.view.index ? 2 : 0
                border.color: root.focused && root.view.focus===0 ? Theme.focus : root.accent
                Text { anchors.fill: parent; anchors.margins: 8; text: parent.record.name; textFormat: Text.PlainText; font.pixelSize: parent.record.buy?23:12; color: Theme.ink; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                MouseArea { anchors.fill: parent; onClicked: root.controller.select(index) }
                Accessible.role: Accessible.PageTab; Accessible.name: record.buy?"Add a set":record.name; Accessible.selected: index===root.view.index
            }
        }
        Rectangle {
            x: 0; y: 50; width: parent.width; height: parent.height-y-39; radius: 28
            gradient: Gradient { GradientStop {position:0;color:Theme.dark?"#25332e":"#e7eee2"} GradientStop {position:1;color:Theme.dark?"#18231f":"#f4eee2"} }
            Rectangle { x: parent.width*.13; y: 15; width: parent.width*.74; height: parent.height-48; radius: width/2; color: Theme.dark?"#30443b":"#f8f7ec"; border.width: 3; border.color: Theme.dark?"#45594d":"#d4dec9" }
            Rectangle { width:parent.width*.68;height:24;radius:width/2;anchors.horizontalCenter:parent.horizontalCenter;anchors.bottom:parent.bottom;anchors.bottomMargin:14;color:Theme.dark?"#35493c":"#d2dcc5" }
            Loader {
                id: portrait; anchors.fill: parent; anchors.topMargin: -10; anchors.bottomMargin: 10
                active: root.visible && !!root.view.preview && root.shell.trainer.avatarRendererAvailable
                source: active?"AvatarPreview3D.qml":""
                onLoaded: {
                    item.modelSource=Qt.binding(function(){return root.view.preview})
                    item.poseHost=root.controller
                    item.closeup=Qt.binding(function(){return root.view.category>=5})
                }
            }
            Text { anchors.centerIn: parent; width: parent.width-30; visible: !portrait.active || !!root.view.previewError; text:root.view.previewError?"Preview unavailable": "Preparing your look...";wrapMode:Text.WordWrap;horizontalAlignment:Text.AlignHCenter;font.pixelSize:12;color:Theme.muted }
            Row { anchors.bottom: parent.bottom; anchors.bottomMargin: 6; anchors.horizontalCenter:parent.horizontalCenter; spacing:12
                SocialIconButton { icon:"arrow-left";label:"Rotate left";onClicked:if(portrait.item)portrait.item.yaw-=30 }
                SocialIconButton { icon:"arrow-right";label:"Rotate right";onClicked:if(portrait.item)portrait.item.yaw+=30 }
            }
        }
        Row { anchors.bottom:parent.bottom; width:parent.width;spacing:4
            Repeater { model:["Rename","Sync","Admire"]
                delegate: Rectangle {
                    required property string modelData;required property int index
                    width:(stage.width-8)/3;height:32;radius:8;color:root.view.focus===5&&root.view.command===index?Theme.selection:"transparent"
                    border.width:root.focused&&root.view.focus===5&&root.view.command===index?2:0;border.color:Theme.focus
                    Text { anchors.centerIn:parent;text:modelData;font.pixelSize:11;color:root.accent }
                    MouseArea {anchors.fill:parent;onClicked:root.controller.activate(index===0?2:index===1?1:3)}
                }
            }
        }
    }
    Item {
        id: catalogue;x:stage.width+18;width:parent.width-x-61;height:parent.height
        Text { id:title;y:2;text:root.view.categories[root.view.category].name;font.family:Theme.displayFamily;font.pixelSize:root.compact?23:28;color:Theme.ink }
        Text { anchors.right:parent.right;y:12;text:root.view.items.length+" items";font.pixelSize:11;color:Theme.muted }
        GridView {
            id:grid;objectName:"wardrobe-grid";y:44;width:parent.width;height:parent.height-y-126;clip:true
            cellWidth:width/3;cellHeight:height/3;model:root.view.items.length;currentIndex:root.view.item
            onCurrentIndexChanged:positionViewAtIndex(currentIndex,GridView.Contain)
            delegate:Item {
                id:tile;required property int index
                readonly property var record:root.view.items[index]
                objectName:"wardrobe-item-"+record.id
                width:grid.cellWidth;height:grid.cellHeight
                Rectangle {
                    anchors.fill:parent;anchors.margins:4;radius:14
                    color:tile.record.equipped?Theme.surfaceColor("#e1ecdc"):Theme.surfaceColor("#f2f0e7")
                    border.width:tile.index===root.view.item?2:1
                    border.color:tile.index===root.view.item?(root.focused&&root.view.focus===2?Theme.focus:root.accent):Theme.divider
                    Image {anchors.fill:parent;anchors.margins:5;anchors.bottomMargin:19;source:root.view.category===7?"":tile.record.thumbnail;fillMode:Image.PreserveAspectFit;asynchronous:true}
                    Rectangle {visible:root.view.category===7;anchors.horizontalCenter:parent.horizontalCenter;y:5;width:Math.max(8,Math.min(42,parent.width-22,parent.height-25));height:width;radius:width/2;color:root.view.category===7?tile.record.id:"transparent"}
                    Text {anchors.bottom:parent.bottom;anchors.bottomMargin:5;anchors.horizontalCenter:parent.horizontalCenter;width:parent.width-10;text:root.view.category===7?"Tone "+(tile.index+1):tile.record.name;font.pixelSize:9;color:Theme.ink;horizontalAlignment:Text.AlignHCenter;elide:Text.ElideRight}
                    Rectangle {visible:tile.record.equipped;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:6;width:19;height:19;radius:10;color:root.accent
                        Text {anchors.centerIn:parent;text:"\u2713";font.pixelSize:12;color:Theme.dark?"#16251b":"#ffffff"}
                    }
                }
                MouseArea {anchors.fill:parent;onClicked:root.controller.selectItem(index);onDoubleClicked:{root.controller.selectItem(index);root.controller.equip()}}
                Accessible.role:Accessible.ListItem;Accessible.name:record.name;Accessible.selected:record.equipped
            }
        }
        Item { id:wardrobePalette;objectName:"wardrobe-palette";y:grid.y+grid.height+7;width:parent.width;height:51
            Text {text:root.view.equippedName==="None"?"Equip an item to choose its colour":root.view.equippedName+" \u00b7 colour";width:parent.width;elide:Text.ElideRight;font.pixelSize:10;color:Theme.muted}
            Row {y:16;spacing:2
                Repeater {model:root.view.colours.length
                    delegate:Item {
                        required property int index;readonly property string tone:root.view.colours[index]
                        objectName:"wardrobe-colour-"+index
                        width:Math.min(30,(wardrobePalette.width-16)/9);height:32
                        Rectangle {anchors.centerIn:parent;width:parent.width-2;height:width;radius:width/2;color:"transparent";border.width:root.view.colour===parent.tone||root.view.focus===3&&root.view.colourIndex===parent.index?2:0;border.color:root.focused&&root.view.focus===3&&root.view.colourIndex===parent.index?Theme.focus:root.accent}
                        Rectangle {anchors.centerIn:parent;width:parent.width-8;height:width;radius:width/2;color:parent.tone;border.width:1;border.color:Theme.divider}
                        MouseArea {anchors.fill:parent;onClicked:root.controller.chooseColour(index)}
                        Accessible.role:Accessible.RadioButton;Accessible.name:"Colour "+tone;Accessible.checked:root.view.colour===tone
                    }
                }
            }
        }
        Rectangle {
            objectName:"wardrobe-equip";y:wardrobePalette.y+wardrobePalette.height+2;width:parent.width;height:35;radius:12
            readonly property bool removable:root.view.chosen.equipped&&root.view.canRemove
            readonly property bool actionable:!root.view.chosen.equipped||removable
            color:Theme.selection;border.width:root.focused&&root.view.focus===4?2:1;border.color:root.focused&&root.view.focus===4?Theme.focus:root.accent;opacity:actionable?1:.6
            Text {anchors.centerIn:parent;text:parent.removable?"Unequip":root.view.chosen.equipped?"Equipped":"Equip";font.pixelSize:13;color:Theme.ink}
            MouseArea {anchors.fill:parent;enabled:parent.actionable;onClicked:root.controller.equip()}
        }
        Text {anchors.bottom:parent.bottom;width:parent.width;maximumLineCount:2;elide:Text.ElideRight;text:root.view.status||"Changes save automatically";textFormat:Text.PlainText;font.pixelSize:9;color:Theme.muted;wrapMode:Text.WordWrap}
    }
    Column {
        id:categories;objectName:"wardrobe-categories";anchors.right:parent.right;y:38;width:52;spacing:2
        Repeater {model:root.view.categories.length
            delegate:Rectangle {
                required property int index;readonly property var record:root.view.categories[index]
                objectName:"wardrobe-category-"+record.id
                width:52;height:Math.min(46,(root.height-50)/8-2);radius:12;color:root.view.category===index?Theme.selection:"transparent"
                border.width:root.focused&&root.view.focus===1&&root.view.category===index?2:0;border.color:Theme.focus
                Image {anchors.horizontalCenter:parent.horizontalCenter;y:1;width:28;height:Math.max(18,parent.height-15);source:"qrc:/wardrobe/"+(["golf-cap","polo-short-sleeve","golf-shorts","crew-socks","golf-shoes-classic","swept","round","skin"])[parent.index]+".png";fillMode:Image.PreserveAspectFit}
                Text {anchors.bottom:parent.bottom;anchors.bottomMargin:2;anchors.horizontalCenter:parent.horizontalCenter;text:parent.record.name;font.pixelSize:8;color:Theme.ink}
                MouseArea {anchors.fill:parent;onClicked:root.controller.selectCategory(index)}
                Accessible.role:Accessible.PageTab;Accessible.name:record.name;Accessible.selected:root.view.category===index
            }
        }
    }
    Rectangle {
        anchors.fill:parent;radius:18;color:Theme.surface;visible:!!root.review.id||root.view.conflict;z:5
        MouseArea {anchors.fill:parent}
        Column {anchors.centerIn:parent;width:Math.min(400,parent.width-36);spacing:18
            Text {width:parent.width;text:root.view.conflict?"This set changed elsewhere":"One more favourite look";font.family:Theme.displayFamily;font.pixelSize:26;color:Theme.ink;wrapMode:Text.WordWrap}
            Text {width:parent.width;text:root.view.conflict?"Your changes are safe on this device. Which version would you like to keep?":"A permanent slot for a complete look. "+(root.review.price||0)+" Sparks. Saving and switching are free.";font.pixelSize:13;color:Theme.muted;wrapMode:Text.WordWrap}
            Row {width:parent.width;spacing:10
                Repeater {model:root.view.conflict?["Use synced set","Keep my changes"]:["Back","Buy slot"]
                    delegate:CapButton {required property string modelData;required property int index;width:(parent.width-10)/2;height:38;label:modelData;selected:root.view.command===index;onActivated:root.view.conflict?root.controller.conflict(index===1):root.controller.confirm(index===1)}
                }
            }
        }
    }
}
