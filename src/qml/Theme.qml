pragma Singleton
import QtQuick

QtObject {
    property string themeId: "turquoise"
    property bool reducedMotion: false
    readonly property var palettes: ({turquoise: ["#297e79", "#175755", "#103e43", "#76b3a4"],
        red: ["#984b51", "#713b45", "#432a37", "#cb9293"], green: ["#497c52", "#345b40", "#203d32", "#92b287"],
        blue: ["#3d739d", "#305477", "#233b53", "#87b2ca"], orange: ["#976038", "#72472d", "#49372b", "#c9a375"]})
    readonly property var palette: palettes[themeId] || palettes.turquoise
    readonly property color chassisTop: palette[0]
    readonly property color chassis: palette[1]
    readonly property color chassisDark: palette[2]
    readonly property color rim: palette[3]
    function motion(milliseconds) { return reducedMotion ? 0 : milliseconds }
    readonly property color paper: "#fffef9"
    readonly property color ink: "#193f3b"
    readonly property color muted: "#526d63"
    readonly property color yellow: "#f4cc62"
    readonly property color blue: "#98cadd"
    readonly property color pink: "#eba9ad"
    readonly property color green: "#b4d8a0"
    readonly property color focus: "#753d15"
    readonly property var tabColors: ["#f4cc62", "#b4d8a0", "#98cadd", "#eba9ad", "#c6b4df"]
    // Logical design space, separate from the physical display's pixel count.
    readonly property int viewportWidth: 960
    readonly property int viewportHeight: 540
    readonly property int pageMargin: 28
    // One chassis geometry for primary pages and frame-mounted services.
    readonly property int tabBaseline: 49
    readonly property int tabBevel: 13
    readonly property int activeTabOverlap: 14
    readonly property int brandWidth: 206
    readonly property int brandBevel: 18
    readonly property int panelInset: 10
    readonly property int contentTopInset: tabBevel + activeTabOverlap + panelInset
    readonly property int footerHeight: 37
    readonly property int footerTop: viewportHeight - footerHeight
    readonly property rect screenBounds: Qt.rect(12, tabBaseline - tabBevel, viewportWidth - 24,
                                                 footerTop - 5 - tabBaseline + tabBevel)
}
