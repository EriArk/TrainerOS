pragma Singleton
import QtQuick

QtObject {
    property string themeId: "turquoise"
    property bool reducedMotion: false
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
    readonly property var tabColors: ["#f4cc62", "#b4d8a0", "#98cadd", "#eba9ad", "#c6b4df"]
    // Logical design space, separate from the physical display's pixel count.
    readonly property int viewportWidth: 960
    readonly property int viewportHeight: 540
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
    readonly property int topRimHeight: viewportWidth - screenBounds.x - screenBounds.width
    readonly property real tabWidth: (screenBounds.x + screenBounds.width - brandWidth - 4 * tabSpacing) / 5
    readonly property int panelInset: 12
    readonly property int contentTopInset: brandHeight + screenBevel + 1 - screenTop
    readonly property int footerHeight: 37
    readonly property int footerTop: viewportHeight - footerHeight
    readonly property rect screenBounds: Qt.rect(12, screenTop, viewportWidth - 24, footerTop - 5 - screenTop)
}
