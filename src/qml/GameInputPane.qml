import QtQuick

Item {
    id: root
    required property var flow
    required property bool takesFocus
    property int chosenRing: 0
    onVisibleChanged: if(!visible)Theme.controller?.cancelCapture()
    readonly property var ringMeta: flow.draft.ringPresentation?.[currentRing] || ({})
    readonly property int ringCount: root.flow.draft.rings?.length || 0
    function ringStart(index) { let offset=23; for(let i=0;i<index;i++)offset+=root.flow.draft.rings[i].length; return offset }
    readonly property int currentRing: {
        for(let i=0;i<ringCount;i++)if(root.flow.focusIndex>=ringStart(i)&&root.flow.focusIndex<ringStart(i)+root.flow.draft.rings[i].length)return i
        return Math.min(chosenRing,Math.max(0,ringCount-1))
    }
    Text { x: 10; y: 8; width: parent.width-78; text: root.flow.title; color: Theme.textColor("#304d40"); font.pixelSize: 19; font.weight: Font.DemiBold; elide: Text.ElideRight }
    Text { anchors.right: parent.right; anchors.rightMargin: 12; y: 12; text: "Back"; font.pixelSize: 12; color: Theme.textColor("#6c8979"); TapHandler { onTapped: root.flow.back() } }
    Text { visible: !root.flow.editing; x: 10; y: 32; width: parent.width-20; text: root.flow.choosing ? "Choose an action · L2 / R2 changes category" : root.flow.editing ? "Your layout for this game · Home remains available" : "PC and native games · separate profiles for each Trainer"; color: Theme.textColor("#829086"); font.pixelSize: 11 }
    Row { id: tabs; x: 10; y: 42; spacing: 14; visible: root.flow.editing
        Repeater { model: root.flow.choosing ? root.flow.targetGroups : ["Setup","Buttons","Radial menus"]
            Item { required property int index; required property string modelData; width: label.implicitWidth+4; height: 28
                Text { id: label; text: modelData; color: (root.flow.choosing?root.flow.targetGroup:root.flow.page)===index ? Theme.textColor("#37634e") : Theme.textColor("#929c93"); font.pixelSize: 12 }
                Rectangle { y: 23; width: parent.width; height: 2; color: Theme.accent; visible: (root.flow.choosing?root.flow.targetGroup:root.flow.page)===index }
                TapHandler { onTapped: root.flow.choosing ? root.flow.setTargetGroup(index) : root.flow.setPage(index) }
            }
        }
    }
    Flickable {
        id: content; x: 10; y: root.flow.editing?77:60; width: parent.width-20; height: parent.height-y-38; clip: true
        visible: root.flow.copying || !root.flow.editing || root.flow.choosing || root.flow.page===0
        contentHeight: grid.height; boundsBehavior: Flickable.StopAtBounds
        function follow() { const items=root.flow.visibleRows; for(let i=0;i<items.length;i++)if(items[i].index===root.flow.focusIndex){const item=choices.itemAt(i);if(item)contentY=Math.max(0,Math.min(Math.max(0,contentHeight-height),item.y<contentY?item.y:item.y+item.height>contentY+height?item.y+item.height-height:contentY));} }
        Connections { target: root.flow; function onChanged(){Qt.callLater(content.follow)} }
        Grid { id: grid; objectName: "settings-input-grid"; width: parent.width; columns: !root.flow.editing||root.flow.copying?1:root.flow.choosing?(root.flow.targetGroup===2?7:4):2; rowSpacing: 4; columnSpacing: 12
            Repeater { id: choices; model: root.flow.editing&&!root.flow.choosing&&!root.flow.copying&&root.flow.page===0 ? root.flow.visibleRows.filter(r=>r.index<6) : root.flow.visibleRows
                Item { required property var modelData; required property int index; width: (grid.width-(grid.columns-1)*12)/grid.columns; height: root.flow.choosing?36:54
                    readonly property bool selected: root.takesFocus && root.flow.focusIndex===modelData.index
                    onSelectedChanged: if(selected)forceActiveFocus()
                    Rectangle { x: 0; y: 8; width: 2; height: parent.height-16; color: Theme.accent; visible: parent.selected }
                    Text { x: 10; y: 10; width: parent.width-20; text: modelData.title; font.pixelSize: 13; font.weight: parent.selected?Font.DemiBold:Font.Normal; color: Theme.textColor("#355341"); elide: Text.ElideRight }
                    Text { objectName: "settings-input-detail-"+modelData.index; x: 10; y: 32; width: parent.width-20; text: modelData.detail || ""; color: Theme.textColor("#6d7e74"); font.pixelSize: 11; elide: Text.ElideRight }
                    Rectangle { x: 10; y: parent.height-1; width: parent.width-10; height: 1; color: Theme.divider }
                    TapHandler { onTapped: root.flow.activate(modelData.index) }
                }
            }
        }
        Text { anchors.centerIn: parent; visible: !root.flow.editing && !root.flow.rows.length; width: parent.width*.8; text: "Add a PC or native game to Games first.\nIts input profile will appear here."; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: Theme.textColor("#87958a"); font.pixelSize: 14 }
    }
    Item { x: 8; y: 75; width: parent.width-16; height: parent.height-y-38; visible: root.flow.editing&&!root.flow.choosing&&!root.flow.copying&&root.flow.page===1
        ControllerDiagram { x: 0; y: 12; width: parent.width*.63; height: Math.min(parent.height-15,width*.72); selectedButton: root.flow.selectedButton; onPicked: button => root.flow.selectBinding(button) }
        Column { x: parent.width*.68; y: 8; width: parent.width-x; spacing: 12
            Text { text: Theme.buttonLabel(root.flow.selectedButton) || "Choose a button"; color: Theme.textColor("#3d6049"); font.pixelSize: 15; font.weight: Font.DemiBold }
            Text { width: parent.width; text: root.flow.rows[root.flow.focusIndex]?.detail || "Select a control on the diagram"; color: Theme.textColor("#83927e"); font.pixelSize: 13; wrapMode: Text.WordWrap }
            Text { text: "Change assignment →"; color: Theme.textColor("#4c876c"); font.pixelSize: 12; TapHandler { onTapped: root.flow.selectBinding(root.flow.selectedButton || "south") } }
            Text { width: parent.width; text: root.flow.draft.modifier==="none"?"No modifier layer": "Hold "+root.flow.draft.modifier+" for alternate bindings"; color: Theme.textColor("#93a08e"); font.pixelSize: 11; wrapMode: Text.WordWrap }
            Text { text: root.flow.rows[6]?.detail || "Base"; color: root.flow.focusIndex===6?Theme.textColor("#386b50"):Theme.textColor("#7b937d"); font.pixelSize: 12; TapHandler { onTapped: root.flow.activate(6) } }
        }
    }
    Item { x: 8; y: 75; width: parent.width-16; height: parent.height-y-38; visible: root.flow.editing&&!root.flow.choosing&&!root.flow.copying&&root.flow.page===2
        RingDiagram { objectName: "settings-ring-diagram"; width: parent.width*.64; height: parent.height; slots: root.flow.draft.rings?.[root.currentRing] || []; selected: root.ringCount&&root.flow.focusIndex>=root.ringStart(root.currentRing)&&root.flow.focusIndex<root.ringStart(root.currentRing)+slots.length ? root.flow.focusIndex-root.ringStart(root.currentRing) : -1; presentation: root.ringMeta; caption: root.ringMeta.name || (root.ringCount?"Ring "+(root.currentRing+1):"Create a ring"); onPicked: slot => root.flow.activate(root.ringStart(root.currentRing)+slot) }
        Column { x: parent.width*.69; y: 3; width: parent.width-x; spacing: 11
            Row { spacing: 10
                Text { text: "Ring"; color: Theme.textColor("#6c8979"); font.pixelSize: 12 }
                Repeater { model: root.ringCount
                    Text { required property int index; text: index+1; width: 15; font.pixelSize: 14; font.underline: root.currentRing===index; color: root.currentRing===index?Theme.textColor("#386b50"):Theme.textColor("#8b9986"); TapHandler { onTapped: { root.chosenRing=index; root.flow.focusRow(root.ringStart(index)) } } }
                }
            }
            Text { font.underline: root.takesFocus&&root.flow.focusIndex===root.flow.rows.length-5; text: "+ Add ring"; color: Theme.textColor("#4b8464"); font.pixelSize: 12; TapHandler { onTapped: root.flow.activate(root.flow.rows.length-5) } }
            Text { font.underline: root.takesFocus&&root.flow.focusIndex===root.flow.rows.length-4; text: "Remove last ring"; color: Theme.textColor("#9b8876"); font.pixelSize: 11; TapHandler { onTapped: root.flow.activate(root.flow.rows.length-4) } }
            Text { width: parent.width; text: "Hold its button, aim the left stick, release. Center cancels."; color: Theme.textColor("#8b9786"); font.pixelSize: 11; wrapMode: Text.WordWrap }
        }
    }
    Text { x: 10; y: 65; visible: root.flow.editing; text: Theme.buttonLabel("Select")+" · Tools"; color: Theme.accent; font.pixelSize: 10; TapHandler { onTapped: root.flow.openTools() } }
    Text { x: 10; anchors.bottom: parent.bottom; anchors.bottomMargin: 28; width: parent.width-20; text: root.flow.status; color: Theme.textColor("#8b806d"); font.pixelSize: 10; elide: Text.ElideRight }
    Row { anchors.right: parent.right; anchors.rightMargin: 17; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; spacing: 24; visible: root.flow.editing&&!root.flow.choosing
        Text { visible: root.flow.page===0; font.underline: root.takesFocus&&root.flow.focusIndex===root.flow.rows.length-3; text: "Reset draft"; color: Theme.textColor("#8e9888"); font.pixelSize: 12; TapHandler { onTapped: root.flow.activate(root.flow.rows.length-3) } }
        Text { font.underline: root.takesFocus&&root.flow.focusIndex===root.flow.rows.length-1; text: "Discard & back"; color: root.flow.focusIndex===root.flow.rows.length-1?Theme.textColor("#314f3a"):Theme.textColor("#8e9888"); font.pixelSize: 12; TapHandler { onTapped: root.flow.activate(root.flow.rows.length-1) } }
        Text { font.underline: root.takesFocus&&root.flow.focusIndex===root.flow.rows.length-2; text: "Save profile"; color: Theme.textColor("#3e7c56"); font.pixelSize: 12; font.weight: Font.DemiBold; TapHandler { onTapped: root.flow.activate(root.flow.rows.length-2) } }
    }

    Rectangle {
        anchors.fill: parent; visible: root.flow.toolsOpen || root.flow.preview>=0 || !!Theme.controller?.captureMode.length; color: Theme.surface
        MouseArea { anchors.fill: parent }
        Column { x: 20; y: 12; width: parent.width-40; visible: root.flow.toolsOpen; spacing: 0
            Text { height: 34; text: "Layout tools"; font.pixelSize: 18; color: Theme.ink }
            Repeater { model: root.flow.tools
                SettingsRow { required property int index; required property string modelData; height: 30; width: parent.width; compact: true; title: modelData; selected: root.flow.toolFocus===index; onActivated: root.flow.activateTool(index) }
            }
        }
        RingDiagram { anchors.fill: parent; anchors.margins: 32; visible: root.flow.preview>=0; slots: root.flow.draft.rings?.[root.flow.preview] || []; presentation: root.flow.draft.ringPresentation?.[root.flow.preview] || ({}); selected: root.flow.previewSelection; caption: "Preview\n"+(root.flow.draft.rings?.[root.flow.preview]?.[root.flow.previewSelection] || ""); onPicked: slot => root.flow.selectPreview(slot) }
        Text { x: 14; y: 12; visible: root.flow.preview>=0; text: "Back"; color: Theme.accent; TapHandler { onTapped: root.flow.back() } }
        Text { anchors.centerIn: parent; width: parent.width*.8; visible: !!Theme.controller?.captureMode.length; text: Theme.controller?.captureStatus || ""; color: Theme.ink; font.pixelSize: 17; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
        Text { anchors.bottom: parent.bottom; anchors.bottomMargin: 24; anchors.horizontalCenter: parent.horizontalCenter; visible: !!Theme.controller?.captureMode.length; text: "Cancel capture"; color: Theme.accent; font.pixelSize: 13; TapHandler { onTapped: Theme.controller.cancelCapture() } }
    }
}
