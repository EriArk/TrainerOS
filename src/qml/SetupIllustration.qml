import QtQuick

// Original vector artwork, repainted only when a step or checked key changes.
Item {
    id: art
    property string stage: "welcome"
    property int checkedControls: 0
    onStageChanged: drawing.requestPaint()
    onCheckedControlsChanged: drawing.requestPaint()
    Canvas {
    id: drawing
    // The shell scales its logical surface 2x on both current handhelds.
    width: art.width*2; height: art.height*2; scale: .5; transformOrigin: Item.TopLeft
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const c = getContext("2d")
        c.reset(); c.scale(width/320, height/272)
        function box(x,y,w,h,r,fill,stroke) {
            c.beginPath(); c.moveTo(x+r,y); c.lineTo(x+w-r,y); c.quadraticCurveTo(x+w,y,x+w,y+r)
            c.lineTo(x+w,y+h-r); c.quadraticCurveTo(x+w,y+h,x+w-r,y+h)
            c.lineTo(x+r,y+h); c.quadraticCurveTo(x,y+h,x,y+h-r)
            c.lineTo(x,y+r); c.quadraticCurveTo(x,y,x+r,y); c.closePath()
            c.fillStyle=fill; c.fill(); if(stroke){c.strokeStyle=stroke;c.lineWidth=2;c.stroke()}
        }
        function circle(x,y,r,fill,stroke) {
            c.beginPath();c.arc(x,y,r,0,Math.PI*2);c.fillStyle=fill;c.fill()
            if(stroke){c.lineWidth=2;c.strokeStyle=stroke;c.stroke()}
        }
        function line(points,color,width) {
            c.beginPath();c.moveTo(points[0],points[1]);for(let i=2;i<points.length;i+=2)c.lineTo(points[i],points[i+1])
            c.strokeStyle=color;c.lineWidth=width;c.lineCap="round";c.lineJoin="round";c.stroke()
        }
        function label(x,y,text,size,color) {c.font="bold "+size+"px sans-serif";c.fillStyle=color;c.textAlign="center";c.fillText(text,x,y)}
        box(14,213,292,39,12,"#b0c4a4","")
        box(14,207,292,35,12,"#e9edca","#94ae94")
        line([33,227,89,219,149,228,215,218,285,224],"#a8cbb2",2)
        circle(268,41,25,"#f4d57d","")
        for(let i=0;i<3;i++) {
            c.beginPath();c.moveTo(16+i*104,132);c.lineTo(66+i*87,60+i*13);c.lineTo(140+i*80,151)
            c.fillStyle=["#bdd4b5","#a2c5ac","#c7dcc0"][i];c.fill()
        }
        if(stage==="clock") {
            circle(157,131,89,"#76978c","#355c56")
            circle(157,125,85,"#f6d680","#355c56")
            circle(157,125,71,"#fff9e5","#bdad78")
            for(let i=0;i<12;i++) {
                const angle=i*Math.PI/6
                line([157+58*Math.sin(angle),125-58*Math.cos(angle),157+64*Math.sin(angle),125-64*Math.cos(angle)],"#527569",3)
            }
            line([157,84,157,125,192,146],"#355c56",6)
            circle(157,125,7,"#db827b","#355c56")
        } else if(stage==="storage") {
            c.save();c.translate(78,50);c.rotate(-.12)
            box(0,5,133,171,12,"#315d60","");box(0,0,133,166,12,"#57969a","#24474c")
            box(12,42,109,80,5,"#fff3ce","#3b7173");label(67,70,"ADVENTURES",11,"#315354")
            circle(67,97,16,"#e8ae64","#bd824b");line([57,97,77,97],"#fff4dc",3)
            for(let i=0;i<5;i++)box(17+i*21,5,13,25,2,"#f1ca73","")
            label(67,149,"GAME LIBRARY",10,"#e5f4e4");c.restore()
            box(189,132,81,90,8,"#bb7c87","#78515d");box(198,146,63,45,4,"#f2d9a0","");label(229,175,"PLAY",14,"#664b5a")
        } else {
            box(34,82,252,137,23,"#285459","");box(34,72,252,139,23,"#4b9293","#284c50")
            box(40,77,240,126,19,"#81bcaf","");box(79,87,162,96,10,"#315b60","#264c50")
            box(86,94,148,82,5,"#fff3c9","")
            if(stage==="network") {
                for(let i=0;i<3;i++) {
                    c.beginPath();c.arc(160,156,15+i*18,Math.PI*1.2,Math.PI*1.8)
                    c.lineWidth=7;c.strokeStyle=["#4b8c89","#7fae91","#b7c997"][i];c.stroke()
                }
                circle(160,155,5,"#b58043","")
            } else {
                c.beginPath();c.moveTo(90,152);c.quadraticCurveTo(125,106,155,150);c.quadraticCurveTo(195,114,230,140)
                c.lineTo(230,174);c.lineTo(90,174);c.fillStyle="#b4cea0";c.fill()
                line([107,165,132,150,157,156,184,137,207,143],"#fbf2d5",8)
                circle(106,163,5,"#d28b76","#92524e");circle(208,139,6,"#ecc465","#a18243")
                line([186,136,186,109],"#55766b",2)
                c.beginPath();c.moveTo(187,109);c.lineTo(207,115);c.lineTo(187,122);c.fillStyle="#de907a";c.fill()
            }
            const coords=[[59,122],[59,154],[44,138],[74,138]]
            for(let i=0;i<4;i++) {
                const done=stage==="controls" && (checkedControls & (1<<i))
                box(coords[i][0]-7,coords[i][1]-7,14,14,3,done?"#f7cc60":"#34565b",done?"#ffe8a0":"#24454c")
            }
            const face=[["X",260,120,0],["Y",246,136,0],["A",274,136,16],["B",260,152,32]]
            for(let i=0;i<4;i++) {
                const f=face[i], done=stage==="controls" && (checkedControls & f[3])
                circle(f[1],f[2],8,done?"#f8cf62":["#a6c8e0","#dec286","#d99a96","#c2d99a"][i],done?"#ffeab0":"#3a6264")
                label(f[1],f[2]+3,f[0],8,"#294b4e")
            }
            box(124,193,30,4,2,"#477c76","");box(165,193,30,4,2,"#477c76","");circle(68,186,4,"#d6e88d","#53756d")
        }
        circle(42,40,5,"#d99886","");line([42,49,42,61],"#88a791",2)
        line([292,84,292,98],"#7eaaa0",2);line([285,91,299,91],"#7eaaa0",2)
    }
    }
}
