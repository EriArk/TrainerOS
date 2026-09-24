import QtQuick
import "PlatformThemes.js" as Platforms

Item {
    id: root
    property var entry: ({})
    property bool selected: false
    readonly property var cardPalette: Platforms.theme(entry.id)
    signal activated()
    Accessible.role: Accessible.Button
    Accessible.name: entry.name + ", " + entry.count + " Adventures"
    onSelectedChanged: { scene.requestPaint(); if(selected && visible)forceActiveFocus(Qt.OtherFocusReason) }
    onVisibleChanged: if(selected && visible)forceActiveFocus(Qt.OtherFocusReason)
    onEntryChanged: scene.requestPaint()
    Canvas {
        id: scene; anchors.fill: parent
        onPaint: {
            const c=getContext("2d"),w=width,h=height,p=root.cardPalette
            c.reset()
            function round(x,y,rw,rh,r) {
                c.beginPath();c.moveTo(x+r,y);c.lineTo(x+rw-r,y);c.quadraticCurveTo(x+rw,y,x+rw,y+r)
                c.lineTo(x+rw,y+rh-r);c.quadraticCurveTo(x+rw,y+rh,x+rw-r,y+rh)
                c.lineTo(x+r,y+rh);c.quadraticCurveTo(x,y+rh,x,y+rh-r);c.lineTo(x,y+r);c.quadraticCurveTo(x,y,x+r,y);c.closePath()
            }
            function polygon(points,color) {c.beginPath();c.moveTo(points[0][0],points[0][1]);for(let i=1;i<points.length;++i)c.lineTo(points[i][0],points[i][1]);c.closePath();c.fillStyle=color;c.fill()}
            function material(x,y,rw,rh,r,color) {
                round(x+3,y+5,rw,rh,r);c.fillStyle="#9910182b";c.fill()
                round(x,y,rw,rh,r);const g=c.createLinearGradient(x,y,x+rw*.6,y+rh)
                g.addColorStop(0,Qt.lighter(color,1.65));g.addColorStop(.35,color);g.addColorStop(1,Qt.darker(color,1.7));c.fillStyle=g;c.fill()
                c.strokeStyle=Qt.lighter(color,1.8);c.lineWidth=1.5;c.stroke()
            }
            function circle(x,y,r,color) {c.beginPath();c.arc(x,y,r,0,Math.PI*2);c.fillStyle=color;c.fill();c.strokeStyle="#55ffffff";c.lineWidth=1;c.stroke()}
            function screen(x,y,rw,rh) {material(x,y,rw,rh,3,"#111c32");polygon([[x+4,y+rh-5],[x+rw*.35,y+rh*.3],[x+rw*.65,y+rh*.65],[x+rw-4,y+8],[x+rw-4,y+rh-5]],p[1]);c.fillStyle="#8ceef9";c.fillRect(x+6,y+5,6,4)}
            function cross(x,y) {material(x,y,7,22,2,"#222b3c");material(x-7,y+7,21,7,2,"#222b3c")}
            round(2,2,w-4,h-4,12);c.save();c.clip()
            const bg=c.createLinearGradient(0,h,w,0);bg.addColorStop(0,"#101529");bg.addColorStop(.52,p[0]);bg.addColorStop(1,p[1]);c.fillStyle=bg;c.fillRect(0,0,w,h)
            c.scale(w/280,h/156)
            polygon([[70,0],[138,0],[246,156],[187,156]],"#24ffffff")
            polygon([[130,0],[160,0],[267,156],[237,156]],"#16ffffff")
            c.lineWidth=1;c.strokeStyle="#36ffffff"
            for(let y=92;y<156;y+=14){c.beginPath();c.moveTo(0,y);c.lineTo(280,y);c.stroke()}
            for(let x=-280;x<500;x+=55){c.beginPath();c.moveTo(185,53);c.lineTo(x,156);c.stroke()}
            c.save();c.translate(168,53);c.rotate(-.13)
            const kind=p[3],body=p[2]
            if(kind==="keyboard") {
                material(-72,-26,147,82,9,body)
                for(let row=0;row<4;++row)for(let key=0;key<12;++key)material(-62+key*10,row*11-11,8,8,1,key===11?"#e88e57":"#514038")
                material(-36,36,75,8,2,"#514038")
            } else if(kind==="vertical") {
                material(-29,-42,65,99,10,body);screen(-22,-32,50,42);cross(-13,20);circle(21,26,6,"#b92c64");circle(10,37,6,"#b92c64")
                for(let i=0;i<4;++i){c.fillStyle=Qt.darker(body,2);c.fillRect(9+i*5,46,2,7)}
            } else if(kind==="dual") {
                material(-47,-40,102,55,7,body);screen(-37,-34,82,42);material(-50,18,108,49,7,body);screen(-23,25,54,33)
                cross(-37,30);circle(42,30,4,"#ff617c");circle(47,39,4,"#42dcda");material(-47,12,102,6,3,Qt.darker(body,1.25))
            } else if(kind==="portable" || kind==="switch") {
                material(-74,-14,155,70,18,body);screen(-40,-6,87,52)
                if(kind==="switch"){material(-74,-14,29,70,12,"#1bcaf2");material(52,-14,29,70,12,"#fc485e")}
                cross(-61,11);circle(67,11,5,"#ff777a");circle(59,23,5,"#52eae5");circle(67,35,5,"#ffe066")
                if(kind==="switch")circle(-59,1,8,"#192339")
            } else if(kind==="cube") {
                polygon([[-53,-19],[16,-42],[70,-14],[0,12]],Qt.lighter(body,1.5));polygon([[-53,-19],[0,12],[0,68],[-53,35]],Qt.darker(body,1.15));polygon([[0,12],[70,-14],[70,41],[0,68]],body)
                c.save();c.translate(7,-16);c.scale(1,.42);circle(0,0,29,"#252143");circle(0,0,19,"#564187");c.restore()
                for(let i=0;i<4;++i)circle(12+i*13,39-i*5,4,"#242238")
            } else if(kind==="addon") {
                material(-68,23,138,42,12,Qt.darker(body,1.2))
                material(-25,-6,55,46,8,body);material(-45,-32,96,43,17,body)
                material(-33,-23,72,12,4,"#141925");c.fillStyle="#080c15";c.fillRect(-27,-20,60,5)
                c.fillStyle=p[1];c.fillRect(-37,0,78,3);circle(-49,45,5,"#e95065")
                for(let i=0;i<4;++i){c.fillStyle="#17202c";c.fillRect(39,35+i*5,22,2)}
                material(-25,50,23,8,2,"#111a29");material(7,50,23,8,2,"#111a29")
            } else if(kind==="tower") {
                polygon([[-35,-40],[12,-49],[45,-33],[2,-22]],Qt.lighter(body,1.7));polygon([[2,-22],[45,-33],[45,55],[2,69]],Qt.darker(body,1.5));material(-35,-40,38,102,3,body)
                c.fillStyle=p[1];c.fillRect(-29,-18,3,49);c.fillStyle="#080f22";c.fillRect(-16,-29,5,64);circle(-22,51,3,p[1])
                for(let i=0;i<7;++i){c.fillStyle="#17273b";c.fillRect(9,6+i*6,28,2)}
            } else if(kind==="arcade") {
                polygon([[-45,-42],[32,-42],[43,-29],[28,16],[57,32],[47,70],[-57,70],[-57,32],[-33,16],[-49,-28]],Qt.darker(body,1.2))
                material(-45,-42,81,15,3,p[1]);screen(-33,-21,64,44);material(-50,27,99,24,4,body)
                c.fillStyle="#ccdff7";c.fillRect(-30,27,4,13);circle(-28,23,7,p[1]);circle(8,36,5,"#fc5686");circle(24,36,5,"#f9d23f");circle(38,36,5,"#55deeb")
                polygon([[-47,54],[40,54],[33,70],[-43,70]],p[1])
            } else if(kind==="pad" || kind==="trident") {
                material(-68,-10,134,54,23,body);material(-62,13,31,45,12,body);material(35,13,31,45,12,body)
                if(kind==="trident")material(-10,15,27,60,12,body)
                cross(-46,1);circle(36,10,8,"#4abbf2");circle(48,27,8,"#e34871");circle(48,-2,6,"#63d389");circle(59,10,6,"#ffdf5a");circle(3,25,9,"#8c91ad")
            } else {
                material(-62,-25,129,85,10,body)
                if(kind==="disc") {c.save();c.translate(0,12);c.scale(1,.72);circle(0,0,36,Qt.darker(body,1.4));circle(0,0,30,body);circle(0,0,7,p[1]);c.restore()}
                else {material(-41,-19,89,24,3,Qt.darker(body,1.7));c.fillStyle="#132137";c.fillRect(-33,-12,70,5)}
                for(let i=0;i<4;++i){c.fillStyle=Qt.darker(body,2);c.fillRect(45,0+i*6,13,2)}
                circle(-45,39,6,p[1]);circle(-27,39,5,"#f76a66");material(-31,48,57,9,2,Qt.darker(body,2))
            }
            c.restore()
            const shade=c.createLinearGradient(0,82,0,156);shade.addColorStop(0,"#00101829");shade.addColorStop(1,"#f5101829");c.fillStyle=shade;c.fillRect(0,75,280,81)
            c.restore();round(2,2,w-4,h-4,12);c.lineWidth=2;c.strokeStyle="#67889baa";c.stroke()
            if(root.selected){for(let i=7;i>=0;--i){round(3+i,3+i,w-6-2*i,h-6-2*i,10);c.lineWidth=3;c.strokeStyle="rgba(255,195,47,"+(.08+(7-i)*.03)+")";c.stroke()}round(2,2,w-4,h-4,12);c.lineWidth=4;c.strokeStyle="#ffcc3d";c.stroke()}
        }
    }
    Column {
        x: 16; y: parent.height-56; width: parent.width-32; spacing: 3
        Text {width: parent.width; text: root.entry.name || ""; font.family: Theme.displayTypeface.name; font.pixelSize: 22; font.bold: true; color: "#ffffff"; elide: Text.ElideRight}
        Text {text: (root.entry.count || 0)+" Adventures"; font.pixelSize: 12; color: "#d1e4f4"}
    }
    MouseArea {anchors.fill: parent; onClicked: root.activated()}
}
