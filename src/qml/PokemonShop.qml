import QtQuick

Item {
    id: root
    required property var shell
    readonly property var shop: shell.center
    readonly property var choice: shop.shopSelection
    readonly property bool browsing: shop.shopRoute === "merchants"
    readonly property bool checkout: shop.shopRoute === "confirm" || shop.shopRoute === "receipt"
    readonly property bool active: visible && !shell.menuOpen && !shell.drawerOpen && !shell.notice.length
    Item {
        anchors.fill: parent; anchors.margins: Theme.panelInset; anchors.topMargin: Theme.contentTopInset
        PageHeader { id: header; compact: true; title: "Shops & Traders"; subtitle: root.shop.shopGroup || root.shop.title; trailing: root.shop.shopBalance < 0 ? "" : (root.choice.currency || "₽") + " " + root.shop.shopBalance }
        Rectangle {
            id: room; y: header.height; width: parent.width; height: parent.height-y; color: "#e7f1eb"; clip: true
            Canvas {
                id: scene; x: 272; width: parent.width-x; height: 119
                onWidthChanged: requestPaint()
                onPaint: {
                    const c=getContext("2d");c.reset();c.scale(3,3)
                    const w=width/3
                    function b(x,y,w,h,color){c.fillStyle=color;c.fillRect(Math.round(x),y,Math.round(w),h)}
                    b(0,0,w,40,"#b1d4da");b(0,1,w,2,"#e5faf0");b(0,34,w,6,"#efdcb9")
                    for(const x of [8,w-53]){
                        b(x,5,45,24,"#516f8b");b(x+2,7,41,20,"#edf1db")
                        for(let row=0;row<2;++row)for(let col=0;col<5;++col){
                            b(x+5+col*7,9+row*10,4,6,["#eaa48c","#d6afce","#86b8b3","#c3d68e","#e8c565"][col]);
                            b(x+6+col*7,8+row*10,2,2,"#fbf4d4")
                        }
                        b(x+2,17,41,2,"#81999f")
                    }
                    const mid=w/2
                    b(mid-12,18,24,14,"#e8f1dd");b(mid-4,20,8,11,"#6596ae")
                    b(mid-7,7,14,13,"#f1ccab");b(mid-8,4,16,6,"#626184");b(mid-6,3,12,3,"#626184")
                    b(mid-4,12,2,2,"#434b62");b(mid+3,12,2,2,"#434b62");b(mid-1,17,3,1,"#b87c72")
                    b(mid-34,29,68,10,"#739ba7");b(mid-36,27,72,4,"#fff0cf");b(mid-32,32,64,2,"#91c3c7")
                    b(mid+18,22,12,6,"#566d80");b(mid+20,23,8,2,"#a5e2cd")
                }
            }
            Rectangle { x: 0; width: 264; height: parent.height; color: "#d1e0dc"; border.color: "#95b0ad" }
            ListView {
                id: stores; objectName: "shop-merchants"; x: 12; y: 12; width: 240; height: parent.height-24; clip: true
                model: root.shop.merchants; currentIndex: root.shop.merchantIndex; spacing: 7
                onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                delegate: CapButton {
                    required property int index; required property var modelData
                    objectName: "shop-merchant-"+index; width: stores.width; height: 51; label: modelData.name.replace(" Poké Mart", ""); detail: modelData.detail
                    textSize: root.browsing && !root.shop.shopGroup.length ? 15 : 17; tint: modelData.discovered ? Theme.blue : "#c8cfce"
                    selected: root.active && root.browsing && index===root.shop.merchantIndex
                    onActivated: if(root.browsing)root.shop.shopActivate(index)
                }
            }
            Text {
                x: 295; y: 146; width: parent.width-x-30; visible: root.browsing
                text: root.choice.name ? root.choice.name + "\n\nWelcome! What can I get for you?" : "More shops await on your journey."
                font.family: Theme.displayFamily; font.pixelSize: 23; color: Theme.ink; wrapMode: Text.WordWrap
            }
            ListView {
                id: stock; x: 286; y: 131; width: (parent.width-x)*0.59; height: parent.height-y-17; clip: true
                visible: !root.browsing; model: root.shop.shopStock; currentIndex: root.shop.stockIndex; spacing: 7
                onCurrentIndexChanged: positionViewAtIndex(currentIndex,ListView.Contain)
                delegate: CapButton {
                    required property int index; required property var modelData
                    objectName: "shop-stock-"+index; width: stock.width; height: 48; label: modelData.name; detail: (root.choice.currency || "₽")+" "+modelData.price
                    tint: modelData.pocket === "Poké Balls" ? "#afd5e8" : index%2 ? "#cdddab" : "#efce9b"
                    selected: root.active && !root.browsing && !root.checkout && index===root.shop.stockIndex
                    onActivated: root.shop.shopActivate(index)
                }
            }
            MountedPanel {
                x: stock.x+stock.width+15; y: 131; width: parent.width-x-15; height: parent.height-y-17
                color: "#fff1d2"; visible: !root.browsing
                Text { x: 14; y: 13; width: parent.width-28; text: root.choice.item || ""; font.family: Theme.displayFamily; font.pixelSize: 21; color: Theme.ink; wrapMode: Text.WordWrap }
                Text { x: 14; y: 64; text: (root.choice.kind === "decoration" ? "Owned  " : "In Bag  ") + (root.choice.owned || 0); font.pixelSize: 14; color: Theme.muted }
                Text { x: 14; y: 91; text: "× " + root.shop.quantity; font.family: Theme.displayFamily; font.pixelSize: 31; color: "#527a83" }
                Text { x: 14; y: 134; text: "Total   " + (root.choice.currency || "₽") + " " + (root.choice.total || 0); font.pixelSize: 18; font.bold: true; color: Theme.ink }
                Text { x: 14; y: 167; width: parent.width-28; text: root.shop.shopMessage || root.choice.pocket || ""; font.pixelSize: 13; color: Theme.muted; wrapMode: Text.WordWrap }
            }
            Text {
                x: 288; y: parent.height-54; width: parent.width-x-20; height: 48; visible: root.browsing
                text: root.shop.shopMessage; font.pixelSize: 16; color: Theme.ink; wrapMode: Text.WordWrap
            }
            Rectangle {
                anchors.fill: parent; color: "#690f303a"; visible: root.checkout
                MountedPanel {
                    anchors.centerIn: parent; width: 490; height: 238; color: "#fff1d2"
                    Text { x: 24; y: 20; width: parent.width-48; text: "Your purchase"; font.family: Theme.displayFamily; font.pixelSize: 27; color: Theme.ink }
                    Text { x: 24; y: 70; width: parent.width-48; height: 96; text: root.shop.shopRoute === "confirm" ? root.shop.quantity+" × "+root.choice.item+"\nTotal  "+root.choice.currency+" "+root.choice.total+"     ·     Remaining  "+(root.shop.shopBalance-root.choice.total) : root.shop.shopMessage; font.pixelSize: 21; color: Theme.ink; wrapMode: Text.WordWrap }
                    CapButton { objectName: "shop-checkout"; x: 24; y: 177; width: parent.width-48; height: 43; label: root.shop.busy ? "Packing your purchase…" : root.shop.shopRoute === "confirm" ? "Confirm purchase" : "Continue shopping"; tint: Theme.yellow; selected: root.active; enabled: !root.shop.busy; onActivated: root.shop.shopActivate(0) }
                }
            }
        }
    }
}
