import QtQuick
import QtWebEngine

// Uses the actual BrowserEngine.qml and Chromium, in an isolated profile.
Window {
    id: window
    width: 960; height: 540; visible: true
    property string original: ""
    property int stage: 0
    function check(ok, message) {
        if (!ok) { console.error("LIFECYCLE_FAILED", message); Qt.exit(2) }
        return ok
    }
    Item {
        id: surface
        property bool active: true
        property bool mediaAllowed: false
        property var page: null
        property string error: ""
        property bool fullScreen: false
        property int forgotten: 0
        function captureTab(id, view) {}
        function forgetTab(id) { forgotten++ }
        function reveal() {}
        function queueFieldCheck() {}
        function scrolled(delta) {}
        function openPopup(kind, rows) {}
        function ask(title, question, callback) { callback(false) }
    }
    Loader {
        id: loader; anchors.fill: parent; visible: surface.active
        onLoaded: { item.flow=browser; item.surface=surface; item.reconcile(); step.start() }
    }
    Component.onCompleted: {
        original=browser.selected
        browser.newTab(); browser.newTab(); browser.select(original)
        loader.source=engineSource
    }
    Timer {
        id: step; interval: 800; repeat: true
        onTriggered: {
            const engine=loader.item
            if (stage===0) {
                if(!window.check(Object.keys(engine.views).length===1,"restored tabs eagerly loaded"))return
                engine.current.runJavaScript("window.perfMarker='kept'")
                browser.newTab(); stage=1
            } else if(stage===1) {
                if(!window.check(Object.keys(engine.views).length===2,"visited tab did not load"))return
                surface.active=false; interval=18000; stage=2
            } else if(stage===2) {
                if(!window.check(engine.views[original].lifecycleState===WebEngineView.LifecycleState.Frozen,"hidden tab was not frozen"))return
                surface.active=true; browser.select(original); interval=800; stage=3
            } else if(stage===3) {
                if(!window.check(engine.current.lifecycleState===WebEngineView.LifecycleState.Active,"selected tab did not resume"))return
                stop()
                engine.current.runJavaScript("window.perfMarker",function(value){
                    if(!window.check(value==="kept","tab DOM was lost"))return
                    browser.closeTab(original)
                    if(!window.check(surface.forgotten===1 && !engine.views[original],"closed tab retained"))return
                    console.log("LIFECYCLE_PASSED lazy restore, freeze, DOM resume, close")
                    Qt.exit(0)
                })
            }
        }
    }
}
