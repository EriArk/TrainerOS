import QtQuick
import QtQuick.Window

// Isolated design fixture. No account, library, wallet or profile writes.
Window {
    id: window
    width: 1280; height: 720; visible: true
    title: "TrainerOS · Profile concepts · sample data"
    color: Theme.chassisDark
    property real uiScale: Math.max(1,width/960)
    property bool scene: false
    property int backdrop: 0
    readonly property color accent: Theme.dark ? "#b9cfa5" : "#527b59"
    readonly property var games: [
        {name: "Pocket Golf", file: "pocket-golf.png"},
        {name: "Falling Blocks", file: "falling-blocks.png"},
        {name: "Pocket Artillery", file: "pocket-artillery.png"}]
    readonly property var awards: [
        {name: "Первый шаг", glyph: "game-controller", color: "#dca86b"},
        {name: "Исследователь", glyph: "sun", color: "#80aea0"},
        {name: "Моменты", glyph: "pictures", color: "#b69ac5"}]

    Item {
        id: canvas; width: window.width/window.uiScale; height: window.height/window.uiScale
        scale: window.uiScale; transformOrigin: Item.TopLeft; focus: true
        Keys.onPressed: event => {
            if(event.key===Qt.Key_1)window.scene=false
            else if(event.key===Qt.Key_2)window.scene=true
            else if(event.key===Qt.Key_T)Theme.themeId=Theme.dark?"light":"dark"
            else if(event.key===Qt.Key_Left)avatar.yaw-=10
            else if(event.key===Qt.Key_Right)avatar.yaw+=10
            else if(event.key===Qt.Key_Escape)Qt.quit()
        }
        Rectangle { anchors.fill: parent; color: Theme.chassisDark }
        Rectangle { anchors.fill: parent; anchors.margins: 7; radius: 21; color: Theme.paper; border.width: 2; border.color: Theme.rim }
        Text { x: 29; y: 24; text: "‹  Игровой профиль"; font.pixelSize: 17; color: Theme.ink }
        Row {
            anchors.right: parent.right; anchors.rightMargin: 32; y: 24; spacing: 27
            Repeater { model: ["Профиль", "Игровой путь", "RetroAchievements", "Гардероб"]
                delegate: Item { required property string modelData; required property int index; width: tabText.width; height: 32
                    Text { id: tabText; text: modelData; font.pixelSize: 14; color: index===0?Theme.ink:Theme.muted }
                    Rectangle { visible: index===0; y: 26; width: parent.width; height: 2; radius: 1; color: window.accent }
                }
            }
        }
        Rectangle { x: 28; y: 66; width: parent.width-56; height: 1; color: Theme.divider }

        Item {
            id: page; x: 28; y: 84; width: parent.width-56; height: parent.height-y-43
            readonly property bool short: height<365
            readonly property real artWidth: window.scene ? width*.52 : width*.37
            Rectangle {
                id: landscape; width: page.artWidth; height: parent.height; radius: window.scene?18:12; clip: true
                gradient: Gradient {
                    GradientStop { position: 0; color: Theme.dark ? "#182536" : window.backdrop===1?"#decde4":"#bdd9dc" }
                    GradientStop { position: 1; color: Theme.dark ? "#293f40" : window.backdrop===1?"#e7d8c3":"#dce8ce" }
                }
                Rectangle { x: parent.width*.60; y: parent.height*.10; width: parent.width*.32; height: width; radius: width/2; color: Theme.dark?"#b8c6ab":"#fff1c9"; opacity: .88 }
                Canvas {
                    id: scenery; anchors.fill: parent
                    property bool night: Theme.dark
                    property int choice: window.backdrop
                    onNightChanged: requestPaint()
                    onChoiceChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onPaint: {
                        let c=getContext("2d");c.reset()
                        const w=width,h=height
                        c.fillStyle=night?"#293b47":choice===1?"#bbaec9":"#96bab3"
                        c.beginPath();c.moveTo(0,h*.47);c.bezierCurveTo(w*.38,h*.12,w*.52,h*.77,w,h*.34);c.lineTo(w,h);c.lineTo(0,h);c.fill()
                        c.fillStyle=night?"#36504b":choice===1?"#c5b8b1":"#b9cdb1"
                        c.beginPath();c.moveTo(0,h*.73);c.bezierCurveTo(w*.4,h*.4,w*.6,h*.6,w,h*.68);c.lineTo(w,h);c.lineTo(0,h);c.fill()
                        c.strokeStyle=night?"#688171":"#e9edd4";c.lineWidth=2
                        c.beginPath();c.moveTo(w*.12,h*.87);c.bezierCurveTo(w*.4,h*.71,w*.64,h*.93,w*.92,h*.77);c.stroke()
                    }
                }
                Text { x: 18; y: 17; text: window.scene?"ТВОЁ МЕСТО":"T R A I N E R  C A R D"; font.pixelSize: 10; font.letterSpacing: 1.3; color: Theme.dark?"#d0dfdc":"#466869" }
                Rectangle { visible: window.scene; x: parent.width*.24; y: parent.height*.84; width: parent.width*.50; height: parent.height*.09; radius: width/2; color: Theme.dark?"#4b6259":"#e7e6c9"; border.color: Theme.dark?"#738276":"#fcf4d8" }
                AvatarPreview3D {
                    id: avatar; x: window.scene?parent.width*.10:0; y: window.scene?18:27
                    width: window.scene?parent.width*.80:parent.width
                    height: parent.height-(window.scene?58:44)
                    modelSource: PreviewConfig.avatar; yaw: window.scene?-12:9
                }
                Row { x: 17; y: parent.height-30; spacing: 7
                    Repeater { model: ["#a7cbbd","#cbb8cf"]
                        delegate: Rectangle { required property string modelData; required property int index; width: 13; height: 13; radius: 6.5; color: modelData; border.width: window.backdrop===index?2:0; border.color: Theme.paper
                            MouseArea { anchors.fill: parent; anchors.margins: -4; onClicked: window.backdrop=index }
                        }
                    }
                    Text { text: "  Выбрать фон"; font.pixelSize: 10; color: Theme.dark?"#e2e9df":"#476254" }
                }
                Item {
                    visible: window.scene; x: 12; y: parent.height*.22; width: 35; height: 134
                    Repeater { model: ["sun","game-controller","pictures"]
                        delegate: Rectangle { required property string modelData; required property int index; y: index*43; width: 32; height: 32; radius: 16; color: Theme.dark?"#30283f42":"#60fffef9"; border.color: Theme.dark?"#819c99":"#f8fbec"
                            MenuGlyph { anchors.centerIn: parent; width: 16; height: 16; glyph: modelData; ink: Theme.dark?"#dae6dc":"#365e5c" }
                        }
                    }
                }
            }
            Item {
                id: content; x: landscape.width+26; width: parent.width-x; height: parent.height
                Text { text: "ERI"; y: -5; font.family: Theme.displayFamily; font.pixelSize: page.short?38:46; color: Theme.ink }
                Text { x: parent.width-22; y: 7; text: "✎"; font.pixelSize: 21; color: Theme.muted }
                Text { y: page.short?43:52; text: "Исследователь миров"; font.pixelSize: 15; color: window.accent }
                Text { y: page.short?68:80; width: parent.width; text: "Ещё одна игра. Ещё одна история."; font.pixelSize: 12; color: Theme.muted; wrapMode: Text.WordWrap }
                Row {
                    id: counters; y: page.short?99:117; width: parent.width; spacing: 21
                    Repeater { model: [{value:"24",name:"игры"},{value:"137 ч",name:"в игре"},{value:"86",name:"достижений"}]
                        delegate: Item { required property var modelData; width: (counters.width-42)/3; height: 49
                            Text { text: modelData.value; font.family: Theme.displayFamily; font.pixelSize: 24; color: Theme.ink }
                            Text { y: 30; width: parent.width; text: modelData.name; font.pixelSize: 10; color: Theme.muted; elide: Text.ElideRight }
                        }
                    }
                }
                Rectangle { y: counters.y+54; width: parent.width; height: 1; color: Theme.divider }
                Text { id: shelfTitle; y: counters.y+66; text: "Моя витрина"; font.pixelSize: 13; color: Theme.ink }
                Text { anchors.right: parent.right; y: shelfTitle.y; text: "Настроить  ›"; font.pixelSize: 11; color: Theme.muted }
                Row {
                    id: trophies; y: shelfTitle.y+24; width: parent.width; spacing: 12
                    Repeater { model: window.awards
                        delegate: Item { required property var modelData; width: (trophies.width-24)/3; height: page.short?39:54
                            Rectangle { width: 29; height: 29; radius: 9; rotation: 7; color: Theme.dark?Qt.darker(modelData.color,1.8):Qt.lighter(modelData.color,1.25) }
                            MenuGlyph { x: 6; y: 6; width: 17; height: 17; glyph: modelData.glyph; ink: Theme.dark?Qt.lighter(modelData.color,1.5):Qt.darker(modelData.color,1.5) }
                            Text { x: 37; y: 0; width: parent.width-x; height: 35; text: modelData.name; font.pixelSize: 11; wrapMode: Text.WordWrap; color: Theme.ink; maximumLineCount: 2; elide: Text.ElideRight }
                        }
                    }
                }
                Item {
                    id: favorites; x: 0; y: trophies.y+trophies.childrenRect.height+9; width: parent.width; height: parent.height-y
                    Text { text: "Любимые игры"; font.pixelSize: 13; color: Theme.ink }
                    Row { id: gameRow; y: 23; width: parent.width; spacing: 11
                        Repeater { model: window.games
                            delegate: Item { required property var modelData; width: (gameRow.width-22)/3; height: art.height+23
                                Rectangle { id: art; width: parent.width; height: Math.min(66,Math.max(39,favorites.height-45)); radius: 7; color: Theme.surface; clip: true
                                    Image { anchors.fill: parent; source: PreviewConfig.assets+"/crates/projects/"+modelData.file; fillMode: Image.PreserveAspectFit; sourceSize: Qt.size(280,150) }
                                }
                                Text { y: art.height+5; width: parent.width; text: modelData.name; font.pixelSize: 10; color: Theme.muted; elide: Text.ElideRight }
                            }
                        }
                    }
                }
            }
        }
        Text { x: 29; y: parent.height-27; text: "← →  Поворот        1 / 2  Вариант        T  Тема"; font.pixelSize: 10; color: Theme.muted }
        Text { anchors.right: parent.right; anchors.rightMargin: 29; y: parent.height-27; text: window.scene?"B · Сцена с персонажем":"A · Карточка Trainer"; font.pixelSize: 10; color: Theme.muted }
    }
    property int stage: 0
    property int ticks: 0
    readonly property var shots: [
        {name:"card-light",scene:false,dark:false,w:1280,h:720},
        {name:"scene-light",scene:true,dark:false,w:1280,h:720},
        {name:"card-dark",scene:false,dark:true,w:1280,h:720},
        {name:"scene-dark",scene:true,dark:true,w:1280,h:720},
        {name:"card-compact",scene:false,dark:false,w:800,h:450},
        {name:"scene-compact",scene:true,dark:false,w:800,h:450},
        {name:"card-16x10",scene:false,dark:false,w:1280,h:800},
        {name:"scene-16x10",scene:true,dark:false,w:1280,h:800}]
    function configure() {
        const s=shots[stage];scene=s.scene;Theme.themeId=s.dark?"dark":"light";width=s.w;height=s.h
    }
    Component.onCompleted: { configure(); showNormal() }
    Timer {
        interval: 1200; running: PreviewConfig.capture; repeat: true
        onTriggered: {
            if(++window.ticks>35){console.error("Avatar/capture timeout");Qt.exit(2);return}
            if(!avatar.ready)return
            stop()
            window.contentItem.grabToImage(function(result){
                if(!result.saveToFile(PreviewConfig.output+"/"+window.shots[window.stage].name+".png")){Qt.exit(3);return}
                console.log("CAPTURE",window.shots[window.stage].name)
                if(++window.stage===window.shots.length){Qt.quit();return}
                window.configure();start()
            },Qt.size(window.width,window.height))
        }
    }
}
