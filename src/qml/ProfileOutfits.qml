import QtQuick

Item {
    id: root
    required property var shell
    readonly property var controller: shell.outfits
    readonly property var view: controller.view
    readonly property var selected: view.selected || ({})
    readonly property var review: view.review || ({})
    readonly property bool confirming: view.replace || !!review.id
    readonly property bool compact: height < 430
    readonly property color accent: Theme.dark ? "#b9cfa5" : "#527b59"
    objectName: "profile-outfits"

    Item {
        id: portrait; width: parent.width*.39; height: parent.height
        Rectangle { anchors.fill: parent; radius: 14; color: Theme.dark ? "#152624" : "#e7eee5" }
        Rectangle { width: parent.width*.68; height: width; radius: width/2; anchors.horizontalCenter:parent.horizontalCenter;y:parent.height*.1;color:Theme.dark?"#253c36":"#d7e5cd" }
        Loader {
            anchors.fill:parent;anchors.bottomMargin:60
            active:root.visible && !!root.view.preview && root.shell.trainer.avatarRendererAvailable
            source:active?"AvatarPreview3D.qml":""
            onLoaded:item.modelSource=Qt.binding(function(){return root.view.preview||""})
        }
        Text { anchors.centerIn:parent;width:parent.width-30;text:root.view.previewBusy?"Preparing preview…":root.view.previewError?"This look needs an available avatar pack.":"";wrapMode:Text.WordWrap;horizontalAlignment:Text.AlignHCenter;font.pixelSize:12;color:Theme.muted }
        Column {
            x:16;anchors.bottom:parent.bottom;anchors.bottomMargin:16;width:parent.width-32;spacing:5
            Text { width:parent.width;text:root.selected.name||"Current look";textFormat:Text.PlainText;font.family:Theme.displayFamily;font.pixelSize:root.compact?22:27;color:Theme.ink;elide:Text.ElideRight }
            Text { width:parent.width;text:root.selected.current||root.view.wearing?"WEARING NOW":root.selected.buy?"ONE SLOT · MANY ADVENTURES":root.selected.saved?"SAVED LOOK":"READY TO SAVE";font.pixelSize:9;font.letterSpacing:1;color:root.accent;elide:Text.ElideRight }
        }
    }
    Item {
        x:portrait.width+24;width:parent.width-x;height:parent.height
        Text { id:title;width:parent.width-95;text:root.confirming?(root.view.replace?"Replace this look?":"Room for another look"):"Ready for anything";font.family:Theme.displayFamily;font.pixelSize:root.compact?24:29;color:Theme.ink;wrapMode:Text.WordWrap }
        Row { anchors.right:parent.right;y:4;spacing:5;SparkIcon {width:20;height:20} Text {text:root.shell.profileAccount.sparksText;font.pixelSize:16;color:Theme.ink} }
        Text { id:intro;y:title.height+8;width:parent.width;text:root.confirming?(root.view.replace?"Your current appearance will replace the saved one. The slot stays yours.":"A permanent slot for a complete look: outfit, colours, face and hairstyle."):"Save complete looks. Switch before a game or from Home while it is running.";font.pixelSize:11;color:Theme.muted;wrapMode:Text.WordWrap }
        ListView {
            id:list;y:intro.y+intro.height+14;width:parent.width;height:Math.max(65,parent.height-y-(root.compact?115:140));clip:true;spacing:3;visible:!root.confirming
            model:root.view.rows||[];currentIndex:root.view.index||0
            onCurrentIndexChanged:positionViewAtIndex(currentIndex,ListView.Contain)
            delegate:Item {
                required property var modelData;required property int index
                readonly property var appearance:((modelData.saved||{}).recipe||{}).appearance||({})
                width:ListView.view.width;height:root.compact?44:53
                Rectangle { anchors.fill:parent;radius:5;color:index===root.view.index?Theme.selection:"transparent";border.width:index===root.view.index&&root.view.focus===0?1:0;border.color:Theme.focus }
                // Static swatches identify saved looks without rendering a rig in every row.
                Item { x:11;y:9;width:30;height:30
                    Rectangle { x:7;y:0;width:17;height:10;radius:5;color:parent.parent.appearance.hairColor||root.accent;visible:!modelData.buy }
                    Rectangle { x:3;y:11;width:25;height:11;radius:4;color:parent.parent.appearance.shirtColor||Theme.muted;visible:!modelData.buy }
                    Rectangle { x:6;y:23;width:19;height:7;radius:2;color:parent.parent.appearance.shortsColor||root.accent;visible:!modelData.buy }
                    Text { anchors.centerIn:parent;visible:!!modelData.buy;text:"+";font.pixelSize:28;color:root.accent }
                }
                Text { x:52;y:7;width:parent.width-x-65;text:modelData.name;textFormat:Text.PlainText;font.pixelSize:13;color:Theme.ink;elide:Text.ElideRight }
                Text { x:52;y:26;width:parent.width-x-65;text:modelData.current?"Always available":modelData.buy?"Permanent slot":modelData.saved?"Complete appearance":"Save your current appearance here";font.pixelSize:9;color:Theme.muted;elide:Text.ElideRight }
                Text { anchors.right:parent.right;anchors.rightMargin:10;anchors.verticalCenter:parent.verticalCenter;text:modelData.buy?modelData.price+" ✦":"";font.pixelSize:12;color:root.accent }
                MouseArea { anchors.fill:parent;enabled:!root.view.busy;onClicked:root.controller.select(index) }
            }
        }
        Column { y:intro.y+intro.height+22;width:parent.width;spacing:14;visible:root.confirming
            Text { width:parent.width;text:root.view.replace?root.selected.name:(root.review.price||0)+" Sparks";textFormat:Text.PlainText;font.pixelSize:25;color:Theme.ink;elide:Text.ElideRight }
            Text { width:parent.width;text:root.view.replace?"The currently worn look will not change.":"Pay once. Rename and overwrite as often as you like. The slot follows your TrainerOS account.";wrapMode:Text.WordWrap;font.pixelSize:12;color:Theme.muted }
        }
        Column {
            anchors.bottom:parent.bottom;width:parent.width;spacing:6
            Text { width:parent.width;text:root.view.status||"";textFormat:Text.PlainText;wrapMode:Text.WordWrap;maximumLineCount:2;elide:Text.ElideRight;font.pixelSize:10;color:Theme.muted }
            Rectangle { width:parent.width;height:1;color:Theme.divider }
            Flow { width:parent.width;spacing:4
                Repeater { model:root.confirming?["Keep browsing",root.view.replace?"Replace":"Buy for "+root.review.price+" Sparks"]:(root.view.actions||[])
                    delegate:Item {
                        required property string modelData;required property int index
                        width:(parent.width-4)/2;height:root.compact?29:35
                        Rectangle { anchors.fill:parent;radius:4;color:root.view.focus===index+1?Theme.selection:"transparent";border.width:root.view.focus===index+1?1:0;border.color:Theme.focus }
                        Text { anchors.centerIn:parent;width:parent.width-10;text:modelData;horizontalAlignment:Text.AlignHCenter;font.pixelSize:11;color:root.accent;elide:Text.ElideRight }
                        MouseArea { anchors.fill:parent;enabled:!root.view.busy;onClicked:root.confirming?root.controller.confirm(index===1):root.controller.activate(index) }
                    }
                }
            }
        }
    }
}
