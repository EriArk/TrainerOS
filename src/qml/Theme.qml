pragma Singleton
import QtQuick

QtObject {
    property var controller: null
    function buttonLabel(hint) {
        const family=controller?.family, profile=controller?.profile
        return controller ? hint.split("/").map(part => controller.glyph(part)).join("/") : hint
    }
    property string themeId: "light"
    readonly property bool dark: themeId === "dark"
    readonly property color accentInk: "#fffef9"
    readonly property color surface: dark ? "#0c1114" : "#fafbf7"
    readonly property color divider: dark ? "#2a383c" : "#dfe6df"
    readonly property color accent: dark ? "#8ebfb1" : "#3d7b61"
    readonly property color selection: dark ? "#1e3335" : "#e5f0e7"
    // Preserve hue of existing UI accents while giving dark surfaces a restrained
    // luminance. Artwork/media are not recolored. Alpha remains unchanged.
    function surfaceColor(value) {
        const c=Qt.color(value)
        if(!dark || c.a===0 || Math.max(c.r,c.g,c.b)<.48)return c
        const mean=(c.r+c.g+c.b)/3
        return Qt.rgba(.018+c.r*.055+(c.r-mean)*.07,.028+c.g*.055+(c.g-mean)*.07,.035+c.b*.055+(c.b-mean)*.07,c.a)
    }
    function textColor(value) {
        const c=Qt.color(value)
        if(!dark || c.a===0 || Math.min(c.r,c.g,c.b)>.72)return c
        return Qt.rgba(.64+c.r*.29,.67+c.g*.27,.65+c.b*.28,c.a)
    }
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
    readonly property var palette: dark ? ["#151e23", "#10171c", "#090e12", "#435759"] : palettes[themeId] || palettes.turquoise
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
    readonly property color paper: dark ? "#090e11" : "#fffef9"
    readonly property color ink: dark ? "#e0e7e4" : "#193f3b"
    readonly property color muted: dark ? "#93a4a2" : "#526d63"
    readonly property color yellow: dark ? "#54482e" : "#f4cc62"
    readonly property color blue: dark ? "#273f4b" : "#98cadd"
    readonly property color pink: dark ? "#503740" : "#eba9ad"
    readonly property color green: dark ? "#304737" : "#b4d8a0"
    readonly property color focus: "#ffe37a"
    readonly property color focusGlow: "#ffbd24"
    readonly property var tabColors: dark ? [yellow, green, blue, pink, "#3b344b", "#493e2e"] : ["#f4cc62", "#b4d8a0", "#98cadd", "#eba9ad", "#c6b4df", "#d9bfa0"]
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
    readonly property int brandWidth: Math.min(286, Math.floor(viewportWidth * .35))
    readonly property int brandBevel: 18
    readonly property int tabSpacing: 4
    readonly property int screenBevel: 11
    readonly property int topRimHeight: 12
    readonly property real tabWidth: (viewportWidth - 12 - brandWidth - 4 * tabSpacing) / 5
    readonly property int panelInset: 12
    readonly property int contentTopInset: brandHeight + screenBevel + 27 - screenTop
    readonly property int footerHeight: 37
    readonly property int footerTop: viewportHeight - footerHeight
    readonly property int adventureTabWidth: 174
    readonly property int adventureTabHeight: 36
    readonly property int drawerHandleOverlap: 32
    // Both compact drawer handles are mounted at the lower-left screen lip.
    readonly property int adventureCutoutWidth: adventureTabWidth - screenBevel - screenBounds.x - panelInset + 16
    readonly property int adventureCutoutHeight: screenBounds.y + screenBounds.height - panelInset - (footerTop - adventureTabHeight) + 8
    readonly property rect screenBounds: Qt.rect(12, screenTop, viewportWidth - 24, footerTop - 5 - screenTop)
}
