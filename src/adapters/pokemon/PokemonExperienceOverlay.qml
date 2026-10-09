import QtQuick
import TrainerOS

Item {
    id: root
    required property var shell
        PartyPanel { anchors.fill: parent; shell: root.shell; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.experienceModel.centerFace && !shell.experienceModel.center.clinicOpen && !shell.experienceModel.center.shopsOpen && shell.experienceModel.party.section !== "saves" && shell.experienceModel.party.section !== "activities" }
        PokemonShop { anchors.fill: parent; shell: root.shell; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.experienceModel.centerFace && shell.experienceModel.center.shopsOpen }
        PokemonClinic { anchors.fill: parent; shell: root.shell; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.experienceModel.centerFace && shell.experienceModel.center.clinicOpen }
        CenterActivitiesPanel { anchors.fill: parent; shell: root.shell; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.experienceModel.centerFace && !shell.experienceModel.center.clinicOpen && !shell.experienceModel.center.shopsOpen && shell.experienceModel.party.section === "activities" }
        SaveCenterPanel { anchors.fill: parent; shell: root.shell; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.experienceModel.centerFace && !shell.experienceModel.center.clinicOpen && !shell.experienceModel.center.shopsOpen && shell.experienceModel.party.section === "saves" }
        Rectangle {
            id: merchantToast; property string message: ""
            anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 12
            width: Math.min(440, parent.width-24); height: 54; radius: 8; color: "#f8e5a9"; border.color: "#b28b42"; z: 1
            visible: toastTimer.running && shell.experienceModel.centerFace && !shell.menuOpen && !shell.drawerOpen && !shell.serviceOpen
            Text { anchors.fill: parent; anchors.margins: 10; text: merchantToast.message; font.pixelSize: 16; color: Theme.ink; wrapMode: Text.WordWrap; verticalAlignment: Text.AlignVCenter }
            Timer { id: toastTimer; interval: 4500 }
            Connections { target: shell.experienceModel.center; function onMerchantDiscovered(message) { merchantToast.message=message; toastTimer.restart() } }
        }
}
