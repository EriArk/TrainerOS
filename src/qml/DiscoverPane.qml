import QtQuick

Item {
    id: root
    required property var social
    property bool takesFocus: false
    readonly property bool compact: height < 380
    readonly property var viewState: social.discover
    readonly property var preview: social.discoverPreview
    readonly property var recent: social.recentPeople
    readonly property bool hasQuery: social.query.length > 0
    function when(value) { return new Date(value*1000).toLocaleDateString(Qt.locale(),"d MMM") }
    Rectangle { anchors.fill: parent; color: Theme.paper }
    Canvas {
        anchors.fill: parent; opacity: .045
        property color ink: Theme.chassis
        onInkChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {const c=getContext("2d");c.reset();c.strokeStyle=ink;c.lineWidth=1.5;for(let r=55;r<width;r+=37){c.beginPath();c.arc(width*.8,height*.7,r,0,Math.PI*2);c.stroke()} }
    }
    Item {
        id: search; x: 20; y: root.compact?6:10; width: parent.width-40; height: root.compact?33:39
        Rectangle { width: parent.width-230; height: parent.height; radius: 9; color: Theme.surfaceColor("#75ffffff"); border.width: root.takesFocus&&root.viewState.area==="search"?2:1; border.color: Qt.tint(Theme.paper,Theme.surfaceColor("#80516860"))
            SocialIconButton { x: 4; y: 3; icon: "magnifying-glass"; label: "Search"; onClicked: root.social.discoverAction("search") }
            Text { x: 41; anchors.verticalCenter: parent.verticalCenter; width: parent.width-x-35; text: root.social.query || "Find people, communities or paste an invite…"; color: root.hasQuery?Theme.ink:Theme.muted; font.pixelSize: 13; elide: Text.ElideRight; textFormat: Text.PlainText }
            MouseArea { x: 37; width: parent.width-x-32; height: parent.height; onClicked: root.social.discoverAction("search") }
            SocialIconButton { anchors.right: parent.right; y: 3; visible: root.hasQuery; icon: "x"; label: "Clear search"; onClicked: root.social.discoverAction("clear") }
        }
        Row { anchors.right: parent.right; height: parent.height; spacing: 20
            Repeater { model: ["People","Communities"]
                Item { required property string modelData; width: label.implicitWidth; height: search.height
                    Text { id: label; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: Theme.ink; font.pixelSize: 15; font.family: Theme.displayFamily }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: Theme.yellow; visible: root.social.searchKind===modelData.toLowerCase() }
                    MouseArea { anchors.fill: parent; onClicked: root.social.setSearchKind(modelData.toLowerCase()) }
                }
            }
        }
    }
    Item {
        id: recentBlock; x: 20; y: search.y+search.height+(root.compact?8:18); width: parent.width-40; height: root.compact?100:139; visible: root.viewState.home
        Text { text: "Recently together"; font.family: Theme.displayFamily; font.pixelSize: root.compact?17:20; color: Theme.ink }
        Text { anchors.right: parent.right; text: "See all ›"; visible: root.recent.length>0; font.pixelSize: 13; color: Theme.muted; MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.social.discoverAction("all") } }
        ListView { id: recentRail; x: 0; y: root.compact?23:31; width: parent.width; height: parent.height-y; orientation: ListView.Horizontal; clip: true; model: root.recent; currentIndex: root.social.recentFocus
            onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
            delegate: Item { required property var modelData; required property int index; width: recentRail.width/6; height: recentRail.height
                Rectangle { anchors.fill: parent; anchors.margins: 2; color: "transparent"; radius: 9; border.width: root.takesFocus&&root.viewState.area==="recent"&&index===root.social.recentFocus?1:0; border.color: Theme.chassis }
                AccountAvatar { anchors.horizontalCenter: parent.horizontalCenter; y: 1; width: root.compact?32:48; name: modelData.name; source: modelData.avatar || "" }
                Text { y: root.compact?34:56; width: parent.width-8; anchors.horizontalCenter: parent.horizontalCenter; text: modelData.name; font.pixelSize: root.compact?12:14; color: Theme.ink; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; textFormat: Text.PlainText }
                Text { y: root.compact?50:75; width: parent.width-8; anchors.horizontalCenter: parent.horizontalCenter; text: modelData.game || ""; font.pixelSize: 10; color: Theme.muted; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter; textFormat: Text.PlainText }
                Text { y: root.compact?62:90; width: parent.width; text: root.when(modelData.last); font.pixelSize: 9; color: Theme.muted; horizontalAlignment: Text.AlignHCenter }
                MouseArea { anchors.fill: parent; onClicked: root.social.discoverAction("recent",index) }
            }
        }
        Item { visible: root.recent.length===0; y: root.compact?25:38; width: parent.width; height: root.compact?68:87
            AccountAvatar { x: 5; y: 12; width: root.compact?36:48; name: "+"; opacity: .7 }
            AccountAvatar { x: root.compact?32:43; y: 4; width: root.compact?36:48; name: "★"; opacity: .9 }
            Text { x: root.compact?90:112; y: 8; text: "Good games bring people together"; font.family: Theme.displayFamily; font.pixelSize: root.compact?16:19; color: Theme.ink }
            Text { x: root.compact?90:112; y: root.compact?32:41; width: parent.width-x-20; text: "Your recent teammates will appear here after you play together."; wrapMode: Text.WordWrap; font.pixelSize: 13; color: Theme.muted }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.chassis; opacity: .15 }
    }
    Item {
        id: content; x: 20; width: parent.width-40; y: root.viewState.home?recentBlock.y+recentBlock.height+(root.compact?8:15):search.y+search.height+19; height: parent.height-y-16
        Text { id: title; x: root.viewState.all?27:0; text: root.viewState.all?"Recently together":root.viewState.home?"Popular TrainerOS communities":"Search results"; font.family: Theme.displayFamily; font.pixelSize: root.compact?17:20; color: Theme.ink }
        Text { x: title.x+title.implicitWidth+18; y: 5; width: Math.max(0,parent.width-x-110); text: root.viewState.status || ""; visible: !root.viewState.all&&root.social.searchResults.length>0; font.pixelSize: 11; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
        SocialIconButton { x: -7; y: -5; visible: root.viewState.all; icon: "arrow-left"; label: "Back to Discover"; onClicked: root.social.discoverAction("back") }
        Row { anchors.right: parent.right; y: -5; spacing: 5; visible: root.viewState.pages>1||root.viewState.more||root.viewState.previous
            SocialIconButton { icon: "arrow-left"; label: "Previous page"; enabled: root.viewState.page>0||root.viewState.previous; onClicked: root.social.discoverAction("previous") }
            Text { anchors.verticalCenter: parent.verticalCenter; text: root.viewState.displayPage+" / "+root.viewState.displayPages; font.pixelSize: 12; color: Theme.muted }
            SocialIconButton { icon: "arrow-left"; iconRotation: 180; label: "Next page"; enabled: root.viewState.page+1<root.viewState.pages||root.viewState.more; onClicked: root.social.discoverAction("next") }
        }
        Grid {
            id: cards; y: root.compact?28:35; width: parent.width; height: parent.height-y; columns: 3; spacing: 12
            visible: !root.viewState.all&&(root.viewState.home||root.social.searchKind==="communities"||root.social.searchResults.length===1&&root.social.searchResults[0].kind==="invite")
            Repeater { model: cards.visible?root.social.searchResults.slice(root.viewState.page*root.viewState.pageSize,(root.viewState.page+1)*root.viewState.pageSize):[]
                DiscoverCommunityCard { required property var modelData; required property int index; entry: modelData; compact: root.compact; width: (cards.width-24)/3; height: root.viewState.home?Math.min(212,cards.height):Math.min(186,(cards.height-12)/2); selected: root.takesFocus&&root.viewState.area==="results"&&root.social.searchFocus===root.viewState.page*root.viewState.pageSize+index; onActivated: root.social.activateSearch(root.viewState.page*root.viewState.pageSize+index) }
            }
        }
        Column { id: people; y: 35; width: parent.width; spacing: 0; visible: !root.viewState.all&&!root.viewState.home&&!cards.visible
            Repeater { model: people.visible?root.social.searchResults.slice(root.viewState.page*6,(root.viewState.page+1)*6):[]
                Item { required property var modelData; required property int index; width: people.width; height: Math.min(58,(content.height-35)/6)
                    Rectangle { anchors.fill: parent; radius: 7; color: Theme.chassis; opacity: root.takesFocus&&root.viewState.area==="results"&&root.social.searchFocus===root.viewState.page*6+index ? .09 : 0 }
                    AccountAvatar { x: 8; anchors.verticalCenter: parent.verticalCenter; width: 35; name: modelData.name; source: modelData.avatar || "" }
                    Text { x: 58; y: 5; width: parent.width-180; text: modelData.name; color: Theme.ink; font.pixelSize: 17; elide: Text.ElideRight; textFormat: Text.PlainText }
                    Text { x: 58; y: 29; text: modelData.tag || modelData.description || ""; color: Theme.muted; font.pixelSize: 11; textFormat: Text.PlainText }
                    Text { anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 13 } text: modelData.kind==="request-tag"?(root.viewState.busy?"Sending…":modelData.action==="Sent"?"Request sent":"Send request ›"):modelData.detail || "Profile ›"; color: Theme.muted; font.pixelSize: 12 }
                    Rectangle { x: 58; anchors.bottom: parent.bottom; width: parent.width-x-12; height: 1; color: Theme.chassis; opacity: .12 }
                    MouseArea { anchors.fill: parent; onClicked: root.social.activateSearch(root.viewState.page*6+index) }
                }
            }
        }
        Grid { id: allRecent; y: 35; width: parent.width; columns: 3; spacing: 12; visible: root.viewState.all
            Repeater { model: allRecent.visible?root.recent.slice(root.viewState.page*6,(root.viewState.page+1)*6):[]
                Item { required property var modelData; required property int index; width: (allRecent.width-24)/3; height: Math.min(115,(content.height-47)/2)
                    Rectangle { anchors.fill: parent; radius: 9; color: "transparent"; border.width: root.takesFocus&&root.social.recentFocus===root.viewState.page*6+index?1:0; border.color: Theme.chassis }
                    AccountAvatar { x: 9; y: root.compact?12:17; width: root.compact?40:51; name: modelData.name; source: modelData.avatar || "" }
                    Column { x: root.compact?58:72; y: root.compact?12:18; width: parent.width-x-9; spacing: root.compact?4:7
                        Text { width: parent.width; text: modelData.name; font.pixelSize: root.compact?15:17; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
                        Text { width: parent.width; text: modelData.game || ""; font.pixelSize: root.compact?11:12; color: Theme.muted; wrapMode: Text.WordWrap; maximumLineCount: root.compact?1:2; elide: Text.ElideRight; textFormat: Text.PlainText }
                        Text { text: root.when(modelData.last); font.pixelSize: 11; color: Theme.muted }
                    }
                    MouseArea { anchors.fill: parent; onClicked: root.social.discoverAction("recent",root.viewState.page*6+index) }
                }
            }
        }
        Item { anchors { top: title.bottom; topMargin: 8; bottom: parent.bottom; left: parent.left; right: parent.right } visible: !root.viewState.all&&root.social.searchResults.length===0
            Item { anchors.centerIn: parent; width: Math.min(parent.width,600); height: root.compact?Math.min(83,parent.height):115
                Item { x: 0; y: 2; width: 112; height: 100; scale: root.compact ? .6 : 1; transformOrigin: Item.TopLeft
                    Rectangle { x: 5; y: 22; width: 75; height: 66; radius: 16; rotation: -10; color: Theme.green; border.color: Qt.lighter(Theme.green,1.2) }
                    Rectangle { x: 37; y: 7; width: 75; height: 66; radius: 16; rotation: 8; color: Theme.blue; border.color: Qt.lighter(Theme.blue,1.2)
                        Text { anchors.centerIn: parent; text: "✦"; font.pixelSize: 33; color: Theme.accentInk }
                    }
                }
                Text { x: root.compact?83:140; y: root.compact?0:8; width: parent.width-x; text: root.viewState.busy?"Looking around…":root.viewState.home?"Your next community is out there":"Keep looking"; font.family: Theme.displayFamily; font.pixelSize: root.compact?17:22; color: Theme.ink; wrapMode: Text.WordWrap; maximumLineCount: 1; elide: Text.ElideRight }
                Text { x: root.compact?83:140; y: root.compact?26:42; width: parent.width-x; text: root.viewState.status || "Search for someone, or join a community with an invitation."; font.pixelSize: root.compact?11:13; color: Theme.muted; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; textFormat: Text.PlainText }
                Text { x: root.compact?83:140; y: root.compact?62:94; text: "Retry ↻"; font.pixelSize: 12; color: Theme.muted; visible: !root.viewState.busy; MouseArea { anchors.fill: parent; anchors.margins: -5; onClicked: root.social.discoverAction("retry") } }
            }
        }
    }
    Rectangle {
        anchors.fill: parent; z: 5; color: Theme.surfaceColor("#6630433b"); visible: !!root.preview.id
        MouseArea { anchors.fill: parent; onClicked: root.social.discoverAction("back") }
        Rectangle {
            anchors.centerIn: parent; width: Math.min(460,parent.width-40); height: Math.min(357,parent.height-20); radius: 15; color: Theme.paper; border.color: Qt.tint(Theme.paper,Theme.surfaceColor("#60516c60")); border.width: 2
            MouseArea { anchors.fill: parent }
            Rectangle { id: previewBanner; x: 7; y: 7; width: parent.width-14; height: parent.height*.32; radius: 10; clip: true; gradient: Gradient { GradientStop { position: 0; color: Theme.chassis } GradientStop { position: 1; color: Theme.blue } }
                Image { anchors.fill: parent; source: root.preview.banner || ""; fillMode: Image.PreserveAspectCrop; asynchronous: true }
            }
            SocialIconButton { anchors { right: parent.right; top: parent.top; margins: 12 } icon: "x"; tint: Theme.paper; label: "Close preview"; onClicked: root.social.discoverAction("back") }
            AccountAvatar { x: 20; y: previewBanner.height-15; width: 58; name: root.preview.name || ""; source: root.preview.avatar || "" }
            Text { x: 91; y: previewBanner.height+17; width: parent.width-x-20; text: root.preview.name || ""; font.family: Theme.displayFamily; font.pixelSize: 22; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
            Text { x: 21; y: previewBanner.height+58; width: parent.width-42; text: (root.preview.members>=0?root.preview.members+" members":"Invitation")+(root.preview.language?" · "+root.preview.language:""); font.pixelSize: 12; color: Theme.muted }
            Text { x: 21; y: previewBanner.height+82; width: parent.width-42; height: parent.height-y-71; text: root.preview.description || "Join this community to meet its members and explore its conversations."; font.pixelSize: 14; wrapMode: Text.WordWrap; maximumLineCount: 4; elide: Text.ElideRight; color: Theme.ink; textFormat: Text.PlainText }
            Text { x: 21; anchors.bottom: join.top; anchors.bottomMargin: 7; width: parent.width-42; text: root.preview.status || ""; font.pixelSize: 11; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
            CapButton { id: join; x: 21; anchors.bottom: parent.bottom; anchors.bottomMargin: 17; width: parent.width-42; height: 36; tint: Theme.green; claimsFocus: false; selected: root.takesFocus; label: root.preview.busy?"Joining…":root.preview.action==="Joined"?"Joined":"Join"; enabled: !root.preview.busy&&root.preview.action!=="Joined"; onActivated: root.social.discoverAction("join") }
        }
    }
}
