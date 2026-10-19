import QtQuick

Item {
    id: root
    required property var social
    readonly property var account: social.account
    property bool active: false
    property bool compact: false
    property bool embeddedRoom: false
    property bool readOnlyChat: false
    property bool filteredFeed: false
    property bool streamChat: false
    readonly property int padControl: active && !embeddedRoom && social.reading ? social.conversationControl : -1
    property var messageRows: social.messages
    function sourceIndex(index) { const row=messageRows[index]; return row && row.sourceIndex !== undefined ? row.sourceIndex : index }
    function visibleIndex(index) { for(let i=0;i<messageRows.length;i++)if(sourceIndex(i)===index)return i;return -1 }
    function escaped(value) { return String(value || "").replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/\n/g,"<br/>") }
    signal menuAnchored(var item)
    signal focusRequested()
    function anchorMenu(item) { menuAnchored(item) }
    Rectangle { id: chatHeading; width: parent.width; height: root.embeddedRoom ? 0 : 45; visible: !root.embeddedRoom; radius: 0; color: "transparent"
        Rectangle { width: parent.width-83; height: parent.height-2; radius: 7; color: "transparent"; border.width: root.padControl===0?2:0; border.color: Theme.chassis }
        AccountAvatar { x: 1; y: 4; width: 34; name: root.social.conversationName; source: root.social.conversationInfo.avatar || "" }
        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; x: 47; y: 5; width: parent.width - 112; text: root.social.conversationName; font.family: Theme.displayFamily; font.pixelSize: 18; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; x: 47; y: 27; width: parent.width - 112; text: root.account.historyBusy ? "Loading messages..." : root.social.conversationInfo.kind === "groups" ? "Group conversation" : root.social.conversationInfo.guild ? "Community channel" : "Personal conversation"; font.pixelSize: 10; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText }
        MouseArea { width: parent.width - 74; height: parent.height; enabled: !root.compact; onClicked: root.social.conversationProfile() }
        SocialIconButton { anchors.right: options.left; y: 6; tint: Theme.green; icon: "phone"; label: "Call"
            highlighted: root.padControl===1
            visible: !root.compact && !root.social.conversationInfo.guild
            enabled: !!root.account.voice && (root.account.voice.available || !!root.account.voice.channel)
            onClicked: { root.anchorMenu(this); root.social.call() } }
        SocialIconButton { id: options; highlighted: root.padControl===2; visible: !root.compact; anchors.right: parent.right; y: 6; icon: "dots-three"; label: "Conversation options"
            onClicked: { root.anchorMenu(this); root.social.contextRow() } }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.chassis; opacity: .18 }
    }
    Rectangle { id: activityStrip; objectName: "active-call-bar"; y: chatHeading.height+3; width: parent.width; height: visible ? 40 : 0; radius: 10; color: Qt.tint(Theme.paper,"#2c92bf9b"); border.color: Qt.tint(Theme.paper,"#6684a88b")
        readonly property var voice: root.account.voice || ({})
        visible: !root.compact && !root.embeddedRoom && !!voice.channel
        readonly property bool connected: voice.state === "connected" || voice.state === "reconnecting"
        Rectangle { x: 12; anchors.verticalCenter: parent.verticalCenter; width: 7; height: 7; radius: 4; color: activityStrip.voice.state==="connected" ? "#548b5d" : Theme.yellow }
        Column { x: 27; anchors.verticalCenter: parent.verticalCenter; width: parent.width-151; spacing: 2
            Text { width: parent.width; text: activityStrip.voice.name || "Voice call"; font.family: Theme.displayFamily; font.pixelSize: 14; color: Theme.ink; elide: Text.ElideRight; textFormat: Text.PlainText }
            Text { width: parent.width; text: activityStrip.voice.state==="connected" ? (activityStrip.voice.muted ? "Microphone off" : "Microphone on") : activityStrip.voice.state==="reconnecting" ? "Reconnecting…" : "Connecting…"; font.pixelSize: 10; color: Theme.muted; elide: Text.ElideRight }
        }
        Row { anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; spacing: 5
            SocialIconButton { objectName: "call-microphone"; icon: "microphone"; label: activityStrip.voice.muted ? "Turn microphone on" : "Mute microphone"; tint: activityStrip.voice.muted ? Theme.paper : Theme.green; highlighted: root.padControl===3; enabled: activityStrip.connected; onClicked: root.social.controlCall("voice-mute")
                Rectangle { anchors.centerIn: parent; width: 22; height: 2; rotation: -45; color: Theme.ink; visible: !!activityStrip.voice.muted }
            }
            SocialIconButton { objectName: "call-output"; glyph: "speaker"; label: activityStrip.voice.deaf ? "Enable call sound" : "Silence call sound"; tint: activityStrip.voice.deaf ? Theme.pink : Theme.paper; highlighted: root.padControl===4; enabled: activityStrip.connected; onClicked: root.social.controlCall("voice-output")
                Rectangle { anchors.centerIn: parent; width: 22; height: 2; rotation: -45; color: Theme.ink; visible: !!activityStrip.voice.deaf }
            }
            SocialIconButton { objectName: "call-end"; icon: "phone"; iconRotation: 135; label: "End call"; tint: Theme.pink; highlighted: root.padControl===5; onClicked: root.social.controlCall("voice-leave") }
        }
    }
    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; id: connectionStatus; y: activityStrip.y + activityStrip.height; width: parent.width; height: visible ? 22 : 0
        visible: !!root.account.status && root.account.status !== "Connected"
        text: root.account.status || ""; color: "#80542a"; font.pixelSize: 11; elide: Text.ElideRight; textFormat: Text.PlainText }
    ListView {
        id: log; objectName: "social-message-log"
        anchors { top: connectionStatus.bottom; topMargin: 7; bottom: fileTray.top; bottomMargin: 8; left: parent.left; right: parent.right }
        clip: true; spacing: 2; model: root.messageRows; currentIndex: root.visibleIndex(root.social.messageIndex)
        onMovementEnded: {
            const first = indexAt(42, contentY + 8)
            if(first >= 0) root.social.retainMessagePosition(root.sourceIndex(first), !root.filteredFeed && atYEnd)
            if(atYBeginning && root.account.historyMore) root.social.earlierMessages()
        }
        function restorePosition() {
            if (moving || dragging) return
            forceLayout()
            if (currentIndex < 0 || currentIndex >= count - 1) positionViewAtEnd()
            else positionViewAtIndex(currentIndex,ListView.Contain)
        }
        onCurrentIndexChanged: Qt.callLater(restorePosition)
        onModelChanged: Qt.callLater(restorePosition)
        onHeightChanged: Qt.callLater(restorePosition)
        Timer {
            interval: 1000; repeat: true
            running: !root.filteredFeed && root.visible && root.active && root.social.conversation && (log.atYEnd || log.contentHeight <= log.height) && root.social.surfaceAvailable
            onTriggered: if (root.account.readTail) root.social.presented(root.account.channel, root.account.readTail)
        }
        header: Item { width: log.width; height: root.account.historyMore ? 25 : 0
            Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; visible: parent.height > 0; text: root.account.historyBusy ? "Loading..." : "Earlier messages"; color: Theme.muted; font.pixelSize: 11 }
            MouseArea { anchors.fill: parent; onClicked: root.social.earlierMessages() }
        }
        delegate: Item {
            id: messageRow
            required property var modelData
            required property int index
            readonly property var previous: index > 0 ? root.messageRows[index-1] : null
            readonly property bool grouped: !!previous && !modelData.system && !previous.system && !!modelData.author && previous.author === modelData.author && !!modelData.timestamp && Math.abs(Date.parse(modelData.timestamp)-Date.parse(previous.timestamp)) < 300000
            width: log.width; height: content.height + (root.streamChat ? 4 : grouped ? 2 : 12)
            Rectangle { anchors.fill: parent; radius: 5; color: root.social.reading && root.padControl<0 && index === log.currentIndex ? Qt.rgba(Theme.rim.r,Theme.rim.g,Theme.rim.b,.15) : "transparent" }
            MouseArea { anchors.fill: parent; onClicked: { root.focusRequested(); root.social.selectMessage(root.sourceIndex(index)) } onPressAndHold: { if (root.compact || root.readOnlyChat) return; root.social.selectMessage(root.sourceIndex(index)); root.anchorMenu(messageRow); if(root.embeddedRoom)root.social.lobbyChatTools();else root.social.options() } }
            AccountAvatar { x: 2; y: 6; width: 28; visible: !root.streamChat && !messageRow.grouped && !modelData.system
                name: modelData.name; source: modelData.avatar || ""
                MouseArea { anchors.fill: parent; enabled: !root.compact && !root.readOnlyChat; onClicked: root.social.profile(modelData.author) }
            }
            Column { id: content; x: root.streamChat ? 3 : modelData.system ? 8 : 40; y: root.streamChat ? 0 : messageRow.grouped ? 0 : 5; width: parent.width - x - 8; spacing: 3
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !root.streamChat && !modelData.system && !messageRow.grouped; width: parent.width; text: modelData.name + (modelData.timestamp ? "   " + Qt.formatDateTime(new Date(modelData.timestamp), "ddd HH:mm") : ""); font.pixelSize: 12; font.bold: true; color: Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText
                    MouseArea { anchors.fill: parent; enabled: !root.compact && !root.readOnlyChat; onClicked: root.social.profile(modelData.author) } }
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; width: parent.width; visible: !!modelData.text || modelData.system
                    text: root.streamChat ? '<b><font color="#5c7d79">'+root.escaped(modelData.name)+':</font></b> '+root.escaped(modelData.text)+(modelData.edited ? " (edited)" : "")
                        : (modelData.text || (modelData.system ? "Conversation updated" : "")) + (modelData.edited ? " (edited)" : "")
                    font.pixelSize: root.streamChat || modelData.system ? 12 : 15; color: modelData.system ? Theme.muted : Theme.ink; wrapMode: Text.Wrap; textFormat: root.streamChat ? Text.StyledText : Text.PlainText }
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !!modelData.callDetail; width: parent.width; text: modelData.callDetail || ""; font.pixelSize: 11; color: Theme.muted; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                Repeater { model: modelData.attachments || []
                    Rectangle { required property var modelData; width: content.width; height: 34; radius: 6; color: "#deedf3"
                        Text { anchors { fill: parent; margins: 8 } text: root.embeddedRoom ? "↓  "+(modelData.filename || "File")+" · "+root.fileSize(modelData.size) : modelData.content_type && modelData.content_type.startsWith("audio/") ? "Voice message · " + (modelData.duration || 0) + " s" : (modelData.filename || "File"); font.pixelSize: 13; color: Theme.ink; textFormat: Text.PlainText; elide: Text.ElideMiddle }
                        MouseArea { anchors.fill: parent; enabled: true; onClicked: { const mime=parent.modelData.content_type || ""; if(!root.embeddedRoom && (mime.startsWith("audio/") || mime.startsWith("image/")))root.social.selectMessage(root.sourceIndex(messageRow.index));else root.social.downloadFile(messageRow.modelData.id,parent.modelData.id) } }
                    }
                }
                Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; visible: !!modelData.delivery; width: parent.width; text: (modelData.delivery || "") + (modelData.retryable ? "  ·  Retry ↻" : ""); font.pixelSize: 12; color: "#80542a"; wrapMode: Text.Wrap; textFormat: Text.PlainText
                    MouseArea { anchors.fill: parent; anchors.topMargin: -4; anchors.bottomMargin: -4; enabled: !!messageRow.modelData.retryable && !root.readOnlyChat; onClicked: root.social.retryMessage(messageRow.modelData.id) }
                }
            }
        }
    }
    Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: log; width: log.width - 40; visible: log.count === 0
        text: root.account.historyBusy ? "Loading messages..." : root.readOnlyChat ? "No spectator messages yet" : "Say hello!"; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; font.pixelSize: 18; color: Theme.muted }
    Rectangle { visible: !!root.account.historyPast; anchors { right: log.right; bottom: log.bottom; margins: 5 } width: 85; height: 28; radius: 8; color: "#e7ddf1"; border.color: "#bdacce"
        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors.centerIn: parent; text: "↓ Latest"; color: Theme.ink; font.pixelSize: 12 }
        MouseArea { anchors.fill: parent; onClicked: root.social.latest() }
    }
    function fileSize(value) { const n=Number(value || 0); return n>=1048576 ? (n/1048576).toFixed(1)+" MB" : n>=1024 ? (n/1024).toFixed(1)+" KB" : n+" B" }
    Column { id: fileTray; anchors { left: parent.left; right: parent.right; bottom: composer.top; bottomMargin: 5 }
        visible: !root.readOnlyChat; height: visible ? Math.min(95,fileList.contentHeight)+(fileStatus.visible?25:0) : 0
        ListView { id: fileList; width: parent.width; height: Math.min(95,contentHeight); clip: true; spacing: 3
            model: root.social.files.selected
            delegate: Rectangle { required property int index; required property var modelData; width: fileList.width; height: 29; radius: 6; color: Qt.tint(Theme.paper,"#227eb9ad")
                Text { x: 8; anchors.verticalCenter: parent.verticalCenter; width: parent.width-39; text: modelData.name+" · "+root.fileSize(modelData.size); color: Theme.ink; font.pixelSize: 12; elide: Text.ElideMiddle; textFormat: Text.PlainText }
                SocialIconButton { anchors.right: parent.right; width: 27; height: 27; icon: "x"; label: "Remove file"; enabled: !root.social.files.busy; onClicked: root.social.files.remove(index) }
            }
        }
        Item { id: fileStatus; width: parent.width; height: visible?25:0; visible: !!root.social.files.status
            Text { x: 3; width: parent.width-32; text: root.social.files.status; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; textFormat: Text.PlainText }
            Rectangle { y: 19; width: (parent.width-32)*root.social.files.progress; height: 2; color: Theme.blue }
            SocialIconButton { anchors.right: parent.right; width: 25; height: 25; icon: "x"; label: "Cancel transfer"; onClicked: root.social.files.cancel() }
        }
    }
    Rectangle { id: composer; visible: !root.readOnlyChat; anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 9 } height: visible ? 40 : 0; radius: 9; color: Theme.paper; border.color: Qt.rgba(Theme.chassis.r,Theme.chassis.g,Theme.chassis.b,.28)
        SocialIconButton { id: attach; highlighted: root.padControl===10; visible: !root.compact; width: visible ? 32 : 0; x: 3; anchors.verticalCenter: parent.verticalCenter; tint: Theme.blue; icon: "plus"; label: "Attach files"; enabled: !root.social.files.busy; onClicked: { root.anchorMenu(this); root.social.attachFile() } }
        Text { renderType: typeof Text.CurveRendering === "number" ? Text.CurveRendering : Text.QtRendering; anchors { left: attach.right; right: emoji.left; margins: 7; verticalCenter: parent.verticalCenter } text: root.social.draft || "Write a message...";
            Rectangle { anchors.fill: parent; anchors.margins: -5; radius: 5; color: "transparent"; border.width: root.padControl===11?2:0; border.color: Theme.chassis } font.pixelSize: 14; color: root.social.draft ? Theme.ink : Theme.muted; elide: Text.ElideRight; textFormat: Text.PlainText
            MouseArea { anchors.fill: parent; anchors.topMargin: -10; anchors.bottomMargin: -10; onClicked: { root.focusRequested(); root.social.compose() } } }
        SocialIconButton { id: emoji; highlighted: root.padControl===12; visible: !root.compact; width: visible ? 32 : 0; anchors { right: mic.left; verticalCenter: parent.verticalCenter } tint: Theme.yellow; icon: "smiley"; label: "Emoji"; onClicked: { root.anchorMenu(this); root.social.emoji() } }
        SocialIconButton { id: mic; highlighted: root.padControl===13; visible: !root.compact && !root.embeddedRoom; width: visible ? 32 : 0; anchors { right: send.left; verticalCenter: parent.verticalCenter } tint: Theme.pink; icon: "microphone"; label: "Record voice message"; enabled: !root.account.voice || !root.account.voice.channel; onClicked: { root.anchorMenu(this); root.social.recordVoice() } }
        SocialIconButton { id: send; objectName: "social-send"; anchors { right: parent.right; rightMargin: 3; verticalCenter: parent.verticalCenter } tint: Theme.green; icon: "paper-plane-right"; label: "Send"; highlighted: root.padControl===14
            enabled: root.account.state === "connected" && !root.social.files.busy && (root.social.draft.trim().length > 0 || root.social.files.selected.length>0); onClicked: root.social.sendDraft() }
    }
}
