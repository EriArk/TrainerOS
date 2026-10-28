import QtQuick

Item {
    id:root
    required property var shell
    readonly property var account:shell.backendAccount
    readonly property var shop:account.shop
    readonly property var selected:shop.selected||({})
    readonly property var review:shop.review||({})
    readonly property bool reviewing:!!review.id
    readonly property bool compact:height<450
    ProfileScene {
        id:scene;width:parent.width*.43;height:parent.height
        profile:root.shell.trainer;appearance:root.selected.appearance||"";showAvatar:root.visible
        Rectangle { anchors.bottom:parent.bottom;width:parent.width;height:70;color:"#bb12242b";visible:!!root.selected.appearance }
        Column { x:16;anchors.bottom:parent.bottom;anchors.bottomMargin:14;spacing:3
            Text { text:root.selected.title||"Classic";font.family:Theme.displayFamily;font.pixelSize:root.compact?24:30;color:root.selected.appearance?"#fff5de":Theme.ink }
            Text { text:root.selected.equipped?"YOUR CURRENT SCENE":"PREVIEW WITH YOUR CHARACTER";font.pixelSize:9;font.letterSpacing:1;color:root.selected.appearance?"#dbe5e0":Theme.muted }
        }
    }
    Item {
        x:scene.width+24;width:parent.width-x;height:parent.height
        Text { id:title;width:parent.width-balance.width-12;text:root.reviewing?"Make it yours":"A world of your own";wrapMode:Text.WordWrap;font.family:Theme.displayFamily;font.pixelSize:root.compact?23:28;color:Theme.ink }
        Row { id:balance;anchors.right:parent.right;y:5;spacing:5
            SparkIcon { width:20;height:20 }
            Text { text:root.shell.profileAccount.sparksText;font.pixelSize:16;color:Theme.ink }
        }
        Text { id:description;y:title.height+10;width:parent.width;text:root.reviewing?"One purchase. Yours on every device.":"Choose a scene, try it with your character, then apply it.";wrapMode:Text.WordWrap;font.pixelSize:11;color:Theme.muted }
        ListView {
            id:list;y:description.y+description.height+14;width:parent.width;height:Math.max(80,parent.height-y-110);spacing:5;clip:true
            visible:!root.reviewing;model:root.shop.items||[]
            onCurrentIndexChanged:positionViewAtIndex(currentIndex,ListView.Contain)
            currentIndex:root.shop.index||0
            delegate:Item {
                required property var modelData;required property int index
                width:ListView.view.width;height:root.compact?49:64
                Rectangle { anchors.fill:parent;radius:7;color:Theme.selection;visible:index===root.shop.index;opacity:.7 }
                Rectangle { x:0;y:8;width:3;height:parent.height-16;radius:1;color:Theme.focus;visible:index===root.shop.index }
                ProfileScene { x:10;y:4;width:root.compact?39:46;height:parent.height-8;radius:4;profile:root.shell.trainer;appearance:modelData.appearance||"";thumbnail:true;showAvatar:false }
                Text { x:root.compact?61:68;y:root.compact?7:13;width:parent.width-x-75;text:modelData.title;textFormat:Text.PlainText;color:Theme.ink;font.pixelSize:14;elide:Text.ElideRight }
                Text { x:root.compact?61:68;y:root.compact?27:35;text:modelData.equipped?"Applied":modelData.owned?"In your collection":"Permanent scene";color:Theme.muted;font.pixelSize:10 }
                Text { anchors.right:parent.right;anchors.rightMargin:10;anchors.verticalCenter:parent.verticalCenter;text:modelData.owned?"✓":modelData.price+" ✦";color:Theme.ink;font.pixelSize:12 }
                MouseArea { anchors.fill:parent;enabled:!root.shop.busy;onClicked:root.account.selectShopItem(index) }
            }
        }
        Column {
            y:description.y+description.height+22;width:parent.width;spacing:14;visible:root.reviewing
            Text { width:parent.width;text:root.review.title||"";textFormat:Text.PlainText;font.family:Theme.displayFamily;font.pixelSize:24;color:Theme.ink }
            Text { width:parent.width;text:root.review.description||"";textFormat:Text.PlainText;wrapMode:Text.WordWrap;font.pixelSize:12;color:Theme.muted }
            Row { spacing:8;SparkIcon {width:28;height:28} Text {text:(root.review.price||0)+" Sparks";font.pixelSize:22;color:Theme.ink} }
            Text { width:parent.width;text:"Permanent · Non-transferable\nNo subscription required. XP is unchanged.";wrapMode:Text.WordWrap;font.pixelSize:11;color:Theme.muted }
        }
        Item {
            anchors.bottom:parent.bottom;width:parent.width;height:100
            Text { width:parent.width;height:47;text:root.shop.status||"";textFormat:Text.PlainText;font.pixelSize:10;color:Theme.muted;wrapMode:Text.WordWrap;maximumLineCount:3;elide:Text.ElideRight }
            Rectangle { y:53;width:parent.width;height:1;color:Theme.divider }
            Row {
                anchors.bottom:parent.bottom;width:parent.width;spacing:8
                Repeater { model:root.reviewing?["Keep browsing","Buy for "+root.review.price+" Sparks"]:["Refresh",root.shop.action||"Sign in"]
                    delegate:Item {
                        required property string modelData;required property int index
                        width:(parent.width-8)/2;height:36
                        readonly property bool highlighted:root.reviewing?root.shell.profileFocus===index+3:root.shell.profileFocus===index
                        Rectangle { anchors.fill:parent;radius:5;color:parent.highlighted?Theme.selection:"transparent";border.width:parent.highlighted?1:0;border.color:Theme.focus }
                        Text { anchors.centerIn:parent;width:parent.width-12;text:modelData;font.pixelSize:11;color:Theme.ink;horizontalAlignment:Text.AlignHCenter;elide:Text.ElideRight }
                        MouseArea { anchors.fill:parent;enabled:!root.shop.busy;onClicked:{if(root.reviewing){if(index===0)root.account.cancelPurchaseReview();else root.account.confirmPurchase()}else{if(index===0)root.account.refreshShop();else root.account.activateShopItem()}} }
                    }
                }
            }
        }
    }
}
