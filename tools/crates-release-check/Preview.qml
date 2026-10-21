import QtQuick
import QtQuick.Window

Window {
    width: 960; height: 540; visible: true; color: Theme.paper
    CratesCataloguePage { anchors.fill: parent; anchors.topMargin: 24; flow: catalogue; active: true }
    CratesProjectPopup { anchors.fill: parent; flow: catalogue; visible: catalogue.detailOpen }
}
