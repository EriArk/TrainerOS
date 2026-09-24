import QtQuick

// One compact row: a title on the left, its context on the right.
Item {
    id: root
    property string title: ""
    property string subtitle: ""
    property string eyebrow: ""
    property string trailing: ""
    property bool compact: false
    property bool multilineStatus: false
    property bool multilineTitle: false
    property int subtitleElide: Text.ElideRight
    readonly property int titleSize: compact ? 27 : 30
    readonly property string context: [eyebrow, subtitle, trailing].filter(value => value.length > 0).join(" · ")
    implicitHeight: Math.max(48, heading.height + 12, multilineStatus ? 54 : 0)
    width: parent ? parent.width : 0
    height: implicitHeight
    TextMetrics { id: titleMetrics; font: heading.font; text: root.title }
    Text {
        id: heading
        objectName: "page-heading"
        x: Theme.pageMargin; anchors.verticalCenter: parent.verticalCenter
        width: root.context.length ? Math.min(titleMetrics.advanceWidth, (parent.width - 2 * x) * (root.multilineTitle ? 0.80 : 0.62)) : parent.width - 2 * x
        height: root.multilineTitle ? implicitHeight : 36; verticalAlignment: Text.AlignVCenter
        wrapMode: root.multilineTitle ? Text.WordWrap : Text.NoWrap
        maximumLineCount: root.multilineTitle ? 2 : 1
        text: root.title; textFormat: Text.PlainText; elide: Text.ElideRight
        color: Theme.ink; font.pixelSize: root.titleSize; font.weight: Font.DemiBold
    }
    Text {
        objectName: "page-heading-context"
        anchors { left: heading.right; leftMargin: 24; right: parent.right; rightMargin: Theme.pageMargin; verticalCenter: parent.verticalCenter }
        height: root.multilineStatus ? 40 : 20; visible: root.context.length > 0
        text: root.context; textFormat: Text.PlainText; elide: root.subtitleElide
        wrapMode: root.multilineStatus ? Text.WordWrap : Text.NoWrap
        maximumLineCount: root.multilineStatus ? 2 : 1
        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
        color: Theme.muted; font.pixelSize: 13
    }
}
