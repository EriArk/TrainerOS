import QtQuick
Item {
    id: root
    required property var shell
    property bool overlay: false
    function hints(h) { return visible && presenter.item && presenter.item.hints ? presenter.item.hints(h) : null }
    Loader {
        id: presenter
        anchors.fill: parent
        active: root.visible
        onActiveChanged: if(active) mount()
        function mount() { if(active && root.shell.experiencePresenter.toString()) setSource(root.shell.experiencePresenter,{shell:root.shell,overlay:root.overlay}) }
        Component.onCompleted: mount()
        Connections { target: root.shell; function onChanged() { if(presenter.source.toString() !== root.shell.experiencePresenter.toString())presenter.mount() } }
    }
    SocialIconButton {
        anchors.right: parent.right; anchors.rightMargin: 12; y: 10
        visible: !root.overlay && root.shell.page === 0
        icon: "dots-three"; label: "Game actions"
        onClicked: root.shell.openContext("game",root.shell.currentAdventureId)
    }
}
