import QtQuick

Item {
    id: root
    required property var profile
    required property var shell
    readonly property var rows: profile.avatarRows
    readonly property bool focused: !shell.menuOpen && !shell.keyboard.open
    function name(value) { return ({swept: "Side swept", ponytail: "Ponytail", crop: "Short crop", bob: "Bob", round: "Round", almond: "Almond"})[value] || value }
    function revealSelection() {
        const field = profile.focusIndex
        const item = field < 2 ? parts.itemAt(field) : field < 7 ? colours.itemAt(field-2) : null
        if (!item) return
        const top = item.mapToItem(options, 0, 0).y
        if (top < choicesView.contentY) choicesView.contentY = top
        else if (top+item.height > choicesView.contentY+choicesView.height)
            choicesView.contentY = top+item.height-choicesView.height
    }
    Connections { target: root.profile; function onChanged() { root.revealSelection() } }

    Text { x: 26; y: 17; text: "Appearance"; font.family: Theme.displayFamily; font.pixelSize: 27; color: Theme.ink }
    Text { x: 27; y: 49; text: "A little more you."; font.pixelSize: 12; color: Theme.muted }
    SocialIconButton { anchors.right: parent.right; anchors.rightMargin: 20; y: 18; icon: "x"; label: "Cancel appearance"; onClicked: root.profile.finishAvatar(false) }

    Rectangle {
        id: stage
        x: 24; y: 70; width: root.width * .40 - 28; height: root.height - y - 22
        radius: 18
        gradient: Gradient { GradientStop { position: 0; color: "#e4eee6" } GradientStop { position: 1; color: "#c6ded4" } }
        Rectangle { width: parent.width*.74; height: width; radius: width/2; anchors.horizontalCenter: parent.horizontalCenter; y: 23; color: "#50ffffff" }
        Rectangle { width: parent.width*.65; height: 22; radius: width/2; anchors.horizontalCenter: parent.horizontalCenter; y: parent.height-60; color: "#2e709a88" }
        Loader {
            id: view; anchors.fill: parent; anchors.topMargin: -15; anchors.bottomMargin: 35
            active: !!root.profile.avatarPreview && root.profile.avatarRendererAvailable
            source: active ? "AvatarPreview3D.qml" : ""
            onLoaded: item.modelSource=Qt.binding(function(){return root.profile.avatarPreview})
        }
        TrainerEmblem { anchors.centerIn: parent; width: 100; height: 100; visible: !view.active; emblem: root.profile.draftEmblem }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 12; spacing: 12
            SocialIconButton { icon: "arrow-left"; label: "Rotate left"; onClicked: { if(view.item) view.item.yaw-=30 } }
            Text { text: "Drag to turn"; font.pixelSize: 11; color: Theme.muted; anchors.verticalCenter: parent.verticalCenter }
            SocialIconButton { icon: "arrow-right"; label: "Rotate right"; onClicked: { if(view.item) view.item.yaw+=30 } }
        }
        Text { x: 12; y: 10; width: parent.width-24; text: root.profile.error || (root.profile.avatarError ? "Preview unavailable" : root.profile.avatarBusy ? "Trying on…" : ""); color: Theme.muted; font.pixelSize: 11; wrapMode: Text.Wrap }
    }

    Flickable {
        id: choicesView
        x: root.width*.40+12; y: 70; width: root.width-x-26
        height: root.height-y-61
        clip: true; contentWidth: width; contentHeight: options.height
        boundsBehavior: Flickable.StopAtBounds
        Column {
        id: options
        width: choicesView.width-6; spacing: 4
        Repeater {
            id: parts
            model: root.rows.slice(0,2)
            delegate: Item {
                id: part
                required property var modelData
                required property int index
                readonly property int field: index
                width: options.width; height: index===0 ? 52 : 43
                objectName: "avatar-field-"+field
                Text { text: part.modelData.title; font.pixelSize: 12; font.weight: Font.DemiBold; color: Theme.muted }
                Row {
                    y: 18; width: parent.width; height: parent.height-y; spacing: 7
                    Repeater {
                        model: part.modelData.choices
                        delegate: Rectangle {
                            id: choice
                            required property string modelData
                            readonly property bool chosen: part.modelData.detail===modelData
                            readonly property bool highlighted: chosen && root.focused && root.profile.focusIndex===part.field
                            width: (part.width-7*(part.modelData.choices.length-1))/part.modelData.choices.length; height: parent.height
                            radius: 9; color: chosen ? "#dcebe3" : "#f2f1e9"
                            border.width: highlighted ? 2 : 1; border.color: highlighted ? Theme.focusGlow : chosen ? "#719d8d" : "#e1e4da"
                            Accessible.role: Accessible.RadioButton; Accessible.name: root.name(modelData); Accessible.checked: chosen
                            Row {
                                anchors.centerIn: parent; spacing: 8
                                Canvas {
                                    visible: part.field===1; width: 32; height: 20
                                    onPaint: {
                                        const c=getContext("2d"); c.reset(); c.strokeStyle="#244a42"; c.lineWidth=2; c.fillStyle="#fffef9";
                                        c.beginPath(); c.moveTo(2,10); c.bezierCurveTo(9,choice.modelData==="round"?-4:2,24,0,30,10); c.bezierCurveTo(22,23,9,22,2,10); c.fill(); c.stroke();
                                        c.fillStyle="#749c89"; c.beginPath(); c.ellipse(12,4,10,13); c.fill(); c.fillStyle="#244a42"; c.beginPath(); c.ellipse(15,6,4,8); c.fill();
                                    }
                                }
                                Text { text: root.name(choice.modelData); font.pixelSize: 12; font.weight: choice.chosen ? Font.DemiBold : Font.Normal; color: Theme.ink; anchors.verticalCenter: parent.verticalCenter }
                            }
                            MouseArea { anchors.fill: parent; enabled: !root.profile.saving; onClicked: root.profile.chooseAvatar(part.field,choice.modelData) }
                        }
                    }
                }
            }
        }
        Rectangle { width: parent.width; height: 1; color: "#e1e4da" }
        Repeater {
            id: colours
            model: root.rows.slice(2,7)
            delegate: Item {
                id: colourRow
                required property var modelData
                required property int index
                readonly property int field: index+2
                width: options.width; height: Math.max(24,Math.min(30,(root.height-278)/5))
                objectName: "avatar-field-"+field
                Text { anchors.verticalCenter: parent.verticalCenter; text: colourRow.modelData.title; font.pixelSize: 12; color: Theme.ink }
                Row {
                    anchors.right: parent.right; height: parent.height; spacing: 7
                    Repeater {
                        model: colourRow.modelData.choices
                        delegate: Item {
                            id: swatch
                            required property string modelData
                            readonly property bool chosen: colourRow.modelData.colour===modelData
                            width: 26; height: parent.height
                            Accessible.role: Accessible.RadioButton; Accessible.name: colourRow.modelData.title+" "+modelData; Accessible.checked: chosen
                            Rectangle { anchors.centerIn: parent; width: 25; height: 25; radius: 13; color: "transparent"; border.width: swatch.chosen ? 2 : 0; border.color: root.focused && root.profile.focusIndex===colourRow.field ? Theme.focusGlow : Theme.ink }
                            Rectangle { anchors.centerIn: parent; width: 18; height: 18; radius: 9; color: swatch.modelData; border.width: 1; border.color: "#30000000" }
                            MouseArea { anchors.fill: parent; enabled: !root.profile.saving; onClicked: root.profile.chooseAvatar(colourRow.field,swatch.modelData) }
                        }
                    }
                    // A saved custom colour remains visible even outside the preset palette.
                    Rectangle { visible: colourRow.modelData.choices.indexOf(colourRow.modelData.colour)<0; width: visible?20:0; height: 20; radius: 10; color: colourRow.modelData.colour; border.color: Theme.ink; anchors.verticalCenter: parent.verticalCenter }
                }
            }
        }
        }
    }
    Rectangle {
        x: choicesView.x+choicesView.width-3; y: choicesView.y
        width: 3; height: choicesView.height; radius: 2; color: "#e1e4da"
        visible: choicesView.contentHeight>choicesView.height
        Rectangle { width: parent.width; radius: 2; color: "#719d8d"; height: parent.height*choicesView.visibleArea.heightRatio; y: parent.height*choicesView.visibleArea.yPosition }
    }
    Row {
        x: choicesView.x; y: root.height-49; width: choicesView.width; spacing: 10
        CapButton { objectName: "avatar-field-7"; width: parent.width*.57; height: 30; label: root.profile.saving ? "Saving…" : "Save appearance"; textSize: 13; tint: Theme.green; selected: root.focused && root.profile.focusIndex===7; enabled: !root.profile.saving; onActivated: root.profile.finishAvatar(true) }
        CapButton { objectName: "avatar-field-8"; width: parent.width*.43-10; height: 30; label: "Cancel"; textSize: 13; tint: "#e6e9df"; selected: root.focused && root.profile.focusIndex===8; enabled: !root.profile.saving; onActivated: root.profile.finishAvatar(false) }
    }
}
