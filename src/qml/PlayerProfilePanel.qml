import QtQuick

Panel {
    id: root
    objectName: "player-profile"
    required property var shell
    readonly property var profile: shell.trainer
    readonly property var record: shell.playerProfile || ({})
    readonly property var account: shell.profileAccount
    readonly property var badges: record.badges || []
    readonly property var latest: record.latestSession || ({})
    readonly property bool browsing: !profile.editing && shell.profileRoute === "summary"
    readonly property bool compact: height < 500
    readonly property color accent: Theme.dark ? "#b9cfa5" : "#527b59"
    patterned: false
    MouseArea { anchors.fill: parent }

    component ProfileAction: Item {
        id: action
        required property int actionIndex
        required property string label
        property bool available: true
        property int textSize: 12
        readonly property bool focused: root.shell.profileContentActive && root.shell.profileFocus === actionIndex
        height: 30
        opacity: available ? 1 : .5
        Rectangle { anchors.fill: parent; radius: 5; color: action.focused ? Theme.selection : pointer.containsMouse ? Theme.surface : "transparent"; border.width: action.focused ? 2 : 0; border.color: Theme.focus }
        Text { anchors.fill: parent; anchors.margins: 5; text: action.label; textFormat: Text.PlainText; color: root.accent; font.pixelSize: action.textSize; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
        MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; enabled: action.available && root.shell.profileContentActive; onClicked: root.shell.activate(action.actionIndex,"profile") }
        Accessible.role: Accessible.Button; Accessible.name: label; Accessible.onPressAction: if(available)root.shell.activate(actionIndex,"profile")
    }
    Item {
        anchors.fill: parent; anchors.margins: 18; visible: !root.profile.editing
        Item {
            id: heading; width: parent.width; height: 34
            Text { text: root.shell.profileRoute === "summary" ? "Player profile" : "\u2039  Back"; font.pixelSize: 17; color: Theme.ink
                MouseArea { anchors.fill: parent; anchors.margins: -4; onClicked: root.shell.pressButton("B") }
            }
            ProfileAction { anchors.right: close.left; width: 88; y: -6; actionIndex: 1; label: "Wardrobe"; visible: root.browsing; available: root.profile.exists && root.profile.avatarAvailable }
            SocialIconButton { id: close; anchors.right: parent.right; y: -6; icon: "x"; label: "Back"; onClicked: root.shell.pressButton("B") }
            Rectangle { y: 30; width: parent.width; height: 1; color: Theme.divider }
        }
        Item {
            id: showcase; y: 42; width: parent.width; height: parent.height-y; visible: root.browsing
            Rectangle {
                id: portrait; width: parent.width*.37; height: parent.height; radius: 12; clip: true
                gradient: Gradient { GradientStop {position:0;color:Theme.dark?"#182536":"#bdd9dc"} GradientStop {position:1;color:Theme.dark?"#293f40":"#dce8ce"} }
                Rectangle { x: parent.width*.60; y: parent.height*.10; width: parent.width*.32; height: width; radius: width/2; color: Theme.dark?"#b8c6ab":"#fff1c9"; opacity:.88 }
                Canvas {
                    anchors.fill: parent
                    property bool night: Theme.dark
                    onNightChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onPaint: {
                        let c=getContext("2d");c.reset();const w=width,h=height
                        c.fillStyle=night?"#293b47":"#96bab3";c.beginPath();c.moveTo(0,h*.47);c.bezierCurveTo(w*.38,h*.12,w*.52,h*.77,w,h*.34);c.lineTo(w,h);c.lineTo(0,h);c.fill()
                        c.fillStyle=night?"#36504b":"#b9cdb1";c.beginPath();c.moveTo(0,h*.73);c.bezierCurveTo(w*.4,h*.4,w*.6,h*.6,w,h*.68);c.lineTo(w,h);c.lineTo(0,h);c.fill()
                    }
                }
                Text { x: 15; y: 13; text: "T R A I N E R"; font.pixelSize: 10; color: Theme.dark?"#d0dfdc":"#466869" }
                Loader {
                    id: avatarView; x: 0; y: 22; width: parent.width; height: parent.height-37
                    active: root.browsing && !!root.profile.avatarPreview && root.profile.avatarRendererAvailable
                    source: active ? "AvatarPreview3D.qml" : ""
                    onLoaded: item.modelSource=Qt.binding(function(){return root.profile.avatarPreview})
                }
                TrainerEmblem { anchors.centerIn: avatarView; width: Math.min(120,avatarView.width*.6); height: width; visible: !avatarView.active; emblem: root.profile.profile.emblem || "compass" }
            }
            Flickable {
                id: factsScroll; x: portrait.width+22; width: parent.width-x; height: parent.height
                contentWidth: width; contentHeight: body.implicitHeight; clip: true
                boundsBehavior: Flickable.StopAtBounds
                function revealFocus() {
                    if (!root.browsing || !root.shell.profileContentActive) return
                    const focus = root.shell.profileFocus
                    const section = focus === 4 ? economy : focus === 3 || focus >= 7 ? awards : focus === 2 || focus === 5 || focus === 6 ? journey : null
                    if (!section) { contentY = 0; return }
                    if (section.y < contentY) contentY = section.y
                    else if (section.y+section.height > contentY+height) contentY = Math.max(0, section.y+section.height-height)
                }
                property int observedFocus: -1
                property string observedRoute: ""
                Connections {
                    target: root.shell
                    function onChanged() {
                        if (factsScroll.observedFocus === root.shell.profileFocus && factsScroll.observedRoute === root.shell.profileRoute) return
                        factsScroll.observedFocus = root.shell.profileFocus
                        factsScroll.observedRoute = root.shell.profileRoute
                        factsScroll.revealFocus()
                    }
                }
                Column {
                id: body; width: factsScroll.width; spacing: root.compact?5:9
                Item {
                    width: parent.width; height: root.compact?42:55
                    Text { objectName: "profile-name"; y: -5; width: parent.width-62; text: root.profile.profile.name || "Your profile"; textFormat: Text.PlainText; font.family: Theme.displayFamily; font.pixelSize: root.compact?28:36; minimumPixelSize:20; fontSizeMode:Text.HorizontalFit; elide:Text.ElideRight; color:Theme.ink }
                    ProfileAction { objectName: "profile-edit"; anchors.right: parent.right; y: -5; width: 55; actionIndex: 0; label: root.profile.exists?"Edit":"Create" }
                    Text { y: root.compact?26:38; width: parent.width; text: root.profile.profile.joined?"Playing since "+root.profile.profile.joined:"Your adventures, your story"; font.pixelSize: 10; color: root.accent; elide:Text.ElideRight }
                }
                Item {
                    id: economy; width: parent.width; height: root.compact?72:88
                    readonly property real split: width*.43
                    Rectangle { y: 3; width: 34; height: 34; radius: 11; color: Theme.dark?"#293f3d":"#e1ecd9"
                        Text { anchors.centerIn: parent; text: root.account.levelText; font.family:Theme.displayFamily; font.pixelSize:23; color:root.accent }
                    }
                    Text { x: 43; y: 1; width: economy.split-x-4; text: "Account level"; color: Theme.ink; font.pixelSize:12; elide:Text.ElideRight }
                    Text { x: 43; y: 20; text: "Trainer Mastery"; color: Theme.muted; font.pixelSize:10 }
                    Text { y: root.compact?43:47; width: economy.split-14; text: root.account.signedIn?"Level not available yet":"Sign in for account progress"; color:Theme.muted; font.pixelSize:10; elide:Text.ElideRight }
                    Rectangle { x: economy.split; height: parent.height; width: 1; color: Theme.divider }
                    Item {
                        x: economy.split+15; width: parent.width-x; height: parent.height
                        SparkIcon { width: 29; height: 29; y: 0 }
                        Text { x: 36; y: -3; text: root.account.sparksText; font.family:Theme.displayFamily; font.pixelSize:28; color:Theme.ink }
                        Text { anchors.right:parent.right;y:6;text:"Sparks";color:Theme.muted;font.pixelSize:12 }
                        Text { y: root.compact?29:31; width: parent.width; text: root.compact?"Balance unavailable":"Balance / support preview unavailable"; color:Theme.muted;font.pixelSize:10;elide:Text.ElideRight }
                        ProfileAction { objectName:"profile-account"; y: root.compact?46:56; width: parent.width; actionIndex:4;label:"TrainerOS account  \u203a"; textSize:10 }
                    }
                }
                Rectangle { width: parent.width; height:1;color:Theme.divider }
                Item {
                    id: awards; width: parent.width; height: root.compact?69:82
                    Text { text:"RetroAchievements";font.pixelSize:14;color:Theme.ink }
                    ProfileAction { objectName:"profile-achievements";anchors.right:parent.right;y:-7;width:64;actionIndex:3;label:"View all \u203a";textSize:10 }
                    Row {
                        y:25;width:parent.width;spacing:10
                        Repeater { model:root.badges
                            delegate: Item {
                                required property var modelData
                                required property int index
                                width:(awards.width-10)/2;height:42
                                Rectangle { anchors.fill:parent;radius:5;color:root.shell.profileFocus===index+7?Theme.selection:"transparent";border.width:root.shell.profileFocus===index+7?2:0;border.color:Theme.focus }
                                Image { id:badge;x:2;y:2;width:34;height:width;source:modelData.icon||"";sourceSize:Qt.size(80,80);asynchronous:true;fillMode:Image.PreserveAspectFit }
                                Text { x:6;y:4;visible:badge.status!==Image.Ready;text:"\u2605";font.pixelSize:25;color:root.accent }
                                Text { x:43;width:parent.width-x;text:modelData.title;textFormat:Text.PlainText;font.pixelSize:11;color:Theme.ink;elide:Text.ElideRight }
                                Text { x:43;y:19;width:parent.width-x;text:modelData.date;font.pixelSize:9;color:Theme.muted;elide:Text.ElideRight }
                                MouseArea { anchors.fill:parent;enabled:root.shell.profileContentActive;onClicked:root.shell.activate(index+7,"profile") }
                            }
                        }
                    }
                    Text { y:28;width:parent.width;visible:!root.badges.length;text:root.record.achievementNote||"No confirmed achievements yet";font.pixelSize:12;color:Theme.muted;wrapMode:Text.WordWrap }
                }
                Item {
                    id: journey;width:parent.width;height:root.compact?90:105
                    Text { text:"Player journey";font.pixelSize:14;color:Theme.ink }
                    ProfileAction { objectName:"profile-story";anchors.right:parent.right;y:-7;width:81;actionIndex:2;label:"All history \u203a";textSize:10 }
                    Item {
                        y:24;width:parent.width;height:root.compact?35:46
                        Rectangle { anchors.fill:parent;radius:5;color:root.shell.profileFocus===6?Theme.selection:"transparent";border.width:root.shell.profileFocus===6?2:0;border.color:Theme.focus }
                        Image { id:lastPhoto;visible:!!root.latest.image;width:70;height:parent.height;source:root.latest.image||"";asynchronous:true;fillMode:Image.PreserveAspectFit }
                        Text { x:lastPhoto.visible?80:0;width:parent.width-x;text:root.latest.game||"Your story starts with a game";textFormat:Text.PlainText;color:Theme.ink;font.pixelSize:12;elide:Text.ElideRight }
                        Text { x:lastPhoto.visible?80:0;y:18;width:parent.width-x;text:root.latest.game?(root.latest.date+" \u00b7 "+root.latest.duration):"Your sessions will appear here";color:Theme.muted;font.pixelSize:10;elide:Text.ElideRight }
                        MouseArea { anchors.fill:parent;enabled:!!root.latest.game&&root.shell.profileContentActive;onClicked:root.shell.activate(6,"profile") }
                    }
                    Text { y:root.compact?64:78;width:parent.width-80;text:(root.record.gamesPlayed??0)+" games  \u00b7  "+(root.record.recordedTime||"\u2014");font.pixelSize:10;color:Theme.muted;elide:Text.ElideRight }
                    ProfileAction { objectName:"profile-screens";anchors.right:parent.right;y:root.compact?56:70;width:80;actionIndex:5;label:"Screens \u203a";textSize:10 }
                }
                }
            }
        }
        GameHubOverview { x:0;y:42;width:parent.width;height:parent.height-y; shell:root.shell; visible:root.shell.profileRoute==="story" }
        AchievementGallery { x:0;y:42;width:parent.width;height:parent.height-y;shell:root.shell;visible:root.shell.profileRoute==="ra" }
        Column {
            x:24;y:64;width:parent.width-48;spacing:16;visible:root.shell.profileRoute==="account"
            Text { text:"TrainerOS account";font.family:Theme.displayFamily;font.pixelSize:28;color:Theme.ink }
            Text { width:parent.width;text:root.shell.backendAccount.detail;textFormat:Text.PlainText;wrapMode:Text.WordWrap;font.pixelSize:15;color:Theme.muted }
            Text { width:parent.width;text:"Mastery, Sparks and supporter top-ups will appear when confirmed account data is available. Your local games and history remain available.";wrapMode:Text.WordWrap;font.pixelSize:13;color:Theme.muted }
            CapButton { objectName:"profile-sign-in";width:Math.min(parent.width,360);height:44;label:root.shell.backendAccount.actionLabel;selected:root.shell.profileContentActive;onActivated:root.shell.activate(0,"profile") }
        }
    }
    Item {
        anchors.fill: parent; visible: root.profile.editing && !root.profile.avatarEditing
        PageHeader { x: 24; y: 18; width: parent.width-90; title: "Edit your profile" }
        SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 18; y: 18; icon: "x"; label: "Cancel editing"; onClicked: root.shell.pressButton("B") }
        TrainerEmblem { x: 30; y: 110; width: 145; height: width; emblem: root.profile.draftEmblem }
        Column {
            x: 205; y: 72; width: parent.width-x-30; spacing: 8
            Repeater {
                model: root.profile.editRows
                delegate: CapButton {
                    required property var modelData; required property int index
                    width: parent.width; height: 48; label: modelData.title; detail: modelData.detail || ""
                    selected: root.profile.focusIndex===index && !root.shell.menuOpen && !root.shell.keyboard.open
                    onActivated: root.shell.activate(index,"profile")
                }
            }
        }
    }
    Text { x: 28; anchors.bottom: parent.bottom; anchors.bottomMargin: 17; width: parent.width-140; text: root.profile.error; visible: text.length>0 && !root.profile.avatarEditing; wrapMode: Text.WordWrap; font.pixelSize: 12; color: Theme.textColor("#853b24") }
    Loader { anchors.fill: parent; active: root.profile.avatarEditing; sourceComponent: Component { AvatarEditor { profile: root.profile; shell: root.shell } } }
}
