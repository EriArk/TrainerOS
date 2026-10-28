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
            ProfileScene {
                id:portrait;width:parent.width*.37;height:parent.height
                profile:root.profile;appearance:root.shell.backendAccount.profileAppearance;showAvatar:root.browsing
                Rectangle { anchors.right:parent.right;anchors.rightMargin:7;y:7;width:105;height:31;radius:6;color:Theme.surface;opacity:.92 }
                ProfileAction { anchors.right:parent.right;anchors.rightMargin:10;y:8;width:98;actionIndex:9;label:"Appearance \u203a";textSize:11 }
            }
            Flickable {
                id: factsScroll; x: portrait.width+22; width: parent.width-x; height: parent.height
                contentWidth: width; contentHeight: body.implicitHeight; clip: true
                boundsBehavior: Flickable.StopAtBounds
                function revealFocus() {
                    if (!root.browsing || !root.shell.profileContentActive) return
                    const focus = root.shell.profileFocus
                    const section = focus === 4 ? economy : focus === 10 ? supportEntry : focus === 3 || (focus >= 7 && focus <= 8) ? awards : focus === 2 || focus === 5 || focus === 6 ? journey : null
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
                    Text { y: root.compact?43:47; width: economy.split-14; text: root.account.levelKnown?root.account.levelXp+" / "+root.account.nextLevelXp+" XP" :root.account.signedIn?"Progress unavailable":"Sign in for account progress"; color:Theme.muted; font.pixelSize:10; elide:Text.ElideRight }
                    Rectangle { y:root.compact?61:68;width:economy.split-14;height:3;radius:1.5;color:Theme.divider;visible:root.account.levelKnown
                        Rectangle { height:parent.height;radius:1.5;color:root.accent;width:parent.width*Math.max(0,Math.min(1,(root.account.levelXp||0)/(root.account.nextLevelXp||1)));Behavior on width { NumberAnimation { duration:300 } } }
                    }
                    Rectangle { x: economy.split; height: parent.height; width: 1; color: Theme.divider }
                    Item {
                        x: economy.split+15; width: parent.width-x; height: parent.height
                        SparkIcon { id:rewardSpark;width: 29; height: 29; y: 0 }
                        Text { x: 36; y: -3; text: root.account.sparksText; font.family:Theme.displayFamily; font.pixelSize:28; color:Theme.ink }
                        Text { anchors.right:parent.right;y:6;text:"Sparks";color:Theme.muted;font.pixelSize:12 }
                        Text { y: root.compact?29:31; width: parent.width; text:rewardFade.running?rewardFlash.text:root.account.stale?"Last confirmed balance · offline":root.account.balanceKnown?"Earned through verified achievements":"Balance unavailable"; color:rewardFade.running?root.accent:Theme.muted;font.pixelSize:10;elide:Text.ElideRight }
                        QtObject { id:rewardFlash;property string text:"" }
                        SequentialAnimation { id:rewardFade;NumberAnimation { target:rewardSpark;property:"scale";to:1.3;duration:180 } NumberAnimation { target:rewardSpark;property:"scale";to:1;duration:250 } PauseAnimation { duration:2500 } }
                        Connections { target:root.shell.backendAccount;function onRewardsEarned(xp,sparks) {rewardFlash.text=(xp>0?"+"+xp+" XP  ·  ":"")+"+"+sparks+" Sparks";rewardFade.restart()} }
                        ProfileAction { objectName:"profile-account"; y: root.compact?46:56; width: parent.width; actionIndex:4;label:"TrainerOS account  \u203a"; textSize:10 }
                    }
                }
                ProfileAction { id:supportEntry;objectName:"profile-support";width:parent.width;actionIndex:10;label:"Support \u00d72 / \u00d73  \u00b7  Compare Sparks \u203a";textSize:11 }
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
        ProfileShop { x:0;y:42;width:parent.width;height:parent.height-y;shell:root.shell;visible:root.shell.profileRoute==="shop" }
        ProfileSupport { x:0;y:42;width:parent.width;height:parent.height-y;shell:root.shell;visible:root.shell.profileRoute==="support" }
        GameHubOverview { x:0;y:42;width:parent.width;height:parent.height-y; shell:root.shell; visible:root.shell.profileRoute==="story" }
        AchievementGallery { x:0;y:42;width:parent.width;height:parent.height-y;shell:root.shell;visible:root.shell.profileRoute==="ra" }
        Item {
            id:rewardAccountPage
            x:24;y:58;width:parent.width-48;height:parent.height-y-28;visible:root.shell.profileRoute==="account"
            readonly property var progress:root.shell.backendAccount.progression
            readonly property var challenge:progress.challenge||({})
            Flickable {
                id:accountScroll
                width:parent.width*.48;height:parent.height;clip:true;contentHeight:accountColumn.height;boundsBehavior:Flickable.StopAtBounds
                function revealAction() {
                    if(root.shell.profileRoute!=="account" || root.shell.profileFocus>=4)return
                    var item=accountActions.itemAt(root.shell.profileFocus)
                    if(!item)return
                    var top=item.mapToItem(accountColumn,0,0).y
                    if(top<contentY)contentY=top
                    else if(top+item.height>contentY+height)contentY=Math.min(Math.max(0,contentHeight-height),top+item.height-height)
                }
                Connections { target:root.shell;function onChanged(){Qt.callLater(accountScroll.revealAction)} }
                onHeightChanged:Qt.callLater(revealAction)
                Column { id:accountColumn;width:parent.width;spacing:10
                    Text { text:"Account & rewards";font.family:Theme.displayFamily;font.pixelSize:25;color:Theme.ink }
                    Text { width:parent.width;text:root.shell.backendAccount.detail;textFormat:Text.PlainText;wrapMode:Text.WordWrap;font.pixelSize:12;color:Theme.muted }
                    Text { width:parent.width;text:root.shell.backendAccount.rewardStatus;textFormat:Text.PlainText;wrapMode:Text.WordWrap;font.pixelSize:12;color:Theme.muted }
                    Text { width:parent.width;text:"RA rewards use your Web API key on the TrainerOS server to verify achievements. The key is not stored there. New achievements after linking earn rewards; old imported history does not.";wrapMode:Text.WordWrap;font.pixelSize:11;color:Theme.muted }
                    Text { width:parent.width;visible:!!rewardAccountPage.challenge.challenge;text:"Add this code as a separate word to your RA profile motto, save, then verify. You may remove it afterwards.\n"+(rewardAccountPage.challenge.challenge||"");textFormat:Text.PlainText;wrapMode:Text.WrapAnywhere;font.pixelSize:12;color:root.accent }
                    Repeater { id:accountActions;model:[root.shell.backendAccount.actionLabel,root.shell.backendAccount.rewardActionLabel,"RetroAchievements settings","Copy profile code"]
                        delegate: Item {
                            required property int index;required property string modelData
                            width:accountColumn.width;height:visible?34:0;visible:index!==3||!!rewardAccountPage.challenge.challenge
                            Rectangle { anchors.fill:parent;radius:4;color:root.shell.profileFocus===index?Theme.selection:"transparent";border.width:root.shell.profileFocus===index?1:0;border.color:Theme.focus }
                            Text { anchors.left:parent.left;anchors.leftMargin:8;anchors.verticalCenter:parent.verticalCenter;width:parent.width-16;text:modelData;font.pixelSize:12;color:Theme.ink;elide:Text.ElideRight }
                            MouseArea { anchors.fill:parent;onClicked:root.shell.activate(index,"profile") }
                        }
                    }
                    Text { width:parent.width;text:"Profile appearances are available from your character card. Support bonuses are not available yet.";wrapMode:Text.WordWrap;font.pixelSize:10;color:Theme.muted }
                }
            }
            Rectangle { x:parent.width*.51;width:1;height:parent.height;color:Theme.divider }
            Column { x:parent.width*.54;width:parent.width-x;height:parent.height;spacing:10
                Text { text:"Recent activity";font.pixelSize:17;color:Theme.ink }
                Text { text:root.account.balanceKnown?root.account.sparksText+" Sparks  ·  "+root.account.xp+" XP":"Sign in to view account progress";font.pixelSize:12;color:root.accent }
                ListView { id:rewardHistory;width:parent.width;height:parent.height-65;clip:true;spacing:6;model:root.shell.backendAccount.progression.history||[]
                    Connections { target:root.shell;function onChanged(){if(root.shell.profileRoute==="account" && root.shell.profileFocus>=4)rewardHistory.positionViewAtIndex(root.shell.profileFocus-4,ListView.Contain)} }
                    delegate: Item { required property var modelData;required property int index;width:ListView.view.width;height:55
                        Rectangle { anchors.fill:parent;radius:4;color:Theme.selection;visible:root.shell.profileFocus===index+4 }
                        Text { width:parent.width;text:modelData.title;textFormat:Text.PlainText;font.pixelSize:12;color:Theme.ink;elide:Text.ElideRight }
                        Text { y:19;width:parent.width;text:modelData.source==="support.topup"?"+"+modelData.sparks+" Sparks \u00b7 Support \u00d7"+modelData.support_multiplier:modelData.source==="shop.purchase"?modelData.sparks+" Sparks \u00b7 Purchase":(modelData.source==="ra.completion"?"Complete set · ":"")+"+"+modelData.xp+" XP · +"+modelData.sparks+" Sparks";font.pixelSize:11;color:root.accent;elide:Text.ElideRight }
                        Text { y:35;text:new Date(modelData.earned_at).toLocaleDateString();font.pixelSize:10;color:Theme.muted }
                    }
                    Text { visible:parent.count===0;width:parent.width;text:"No earned rewards yet";font.pixelSize:12;color:Theme.muted }
                }
            }
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
