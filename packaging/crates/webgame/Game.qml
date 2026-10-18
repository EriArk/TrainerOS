import QtQuick
import QtQuick.Window
import QtWebEngine
Window {
    id: window; width: 960; height: 540; visible: true; visibility: Window.FullScreen
    title: view.title || "Game"; color: "#faf8ef"
    WebEngineView {
        id: view
        anchors.fill: parent; focus: true; url: gameUrl
        zoomFactor: responsiveViewport ? 1 : Math.min(width/600,height/850)
        profile: WebEngineProfile {
            storageName: "game"; offTheRecord: false
            persistentStoragePath: gameData+"/web"; cachePath: gameData+"/cache"
        }
        settings.localContentCanAccessRemoteUrls: false
        // Reviewed offline packages load their own relative JS/CSS resources.
        settings.localContentCanAccessFileUrls: true
        onNewWindowRequested: function(request) { }
        onNavigationRequested: function(request) {
            if (request.url.toString().indexOf("file:") !== 0) request.action = WebEngineNavigationRequest.IgnoreRequest
        }
        onLoadingChanged: function(request) { if(request.status === WebEngineView.LoadSucceededStatus) forceActiveFocus() }
    }
}
