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
        keyboard.setLayout("pc")
        shell.keyboard=keyboard
        browser.openUrl(probeUrl)
        engine.source=engineSource
        panel.setSource(keyboardSource,{shell:shell})
    }
    Timer {
        id: step; running: true; repeat: true; interval: 350
        onTriggered: {
            if(busy||!surface.page||surface.page.loading||!panel.item)return
            const web=surface.page
            if(keyboard.live)browser.editPageField(web,false)
            if(stage===0){web.forceActiveFocus();script("document.querySelector('#plain').focus();document.querySelector('#plain').setSelectionRange(1,3);true",true)}
            else if(stage===1){browser.editPageField(web,false);if(!keyboard.live){fail("did not detect editable focus");return}stage++}
            else if(stage===2){
                const key=find(panel.item,"key-Q")
                if(!key){fail("keyboard key missing");return}
                browser.pointerEvent(key,key.width/2,key.height/2,1);stage++
            }
            else if(stage===3){window.contentItem.grabToImage(result=>result.saveToFile(probeData+"/input.png"));script("document.querySelector('#plain').value", "aqd")}
            else if(stage===4){keyboard.insertText("Ж🙂");stage++}
            else if(stage===5){keyboard.backspace();stage++}
            else if(stage===6)script("document.querySelector('#plain').value", "aqЖd")
            else if(stage===7){ctrl("A");stage++}
            else if(stage===8){keyboard.insertText("replace");stage++}
            else if(stage===9)script("document.querySelector('#plain').value+':'+window.events", "replace:4")
            else if(stage===10)script("document.querySelector('#rich').focus();true",true)
            else if(stage===11){keyboard.insertText("editor");stage++}
            else if(stage===12)script("document.querySelector('#rich').textContent", "editor")
            else if(stage===13)script("document.querySelector('#shadow').shadowRoot.querySelector('input').focus();true",true)
            else if(stage===14){keyboard.insertText("shadow");stage++}
            else if(stage===15)script("document.querySelector('#shadow').shadowRoot.querySelector('input').value", "shadow")
            else if(stage===16)script("document.querySelector('#frame').contentWindow.postMessage('focus','*');true",true)
            else if(stage===17){keyboard.insertText("frame");stage++}
            else if(stage===18)script("window.frameValue", "frame")
            else if(stage===19)script("document.querySelector('#password').focus();true",true)
            else if(stage===20){keyboard.insertText("secret");stage++}
            else if(stage===21){if(keyboard.editableText.length){fail("secret copied into keyboard");return}script("document.querySelector('#password').value", "secret")}
            else if(stage===22){tap("tab");stage++}
            else if(stage===23){keyboard.insertText("line1");tap("apply");keyboard.insertText("line2");stage++}
            else if(stage===24)script("document.querySelector('#multiline').value", "line1\nline2")
            else if(stage===25){browser.stopPageInput();stage++}
            else if(stage===26){browser.editPageField(web,false);if(keyboard.open){fail("Hide immediately reopened keyboard");return}stage++}
            else if(stage===27){browser.editPageField(web,true);if(!keyboard.open){fail("tap failed to reopen");return}stage++}
            else if(stage===28)script("document.querySelector('#readonly').focus();true",true)
            else if(stage===29){browser.editPageField(web,false);if(keyboard.open){fail("readonly field retained keyboard");return}stage++}
            else if(stage===30){browser.editPageField(null,false);keyboard.insertText("STALE");script("document.querySelector('#multiline').value", "line1\nline2")}
            else if(stage===31)script("document.querySelector('#plain').focus();true",true)
            else if(stage===32){browser.editPageField(web,true);ctrl("A");stage++}
            else if(stage===33){ctrl("C");stage++}
            else if(stage===34){ctrl("X");stage++}
            else if(stage===35)script("document.querySelector('#plain').value", "")
            else if(stage===36){ctrl("V");stage++}
            else if(stage===37)script("document.querySelector('#plain').value", "replace")
            else if(stage===38){ctrl("Z");stage++}
            else if(stage===39)script("document.querySelector('#plain').value", "")
            else if(stage===40){ctrl("Y");stage++}
            else if(stage===41)script("document.querySelector('#plain').value", "replace")
            else if(stage===42){tap("layout");stage++}
            else if(stage===43){const button=find(panel.item,"keyboard-number-pad");browser.pointerEvent(button,button.width/2,button.height/2,1);stage++}
            else if(stage===44){const button=find(panel.item,"keyboard-layout-back");browser.pointerEvent(button,button.width/2,button.height/2,1);stage++}
            else if(stage===45){
                if(keyboard.numberPad||keyboard.optionsOpen||!keyboard.live||find(panel.item,"key-num-0")){fail("layout switch lost live focus or retained number pad");return}
                const key=find(panel.item,"key-Q");if(key.width<44){fail("letters did not use freed width");return}
                window.contentItem.grabToImage(result=>result.saveToFile(probeData+"/letters.png"));stage++
            }
            else if(stage===46){stage++}
            else if(stage===47){tap("1");stage++}
            else if(stage===48)script("document.querySelector('#plain').value", "replace1")
            else if(stage===49){stage++}
            else if(stage===50){tap("shift");tap("shift");stage++}
            else if(stage===51){tap("Q");tap("Q");stage++}
            else if(stage===52)script("document.querySelector('#plain').value", "replace1QQ")
            else if(stage===53){tap("layout");stage++}
            else if(stage>=54&&stage<=56){
                const button=find(panel.item,"keyboard-"+["function-row","navigation-block","system-keys"][stage-54]);
                browser.pointerEvent(button,button.width/2,button.height/2,1);stage++
            }
            else if(stage===57){window.contentItem.grabToImage(result=>result.saveToFile(probeData+"/options.png"));stage++}
            else if(stage===58){const button=find(panel.item,"keyboard-layout-back");browser.pointerEvent(button,button.width/2,button.height/2,1);stage++}
            else if(stage===59){
                if(keyboard.functionRow||keyboard.navigationBlock||keyboard.systemKeys||find(panel.item,"key-num-0")||find(panel.item,"key-f1")||find(panel.item,"key-up")||find(panel.item,"key-win")){fail("hidden keyboard groups retained keys");return}
                if(find(panel.item,"key-Q").width<55){fail("compact keyboard did not reclaim width");return}
                window.contentItem.grabToImage(result=>result.saveToFile(probeData+"/compact.png"));stage++
            }
            else if(stage===60){tap("1");stage++}
            else if(stage===61)script("document.querySelector('#plain').value", "replace1QQ1")
            else {passed=true;console.log("INPUT_PASSED actual keyboard tap, selection, unicode, input events, contenteditable, shadow, cross-origin frame, password, tab, multiline, hide, readonly, stale input, Ctrl A/C/X/V/Z/Y, layout popup, digits without number pad, double Shift and repeated letters");Qt.exit(0)}
        }
    }
}
