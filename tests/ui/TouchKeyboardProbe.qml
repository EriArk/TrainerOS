import QtQuick

Window {
    id: window
    width: 960; height: 640; visible: true
    property int stage: 0
    property bool busy: false
    property bool passed: false
    QtObject {
        id: shell
        property var keyboard: null
        property bool menuOpen: false
        property string notice: ""
        function pressButton(button) { keyboard.cancel() }
    }
    Item {
        id: surface
        property bool active: true
        property bool mediaAllowed: false
        property var page: null
        property string error: ""
        property bool fullScreen: false
        function captureTab(id, view) {}
        function forgetTab(id) {}
        function reveal() {}
        function queueFieldCheck() {}
        function scrolled(delta) {}
        function openPopup(kind, rows) {}
        function ask(title, question, callback) { callback(false) }
    }
    Item {
        width: 960; height: window.height/scale; scale: window.width/960; transformOrigin: Item.TopLeft
    Loader {
        id: engine; width: parent.width; height: keyboard.live && panel.item ? parent.height-panel.item.occupiedHeight : parent.height
        onLoaded: {item.flow=browser;item.surface=surface;item.reconcile()}
    }
    Loader {
        id: panel; anchors.fill: parent
    }
    }
    function fail(message) { console.error("INPUT_FAILED",stage,message); step.stop(); Qt.exit(2) }
    function script(js, expected) {
        busy=true
        surface.page.runJavaScript(js, function(value) {
            if(expected!==undefined && value!==expected) {fail(JSON.stringify(value)+" != "+JSON.stringify(expected));return}
            busy=false;stage++
        })
    }
    function find(item, name) {
        if(item.objectName===name)return item
        for(let child of item.children||[]) {const result=find(child,name);if(result)return result}
        return null
    }
    function tap(id) {
        const key=find(panel.item,"key-"+id)
        if(!key){fail("keyboard key missing: "+id);return}
        browser.pointerEvent(key,key.width/2,key.height/2,1)
    }
    function ctrl(id) {tap("ctrl");tap(id)}
    Component.onCompleted: {
        keyboard.setLayout("touch")
        shell.keyboard=keyboard
        browser.openUrl(probeUrl)
        engine.source=engineSource
        panel.setSource(keyboardSource,{shell:shell})
    }
    function gesture(id, phase) {const key=find(panel.item,"key-"+id);pointerProbe.pointer(key,key.width/2,key.height/2,phase)}
    function accent(phase) {const p=find(panel.item,"keyboard-touch-surface");pointerProbe.pointer(p,p.accentLeft-p.x+72,p.accentTop-p.y+25,phase)}
    Timer {
        id: step; running: true; repeat: true; interval: 350
        onTriggered: {
            if(busy||!surface.page||surface.page.loading||!panel.item)return
            const web=surface.page
            if(keyboard.live)browser.editPageField(web,false)
            if(stage===0){web.forceActiveFocus();script("plain.value='';plain.focus();true",true)}
            else if(stage===1){browser.editPageField(web,false);if(!keyboard.live||!keyboard.touchLayout){fail("touch input did not open");return}stage++}
            else if(stage===2){tap("Q");stage++}
            else if(stage===3){window.contentItem.grabToImage(result=>result.saveToFile(probeData+"/touch.png"));script("plain.value","q")}
            else if(stage===4){gesture("Q",1);stage++}
            else if(stage===5){gesture("W",0);stage++}
            else if(stage===6){gesture("W",2);stage++}
            else if(stage===7)script("plain.value","qw")
            else if(stage===8){gesture("E",1);stage++}
            else if(stage===9){stage++}
            else if(stage===10){accent(0);window.contentItem.grabToImage(result=>result.saveToFile(probeData+"/accents.png"));stage++}
            else if(stage===11){accent(2);stage++}
            else if(stage===12)script("plain.value","qwé")
            else if(stage===13){tap("space");stage++}
            else if(stage===14){tap("space");stage++}
            else if(stage===15)script("plain.value","qwé. ")
            else if(stage===16){gesture("space",1);stage++}
            else if(stage===17){stage++}
            else if(stage===18){const key=find(panel.item,"key-space");pointerProbe.pointer(key,key.width/2-36,key.height/2,0);stage++}
            else if(stage===19){const key=find(panel.item,"key-space");pointerProbe.pointer(key,key.width/2-36,key.height/2,2);stage++}
            else if(stage===20)script("plain.selectionStart",2)
            else if(stage===21){tap("Q");stage++}
            else if(stage===22)script("plain.value","qwqé. ")
            else if(stage===23)script("plain.value='';plain.setSelectionRange(0,0);true",true)
            else if(stage===24){gesture("symbols",1);stage++}
            else if(stage===25){gesture("Q",0);stage++}
            else if(stage===26){gesture("2",0);stage++}
            else if(stage===27){gesture("2",2);stage++}
            else if(stage===28){tap("space");stage++}
            else if(stage===29){if(keyboard.symbolPage!==0){fail("quick number did not return to letters");return}script("plain.value","2 ")}
            else if(stage===30){gesture("shift",1);stage++}
            else if(stage===31){gesture("Q",0);stage++}
            else if(stage===32){gesture("Q",2);stage++}
            else if(stage===33)script("plain.value","2 Q")
            else if(stage===34){tap("shift");tap("shift");stage++}
            else if(stage===35){tap("Q");tap("Q");stage++}
            else if(stage===36)script("plain.value","2 QQQ")
            else if(stage===37){tap("layout");stage++}
            else if(stage===38){const button=find(panel.item,"keyboard-layout-mode");browser.pointerEvent(button,button.width/2,button.height/2,1);stage++}
            else if(stage===39){if(keyboard.touchLayout||!find(panel.item,"key-num-enter")){fail("PC mode missing");return}const button=find(panel.item,"keyboard-layout-mode");browser.pointerEvent(button,button.width/2,button.height/2,1);stage++}
            else if(stage===40){window.contentItem.grabToImage(result=>result.saveToFile(probeData+"/touch-options.png"));const button=find(panel.item,"keyboard-layout-back");browser.pointerEvent(button,button.width/2,button.height/2,1);stage++}
            else if(stage===41){if(!keyboard.touchLayout){fail("Touch mode lost");return}script("plain.value","2 QQQ")}
            else if(stage===42)script("document.querySelector('#readonly').focus();true",true)
            else if(stage===43){browser.editPageField(web,false);if(keyboard.open){fail("readonly retained keyboard");return}stage++}
            else if(stage===44)script("document.querySelector('#password').focus();true",true)
            else if(stage===45){browser.editPageField(web,true);stage++}
            else if(stage===46){tap("Q");stage++}
            else if(stage===47){tap("space");stage++}
            else if(stage===48){tap("space");stage++}
            else if(stage===49){if(keyboard.editableText.length){fail("password copied");return}script("document.querySelector('#password').value","q  ")}
            else {passed=true;console.log("INPUT_PASSED touch release/slide, accents, double space, held-space trackpad, quick numbers, Shift slide, Caps Lock, PC/Touch preferences and password boundaries");Qt.exit(0)}
        }
    }
}
