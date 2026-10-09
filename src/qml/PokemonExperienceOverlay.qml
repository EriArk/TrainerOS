import QtQuick

Item {
    id: root
    required property var shell
        PartyPanel { anchors.fill: parent; shell: root.shell; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.centerFace && !shell.center.clinicOpen && !shell.center.shopsOpen && shell.party.section !== "saves" && shell.party.section !== "activities" }
        PokemonShop { anchors.fill: parent; shell: root.shell; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.centerFace && shell.center.shopsOpen }
        PokemonClinic { anchors.fill: parent; shell: root.shell; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.centerFace && shell.center.clinicOpen }
        CenterActivitiesPanel { anchors.fill: parent; shell: root.shell; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.centerFace && !shell.center.clinicOpen && !shell.center.shopsOpen && shell.party.section === "activities" }
        SaveCenterPanel { anchors.fill: parent; shell: root.shell; enabled: !shell.serviceOpen && !shell.drawerOpen; visible: (!shell.serviceOpen || shell.service === "settings") && shell.centerFace && !shell.center.clinicOpen && !shell.center.shopsOpen && shell.party.section === "saves" }
        Rectangle {
            id: merchantToast; property string message: ""
            anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 12
            width: Math.min(440, parent.width-24); height: 54; radius: 8; color: "#f8e5a9"; border.color: "#b28b42"; z: 1
            visible: toastTimer.running && shell.centerFace && !shell.menuOpen && !shell.drawerOpen && !shell.serviceOpen
            Text { anchors.fill: parent; anchors.margins: 10; text: merchantToast.message; font.pixelSize: 16; color: Theme.ink; wrapMode: Text.WordWrap; verticalAlignment: Text.AlignVCenter }
            Timer { id: toastTimer; interval: 4500 }
            Connections { target: shell.center; function onMerchantDiscovered(message) { merchantToast.message=message; toastTimer.restart() } }
        }
}
