import QtQuick

Item {
    id:root
    property string kind: "beaten"
    property bool bright: false
    Rectangle {
        anchors.centerIn:parent;width:parent.width*.72;height:width;radius:width/2
        rotation:-12;color:root.bright?"#263b48":"#26353c";border.color:"#a6bcb3";border.width:1
        gradient:Gradient {GradientStop {position:0;color:"#52646a"} GradientStop {position:.5;color:"#243238"} GradientStop {position:1;color:"#101a23"}}
        Rectangle {anchors.fill:parent;anchors.margins:5;radius:width/2;color:"transparent";border.width:1;border.color:"#547477"}
        Rectangle {x:width*.17;y:height*.10;width:parent.width*.48;height:parent.height*.16;radius:height/2;rotation:-20;color:"#effff4";opacity:.13}
    }
    Canvas {
        anchors.fill:parent
        onWidthChanged:requestPaint()
        onHeightChanged:requestPaint()
        onPaint:{
            const c=getContext("2d");c.reset();const s=Math.min(width,height),x=width/2,y=height/2;
            const g=c.createLinearGradient(0,y-s*.3,0,y+s*.3);g.addColorStop(0,"#fff2b6");g.addColorStop(.48,"#efc46c");g.addColorStop(1,"#b57d32");
            c.fillStyle=g;c.strokeStyle="#ffdf98";c.lineWidth=Math.max(1,s*.008);c.beginPath();
            for(let i=0;i<8;++i){const a=i*Math.PI/4-Math.PI/2,r=s*(i%2?.09:.29);if(i===0)c.moveTo(x+Math.cos(a)*r,y+Math.sin(a)*r);else c.lineTo(x+Math.cos(a)*r,y+Math.sin(a)*r)}
            c.closePath();c.fill();c.stroke();c.fillStyle="#fff3ca";c.beginPath();c.arc(x+s*.25,y-s*.22,s*.025,0,Math.PI*2);c.fill();
        }
    }
    Text {anchors.horizontalCenter:parent.horizontalCenter;y:parent.height*.76;text:root.kind==="outfit_slot"?"LOOK":root.kind==="clothing"?"WARDROBE":root.kind==="hardcore"?"HARDCORE":root.kind==="completed"?"100%":"VICTORY";color:"#f9df9f";font.pixelSize:Math.max(8,parent.width*.065);font.letterSpacing:2}
}
