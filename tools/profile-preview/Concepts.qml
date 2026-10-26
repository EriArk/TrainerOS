import QtQuick
import QtQuick.Window

// Isolated design fixture. No account, library, wallet or profile writes.
Window {
    id: window
    width: 1280; height: 720; visible: true
    title: "TrainerOS · Profile concepts · sample data"
    color: Theme.chassisDark
    property real uiScale: Math.max(1,width/960)
    readonly property bool scene: false
    // Isolated illustrative account states; these are never production balances.
    property int accountState: 0
    readonly property bool accountKnown: accountState===0 || accountState===2
    readonly property bool accountFresh: accountState===0
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
            if(event.key===Qt.Key_D)window.accountState=(window.accountState+1)%4
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
            Repeater { model: ["Профиль", "Гардероб"]
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
            Column {
                id: content; x: landscape.width+24; width: parent.width-x; height: parent.height
                spacing: page.short?6:9
                Item {
                    width: parent.width; height: page.short?48:62
                    Text { text: "ERI"; y: -6; font.family: Theme.displayFamily; font.pixelSize: page.short?31:39; color: Theme.ink }
                    Text { anchors.right: parent.right; y: 2; text: "✎"; font.pixelSize: 20; color: Theme.muted }
                    Text { y: page.short?29:40; text: "Исследователь миров"; font.pixelSize: page.short?12:14; color: window.accent }
                }
                Item {
                    id: account; width: parent.width; height: page.short?84:100
                    readonly property real levelWidth: width*.42
                    Rectangle { x: 0; y: 5; width: 36; height: 36; radius: 12; color: Theme.dark?"#293f3d":"#e1ecd9"
                        Text { anchors.centerIn: parent; text: window.accountKnown?"12":"—"; font.pixelSize: 22; font.family: Theme.displayFamily; color: window.accent }
                    }
                    Text { x: 46; y: 2; text: "Уровень аккаунта"; font.pixelSize: 12; color: Theme.ink }
                    Text { x: 46; y: 22; text: "Trainer Mastery"; font.pixelSize: 10; color: Theme.muted }
                    Rectangle { y: 50; width: account.levelWidth-22; height: 4; radius: 2; color: Theme.divider
                        Rectangle { width: parent.width*.62; height: parent.height; radius: 2; color: window.accent; visible: window.accountKnown }
                    }
                    Text { y: 62; width: account.levelWidth-16; text: window.accountKnown?"Прогресс к уровню 13":window.accountState===3?"Войдите в аккаунт":"Данные недоступны"; font.pixelSize: 10; color: Theme.muted; elide: Text.ElideRight }
                    Rectangle { x: account.levelWidth; y: 0; width: 1; height: account.height-8; color: Theme.divider }
                    Item {
                        x: account.levelWidth+18; width: parent.width-x; height: parent.height
                        Rectangle { width: 24; height: 24; radius: 12; y: 5; color: Theme.dark?"#b89245":"#e9bf63"; border.width: 1; border.color: Theme.dark?"#ead193":"#b9923c"
                            Rectangle { anchors.centerIn: parent; width: 14; height: 14; radius: 7; color: "transparent"; border.color: Theme.dark?"#ead193":"#b9923c" }
                        }
                        Text { x: 33; y: -1; text: window.accountKnown?"1 250":"—"; font.family: Theme.displayFamily; font.pixelSize: 27; color: Theme.ink }
                        Text { anchors.right: parent.right; y: 10; text: "Coins"; font.pixelSize: 12; color: Theme.muted }
                        Text { y: 35; text: window.accountFresh?"При поддержке ×3  →  4 250":window.accountState===2?"Последний известный баланс":window.accountState===3?"Войдите через Fluxer":"Баланс пока недоступен"; font.pixelSize: 11; color: window.accountFresh?window.accent:Theme.muted }
                        Text { y: 54; width: parent.width; text: window.accountFresh?"+3 000 за прошлые награды":window.accountState===2?"Доплата: нужен свежий расчёт":"Доплата: нет подтверждённого расчёта"; font.pixelSize: 10; color: Theme.muted; wrapMode: Text.WordWrap }
                        Text { y: 74; visible: !page.short; text: window.accountFresh?"Пример уровня поддержки · Подробнее ›":window.accountState===2?"Офлайн · данные из кеша":"История и локальный профиль доступны"; font.pixelSize: 9; color: Theme.muted }
                    }
                }
                Rectangle { width: parent.width; height: 1; color: Theme.divider }
                Item {
                    id: recentAwards; width: parent.width; height: page.short?76:90
                    Text { text: "RetroAchievements"; font.pixelSize: 14; color: Theme.ink }
                    Text { anchors.right: parent.right; y: 2; text: "Все достижения  ›"; font.pixelSize: 11; color: window.accent }
                    Row { y: 27; width: parent.width; spacing: 12
                        Repeater { model: [ {name:"Первое открытие",when:"Сегодня · 18:42",glyph:"sun",tint:"#dca86b"}, {name:"Без единой ошибки",when:"Вчера · 21:08",glyph:"game-controller",tint:"#80aea0"} ]
                            delegate: Item { required property var modelData; width: (recentAwards.width-12)/2; height: 43
                                Rectangle { width: 36; height: 36; radius: 10; rotation: 5; color: Theme.dark?Qt.darker(modelData.tint,1.8):Qt.lighter(modelData.tint,1.25) }
                                MenuGlyph { x: 8; y: 8; width: 20; height: 20; glyph: modelData.glyph; ink: Theme.dark?Qt.lighter(modelData.tint,1.5):Qt.darker(modelData.tint,1.5) }
                                Text { x: 45; width: parent.width-x; text: modelData.name; font.pixelSize: 11; color: Theme.ink; elide: Text.ElideRight }
                                Text { x: 45; y: 20; text: modelData.when; font.pixelSize: 10; color: Theme.muted }
                            }
                        }
                    }
                }
                Item {
                    id: journey; width: parent.width; height: content.height-y
                    Text { text: "Игровой путь"; font.pixelSize: 14; color: Theme.ink }
                    Text { anchors.right: parent.right; y: 2; text: "Вся история  ›"; font.pixelSize: 11; color: window.accent }
                    Image { id: memory; y: 27; width: page.short?72:90; height: page.short?40:50; source: PreviewConfig.assets+"/crates/projects/pocket-golf.png"; fillMode: Image.PreserveAspectCrop; sourceSize: Qt.size(240,140) }
                    Text { x: memory.width+12; y: 27; text: "Вечер на поле"; font.pixelSize: 12; color: Theme.ink }
                    Text { x: memory.width+12; y: 46; text: "Pocket Golf · сегодня · 42 мин"; font.pixelSize: 10; color: Theme.muted }
                    Text { y: memory.y+memory.height+8; text: "24 игры  ·  137 ч в игре  ·  Снимки  ›"; font.pixelSize: 10; color: Theme.muted }
                }
            }
        }
        Text { x: 29; y: parent.height-27; text: "← →  Поворот      T  Тема      D  Данные"; font.pixelSize: 10; color: Theme.muted }
        Text { anchors.right: parent.right; anchors.rightMargin: 29; y: parent.height-27; text: "A · Макет · демонстрационные данные"; font.pixelSize: 10; color: Theme.muted }
    }
    property int stage: 0
    property int ticks: 0
    readonly property var shots: [
        {name:"profile-a-light",state:0,dark:false,w:1280,h:720},
        {name:"profile-a-dark",state:0,dark:true,w:1280,h:720},
        {name:"profile-a-compact",state:0,dark:false,w:800,h:450},
        {name:"profile-a-16x10",state:0,dark:false,w:1280,h:800},
        {name:"profile-a-unavailable",state:1,dark:false,w:1280,h:720},
        {name:"profile-a-offline",state:2,dark:true,w:1280,h:720},
        {name:"profile-a-signed-out",state:3,dark:false,w:800,h:450}]
    function configure() {
        const s=shots[stage];accountState=s.state;Theme.themeId=s.dark?"dark":"light";width=s.w;height=s.h
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
