import QtQuick
import "WorldThemes.js" as Worlds

Item {
    id: root
    property var entry: ({})
    property bool selected: false
    property string portion: "whole"
    readonly property var palette: Worlds.theme(entry.id)
    Accessible.role: Accessible.Button
    Accessible.name: entry.name + ", " + entry.owned + " linked Adventures"
    onSelectedChanged: { scenery.requestPaint(); if(selected && visible)forceActiveFocus(Qt.OtherFocusReason) }
    onVisibleChanged: if(selected && visible)forceActiveFocus(Qt.OtherFocusReason)
    onEntryChanged: scenery.requestPaint()
    Canvas {
        id: scenery; anchors.fill: parent
        onPaint: {
            const c=getContext("2d"), w=width, h=height
            c.reset()
            function edge(inset) {
                const l=inset,r=w-inset,t=inset,b=h-inset,rad=10
                c.beginPath()
                if(root.portion==="upper") {
                    c.moveTo(l,t+rad);c.quadraticCurveTo(l,t,l+rad,t);c.lineTo(r-rad,t);c.quadraticCurveTo(r,t,r,t+rad)
                    c.lineTo(r,h*.40-inset);c.lineTo(l,h*.60-inset)
                } else if(root.portion==="lower") {
                    c.moveTo(l,h*.60+inset);c.lineTo(r,h*.40+inset);c.lineTo(r,b-rad);c.quadraticCurveTo(r,b,r-rad,b)
                    c.lineTo(l+rad,b);c.quadraticCurveTo(l,b,l,b-rad)
                } else {
                    c.moveTo(l+rad,t);c.lineTo(r-rad,t);c.quadraticCurveTo(r,t,r,t+rad);c.lineTo(r,b-rad)
                    c.quadraticCurveTo(r,b,r-rad,b);c.lineTo(l+rad,b);c.quadraticCurveTo(l,b,l,b-rad);c.lineTo(l,t+rad);c.quadraticCurveTo(l,t,l+rad,t)
                }
                c.closePath()
            }
            edge(1);c.fillStyle=Qt.darker(root.palette[0],1.65);c.fill()
            c.save();edge(3);c.clip()
            const wash=c.createLinearGradient(0,0,0,h)
            wash.addColorStop(0,Qt.lighter(root.palette[0],1.16));wash.addColorStop(1,root.palette[0]);c.fillStyle=wash;c.fillRect(0,0,w,h)
            c.scale(w/280,h/150);c.strokeStyle=root.palette[1];c.fillStyle=root.palette[1];c.lineWidth=2;c.globalAlpha=.25
            function line(points,fill) {
                c.beginPath();c.moveTo(points[0][0],points[0][1]);for(let i=1;i<points.length;++i)c.lineTo(points[i][0],points[i][1]);
                if(fill){c.closePath();c.fill()}else c.stroke()
            }
            const motif=root.palette[2]
            if(motif==="mountains") {
                line([[-20,115],[65,18],[102,62],[159,4],[248,111],[300,70],[300,160],[-20,160]],true)
                line([[31,58],[65,18],[83,48],[67,41],[55,53],[48,44]],false)
                line([[127,44],[159,4],[190,42],[169,32],[157,44],[147,33]],false)
                line([[-10,136],[100,75],[140,108],[205,68],[290,116]],false)
            } else if(motif==="waves" || motif==="dunes" || motif==="fields") {
                for(let y=20;y<170;y+=26){c.beginPath();c.moveTo(-10,y);c.bezierCurveTo(60,y-38,98,y+35,155,y);c.bezierCurveTo(200,y-27,237,y+28,295,y-8);c.stroke()}
                if(motif==="waves") {line([[172,52],[190,21],[220,30],[239,54],[211,63]],true);line([[32,92],[49,75],[70,81],[82,97]],true)}
                if(motif==="fields") {c.beginPath();c.arc(225,34,20,0,Math.PI*2);c.stroke();line([[175,150],[164,70],[190,2]],false)}
            } else if(motif==="city") {
                const heights=[51,29,71,96,46,81,118,66]
                for(let i=0;i<8;++i){const x=i*39-10,y=135-heights[i];c.strokeRect(x,y,29,heights[i]);for(let j=y+8;j<120;j+=13){c.fillRect(x+6,j,4,4);c.fillRect(x+18,j,4,4)}}
                line([[-10,107],[62,87],[130,110],[195,90],[290,110]],false)
            } else if(motif==="forest" || motif==="shrine") {
                for(let i=0;i<6;++i){const x=i*55-5;line([[x-27,112],[x,20+(i%2)*15],[x+30,112]],true);line([[x,54],[x,150]],false)}
                if(motif==="shrine") {c.globalAlpha=.34;c.lineWidth=5;line([[104,113],[104,51],[205,51],[205,112]],false);line([[86,39],[104,46],[205,46],[222,36]],false);line([[116,70],[194,70]],false)}
            } else if(motif==="leaves") {
                for(let i=0;i<6;++i){c.save();c.translate(i*58-12,80+(i%2)*39);c.rotate(-.8+i*.3);c.beginPath();c.moveTo(0,30);c.bezierCurveTo(-42,0,-22,-52,0,-61);c.bezierCurveTo(31,-27,42,4,0,30);c.fill();line([[0,30],[0,-50]],false);c.restore()}
            } else if(motif==="stadium") {
                c.save();c.translate(153,89);c.scale(1,.48);for(let r=42;r<153;r+=21){c.beginPath();c.arc(0,0,r,0,Math.PI*2);c.stroke()}c.restore()
                for(let i=0;i<7;++i)line([[i*48-20,113],[i*48+6,20],[i*48+34,113]],false)
            } else if(motif==="garden") {
                c.save();c.translate(158,62);for(let i=0;i<10;++i){c.rotate(Math.PI/5);c.beginPath();c.moveTo(0,0);c.bezierCurveTo(-35,-37,-10,-74,0,-83);c.bezierCurveTo(28,-57,35,-25,0,0);c.stroke()}c.restore()
                c.strokeRect(20,17,240,117)
            } else {
                c.beginPath();c.arc(222,32,26,0,Math.PI*2);c.stroke()
                line([[-5,135],[61,111],[40,61],[101,38],[153,77],[206,97],[289,51]],false)
                line([[-10,118],[61,81],[95,120],[152,13],[193,42],[243,5],[286,57]],false)
                for(let i=0;i<4;++i){c.beginPath();c.arc(40+i*63,61+(i%2)*24,5,0,Math.PI*2);c.fill()}
            }
            c.restore()
            // The label is printed on a quiet part of the same material.
            c.save();edge(3);c.clip()
            const textWash=c.createLinearGradient(0,h*.48,0,h)
            textWash.addColorStop(0,"#00ffffff");textWash.addColorStop(1,"#cceef1dc")
            c.fillStyle=textWash;c.fillRect(0,0,w,h);c.restore()
            edge(4);c.strokeStyle="#80ffffff";c.lineWidth=1;c.stroke()
            if(root.selected) {
                for(let i=7;i>=0;--i){edge(3+i);c.strokeStyle="rgba(255,184,28,"+(0.08+(7-i)*.025)+")";c.lineWidth=3;c.stroke()}
                edge(2);c.strokeStyle="#ffbe29";c.lineWidth=4;c.stroke();edge(4);c.strokeStyle="#fff7ba";c.lineWidth=1.5;c.stroke()
            }
        }
    }
    Column {
        x: 17; width: parent.width-34; spacing: root.portion==="whole" ? 4 : 1
        y: root.portion==="upper" ? 13 : root.portion==="lower" ? parent.height-55 : parent.height-61
        Text {
            width: parent.width; text: root.entry.name || ""; textFormat: Text.PlainText
            font.family: Theme.displayFamily; font.pixelSize: root.portion==="whole" ? 27 : 20; font.bold: true
            horizontalAlignment: root.portion==="lower" ? Text.AlignRight : Text.AlignLeft
            color: "#203e37"; elide: Text.ElideRight
        }
        Text {
            width: parent.width; text: root.entry.count ? root.entry.owned + " / " + root.entry.count + " Adventures" : "No Adventures yet"
            horizontalAlignment: root.portion==="lower" ? Text.AlignRight : Text.AlignLeft
            color: "#3c594c"; font.pixelSize: 12; elide: Text.ElideRight
        }
    }
}
