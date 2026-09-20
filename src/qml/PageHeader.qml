import QtQuick

// Printed inside the shared safe viewport. Home and small confirmations retain
// their own composition; feature code supplies words, never header coordinates.
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
    readonly property int titleTop: eyebrow.length ? 23 : 5
    readonly property int titleSize: compact ? 27 : 30
    implicitHeight: titleTop + heading.height + (subtitle.length ? (multilineStatus ? 42 : 23) : 7)
    width: parent ? parent.width : 0
    height: implicitHeight
    Text {
        x: Theme.pageMargin; y: 3; width: parent.width - 2 * x
        visible: root.eyebrow.length > 0; text: root.eyebrow
        textFormat: Text.PlainText; elide: Text.ElideRight
        color: Theme.muted; font.pixelSize: 11; font.letterSpacing: 1.2
    }
    Text {
        id: heading
        objectName: "page-heading"
        x: Theme.pageMargin; y: root.titleTop
        width: parent.width - 2 * x - (root.trailing.length ? trailingLabel.width + 22 : 0)
        height: root.multilineTitle ? implicitHeight : 36; verticalAlignment: Text.AlignVCenter
        wrapMode: root.multilineTitle ? Text.WordWrap : Text.NoWrap
        maximumLineCount: root.multilineTitle ? 2 : 1
        text: root.title; textFormat: Text.PlainText; elide: Text.ElideRight
        color: Theme.ink; font.pixelSize: root.titleSize; font.weight: Font.DemiBold
    }
    Text {
        id: trailingLabel
        anchors { right: parent.right; rightMargin: Theme.pageMargin }
        y: root.titleTop + 12; width: Math.min(implicitWidth, parent.width * 0.44)
        text: root.trailing; textFormat: Text.PlainText; elide: Text.ElideRight
        color: Theme.muted; font.pixelSize: 14
    }
    Text {
        x: Theme.pageMargin + 1; y: heading.y + heading.height
        width: parent.width - 2 * Theme.pageMargin - 2
        height: root.multilineStatus ? 40 : 20; visible: root.subtitle.length > 0
        text: root.subtitle; textFormat: Text.PlainText; elide: root.subtitleElide
        wrapMode: root.multilineStatus ? Text.WordWrap : Text.NoWrap
        maximumLineCount: root.multilineStatus ? 2 : 1
        color: Theme.muted; font.pixelSize: 14
    }
}
