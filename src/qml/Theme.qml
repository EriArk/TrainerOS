pragma Singleton
import QtQuick

QtObject {
    property string themeId: "turquoise"
    property bool reducedMotion: false
    property bool television: false
    property real uiScale: 1
    readonly property FontLoader displayTypeface: FontLoader { source: "qrc:/fonts/Fredoka.ttf" }
    readonly property string displayFamily: displayTypeface.name
    readonly property FontLoader brandTypeface: FontLoader { source: "qrc:/fonts/ChakraPetch-Bold.ttf" }
    readonly property string brandFamily: brandTypeface.name
    readonly property FontLoader gameTypeface: FontLoader { source: "qrc:/fonts/Bungee-Regular.ttf" }
    readonly property string gameFamily: gameTypeface.name
    readonly property var palettes: ({turquoise: ["#297e79", "#175755", "#103e43", "#76b3a4"],
        red: ["#984b51", "#713b45", "#432a37", "#cb9293"], green: ["#497c52", "#345b40", "#203d32", "#92b287"],
        blue: ["#3d739d", "#305477", "#233b53", "#87b2ca"], orange: ["#976038", "#72472d", "#49372b", "#c9a375"]})
    readonly property var palette: palettes[themeId] || palettes.turquoise
    // The quiet Home landscape belongs to the shell theme, never to game media.
    // sky, ground, path, sun, outer rim, inner rim, translucent base.
    // Keep the approved muted blend with the shell's paper pattern underneath.
    readonly property var homeLandscapePalettes: ({
        turquoise: ["#638eca", "#497643", "#ddb467", "#fff2b1", "#c4d7a9", "#7b9b83", "#b5204c47"],
        red: ["#916c89", "#614461", "#dcad98", "#f6d5ad", "#d4b2b0", "#a78996", "#b54f263a"],
        green: ["#88a174", "#386334", "#c3b46e", "#e8e9a7", "#c2d0a5", "#889d7d", "#b52f4930"],
        blue: ["#739ad0", "#456482", "#b1becb", "#d3e5dd", "#b5d0d6", "#819fb2", "#b5273f60"],
        orange: ["#bd9168", "#80603b", "#e4bb79", "#ffe0a2", "#dbc39b", "#b19a79", "#b55e3b24"]
    })
    readonly property var homeLandscape: homeLandscapePalettes[themeId] || homeLandscapePalettes.turquoise
    readonly property color chassisTop: Qt.darker(palette[0], 1.12)
    readonly property color chassis: Qt.darker(palette[1], 1.12)
    readonly property color chassisDark: Qt.darker(palette[2], 1.08)
    readonly property color rim: palette[3]
    readonly property color chassisCrown: Qt.lighter(chassisTop, 1.16)
    readonly property color chassisFoot: Qt.darker(chassis, 1.24)
    readonly property color edgeLight: Qt.lighter(rim, 1.12)
    readonly property color edgeShadow: Qt.darker(chassisDark, 1.35)
    readonly property color screenSlope: Qt.lighter(chassis, 1.18)
    readonly property color screenContact: Qt.darker(screenSlope, 1.25)
    function motion(milliseconds) { return reducedMotion ? 0 : milliseconds }
    readonly property color paper: "#fffef9"
    readonly property color ink: "#193f3b"
    readonly property color muted: "#526d63"
    readonly property color yellow: "#f4cc62"
    readonly property color blue: "#98cadd"
    readonly property color pink: "#eba9ad"
    readonly property color green: "#b4d8a0"
    readonly property color focus: "#ffe37a"
    readonly property color focusGlow: "#ffbd24"
    readonly property var tabColors: ["#f4cc62", "#b4d8a0", "#98cadd", "#eba9ad", "#c6b4df", "#d9bfa0"]
    // Logical design space, separate from the physical display's pixel count.
    property real viewportWidth: 960
    property real viewportHeight: 540
    readonly property int pageMargin: 28
    // One chassis geometry for primary pages and frame-mounted services.
    readonly property int brandHeight: 63
    readonly property int screenTop: 36
    readonly property int tabBaseline: 53
    readonly property int tabBevel: 13
    readonly property int activeTabOverlap: brandHeight + screenBevel - tabBaseline
    readonly property int brandWidth: 206
    readonly property int brandBevel: 18
    readonly property int tabSpacing: 4
    readonly property int screenBevel: 11
    readonly property int topRimHeight: 12
    readonly property real tabWidth: (viewportWidth - 12 - brandWidth - 4 * tabSpacing) / 5
    readonly property int panelInset: 12
    readonly property int contentTopInset: brandHeight + screenBevel + 27 - screenTop
    readonly property int footerHeight: 37
    readonly property int footerTop: viewportHeight - footerHeight
    readonly property int adventureTabWidth: 242
    readonly property int adventureTabHeight: 53
    // Only the lower-left corner is occupied. Right-hand content reaches the lip.
    readonly property int adventureCutoutWidth: adventureTabWidth - screenBevel - screenBounds.x - panelInset + 16
    readonly property int adventureCutoutHeight: screenBounds.y + screenBounds.height - panelInset - (footerTop - adventureTabHeight) + 8
    readonly property rect screenBounds: Qt.rect(12, screenTop, viewportWidth - 24, footerTop - 5 - screenTop)
}
