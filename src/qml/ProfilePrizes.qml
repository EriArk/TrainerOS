import QtQuick
import QtMultimedia

Item {
    id:root
    objectName:"profile-prizes"
    required property var shell
    readonly property var account:shell.backendAccount
    readonly property var view:account.prizes
    readonly property var selected:view.selected||({})
    readonly property bool active:visible&&shell.profileContentActive
    readonly property bool compact:height<420
    function label(kind){return kind==="clothing"?"Wardrobe reward":kind==="outfit_slot"?"Permanent look slot":kind==="completed"?"All achievements":kind==="hardcore"?"Hardcore distinction":"Game beaten"}
    function pinned(){for(const row of view.shelf||[])if(row.game_id===selected.game_id)return true;return false}
    component Link:Item {
        required property int actionIndex
        required property string text
        property bool allowed:true
        width:125;height:32;opacity:allowed?1:.4
        Rectangle {anchors.fill:parent;radius:5;color:root.shell.profileFocus===actionIndex?Theme.selection:"transparent";border.color:Theme.focus;border.width:root.shell.profileFocus===actionIndex?2:0}
        Text {anchors.centerIn:parent;text:parent.text;color:Theme.ink;font.pixelSize:11}
        MouseArea {anchors.fill:parent;enabled:root.active&&parent.allowed;onClicked:root.shell.activate(parent.actionIndex,"profile")}
    }
    Text {id:title;text:"Hall of fame";color:Theme.ink;font.family:Theme.displayFamily;font.pixelSize:root.compact?26:32}
    Text {y:title.height+5;text:(view.pending||0)>0?view.pending+" rewards waiting to be celebrated":"Your adventures, kept forever";font.pixelSize:12;color:Theme.muted}
    ListView {
        id:list;y:66;width:parent.width*.43;height:parent.height-y-76;clip:true;spacing:3
        model:root.view.items||[];currentIndex:root.view.index||0
        onCurrentIndexChanged:positionViewAtIndex(currentIndex,ListView.Contain)
        delegate:Item {
            required property var modelData;required property int index
            width:list.width;height:59
            Rectangle {anchors.fill:parent;radius:6;color:root.view.index===index?Theme.selection:"transparent";border.color:Theme.focus;border.width:root.shell.profileFocus>=100&&root.view.index===index?2:0}
            PrizeEmblem {x:3;y:3;width:48;height:48;kind:modelData.kind}
            Text {x:60;y:9;width:parent.width-x-8;text:modelData.title;textFormat:Text.PlainText;color:Theme.ink;font.pixelSize:12;elide:Text.ElideRight}
            Text {x:60;y:31;width:parent.width-x-8;text:root.label(modelData.kind);color:Theme.muted;font.pixelSize:10;elide:Text.ElideRight}
            MouseArea {anchors.fill:parent;enabled:root.active;onClicked:root.shell.activate(100+index,"profile")}
        }
        Text {anchors.centerIn:parent;width:parent.width-20;visible:!list.count;text:root.view.connected?"Your first level rewards and verified game trophies will appear here.":"Sign in to restore your rewards.";wrapMode:Text.WordWrap;color:Theme.muted;font.pixelSize:13;horizontalAlignment:Text.AlignHCenter}
    }
    Rectangle {x:list.width+16;y:66;width:1;height:list.height;color:Theme.divider}
    Item {
        x:list.width+34;y:61;width:parent.width-x;height:parent.height-y-76
        PrizeEmblem {id:emblem;anchors.horizontalCenter:parent.horizontalCenter;width:Math.min(parent.width*.48,root.compact?130:185);height:width;kind:root.selected.kind||"beaten"}
        Text {id:name;y:emblem.height+4;width:parent.width;text:root.selected.title||"A place for your victories";textFormat:Text.PlainText;color:Theme.ink;font.pixelSize:root.compact?17:21;horizontalAlignment:Text.AlignHCenter;wrapMode:Text.WordWrap;maximumLineCount:2;elide:Text.ElideRight}
        Text {y:name.y+name.height+9;width:parent.width;text:root.selected.kind==="clothing"?"Earned and kept for you.\nThe prize item is being prepared.":root.selected.kind==="outfit_slot"?"Ready in your wardrobe.\nAdds to your purchased look slots.":root.selected.id?root.label(root.selected.kind)+" · "+String(root.selected.earned_at).slice(0,10):"Level up, finish games and choose\nfive trophies for your profile shelf.";color:Theme.muted;font.pixelSize:12;horizontalAlignment:Text.AlignHCenter;wrapMode:Text.WordWrap}
    }
    Row {
        y:parent.height-70;spacing:12
        Repeater {model:root.view.upcoming||[];delegate:Text {required property var modelData;text:"Lv "+modelData.level+" · Clothing"+(modelData.slot?" + look slot":"");font.pixelSize:10;color:Theme.muted}}
    }
    Row {
        anchors.bottom:parent.bottom;anchors.bottomMargin:22;spacing:6
        Link {actionIndex:0;text:root.view.busy?"Refreshing…":"Refresh"}
        Link {actionIndex:1;text:"Celebrate ("+(root.view.pending||0)+")";allowed:(root.view.pending||0)>0}
        Link {actionIndex:2;text:root.pinned()?"Remove from shelf":"Place on shelf";allowed:!!root.selected.game_id&&root.selected.kind!=="clothing"}
        Link {actionIndex:3;text:"Earlier rewards";allowed:!!root.view.next_before}
        Link {actionIndex:4;text:"Wardrobe"}
    }
    Text {anchors.bottom:parent.bottom;width:parent.width;text:root.view.status||(root.view.stale?"Saved collection · reconnect to refresh":"");color:Theme.muted;font.pixelSize:10;elide:Text.ElideRight}
    Item {
        id:celebration;anchors.fill:parent;visible:root.visible&&!!root.view.celebrating;z:10
        property real reveal:0
        onVisibleChanged:if(visible){
            reveal=0;entrance.restart()
            const state=root.shell.social.account||({})
            if(state.notificationSound!==false&&!state.doNotDisturb)chime.play()
        }else {entrance.stop();chime.stop()}
        SoundEffect {id:chime;source:"qrc:/audio/prize-chime.wav";volume:.3}
        NumberAnimation {id:entrance;target:celebration;property:"reveal";from:0;to:1;duration:1500;easing.type:Easing.OutCubic}
        Rectangle {anchors.fill:parent;radius:12;color:"#111a24"}
        MouseArea {anchors.fill:parent;onClicked:{entrance.complete();celebration.reveal=1}}
        Repeater {
            model:18
            delegate:Rectangle {required property int index;width:2+index%3;height:width;radius:width/2;color:"#f3d588";opacity:(1-celebration.reveal*.65)*.65;x:celebration.width*.5+Math.cos(index*2.4)*celebration.width*.45*celebration.reveal;y:celebration.height*.45+Math.sin(index*2.4)*celebration.height*.4*celebration.reveal}
        }
        Loader {
            x:8;y:12;width:parent.width*.29;height:parent.height-24
            active:celebration.visible&&root.shell.trainer.avatarRendererAvailable&&!!root.shell.trainer.avatarPreview
            source:active?"AvatarPreview3D.qml":""
            onLoaded:{item.modelSource=Qt.binding(function(){return root.shell.trainer.avatarPreview});item.poseHost=root.shell.outfits;item.admire()}
        }
        Column {
            x:parent.width*.28;width:parent.width*.68;y:root.compact?14:25;spacing:root.compact?7:13
            Text {anchors.horizontalCenter:parent.horizontalCenter;text:"A MOMENT TO SHINE";font.pixelSize:10;font.letterSpacing:3;color:"#b2c5c3"}
            Text {anchors.horizontalCenter:parent.horizontalCenter;text:root.view.celebrationCount+" rewards earned";font.family:Theme.displayFamily;font.pixelSize:root.compact?27:35;color:"#fff2d5"}
            PrizeEmblem {anchors.horizontalCenter:parent.horizontalCenter;width:root.compact?110:170;height:width;bright:true;scale:.75+.25*celebration.reveal}
            Text {width:parent.width;text:"Your progress is yours to keep.";color:"#ead7ad";font.pixelSize:15;horizontalAlignment:Text.AlignHCenter}
            Text {width:parent.width;text:{const counts={};for(const row of root.view.waiting||[])counts[root.label(row.kind)]=(counts[root.label(row.kind)]||0)+1;return Object.keys(counts).map(k=>counts[k]+" "+k).join("  ·  ")} color:"#afc2c2";font.pixelSize:11;wrapMode:Text.WordWrap;horizontalAlignment:Text.AlignHCenter}
        }
        Row {
            anchors.bottom:parent.bottom;anchors.bottomMargin:15;anchors.horizontalCenter:parent.horizontalCenter;spacing:30
            Text {text:"B  Later";color:"#b8c9c8";font.pixelSize:13;MouseArea {anchors.fill:parent;anchors.margins:-8;onClicked:root.account.closeCelebration(false)}}
            Text {text:"A  Continue";color:"#ffe4a2";font.pixelSize:13;MouseArea {anchors.fill:parent;anchors.margins:-8;onClicked:root.account.closeCelebration(true)}}
        }
    }
}
