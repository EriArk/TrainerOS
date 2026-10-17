import QtQuick

Image {
    id: root
    property string kind: "file"
    property string preview: ""
    readonly property string asset: {
        if (kind === "folder" || kind === "inode-directory") return "folder"
        if (kind === "user-home") return "home"
        if (kind === "folder-download") return "downloads"
        if (kind === "folder-pictures") return "pictures"
        if (kind === "folder-documents") return "documents"
        if (kind === "user-trash") return "trash"
        if (kind.startsWith("drive-")) return "drive"
        if (kind.startsWith("network")) return "network"
        if (kind.startsWith("image")) return "image"
        if (kind.startsWith("audio")) return "audio"
        if (kind.startsWith("video")) return "video"
        if (kind.includes("pdf")) return "pdf"
        if (kind.includes("zip") || kind.includes("archive") || kind.includes("compressed") || kind.includes("tar")) return "archive"
        return "file"
    }
    source: preview.length ? preview : "qrc:/files-icons/" + asset + ".svg"
    sourceSize: Qt.size(width * Theme.uiScale, height * Theme.uiScale)
    fillMode: Image.PreserveAspectFit
    asynchronous: true
    onStatusChanged: if(status === Image.Error && preview.length) source = "qrc:/files-icons/" + asset + ".svg"
}
