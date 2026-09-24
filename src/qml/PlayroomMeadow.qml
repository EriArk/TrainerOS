import QtQuick

// Original tile-style scenery; cached independently from moving actors.
Item {
    id: meadow
    Canvas {
        id: tiles
        width: 456; height: Math.max(136, Math.ceil(meadow.height * width / Math.max(1, meadow.width)))
        transform: Scale { xScale: meadow.width/456; yScale: meadow.width/456 }
        onHeightChanged: requestPaint()
        smooth: false
        onPaint: {
            const c = getContext("2d")
            const h = height, bottomFence = h-23
            c.reset()
            function box(x,y,w,h,color) { c.fillStyle=color; c.fillRect(x,y,w,h) }
            function tuft(x,y,tone) { box(x,y,1,2,tone); box(x+2,y+1,1,2,tone); box(x+4,y,1,2,tone) }
            function tree(x,y) {
                box(x+5,y+21,26,6,"#548d55"); box(x+15,y+20,6,11,"#785836"); box(x+16,y+21,2,8,"#b58d50")
                box(x+9,y,17,4,"#315d4e"); box(x+4,y+4,27,5,"#315d4e"); box(x,y+9,35,13,"#315d4e")
                box(x+4,y+22,27,5,"#315d4e"); box(x+7,y+4,20,5,"#4e9360"); box(x+3,y+9,29,11,"#4e9360")
                box(x+7,y+20,20,3,"#4e9360"); box(x+10,y+4,11,3,"#92bd66"); box(x+5,y+10,13,4,"#79b35f")
                box(x+19,y+11,8,5,"#66a661"); box(x+9,y+17,6,3,"#79b35f")
            }
            function post(x,y) {
                box(x+2,y+1,5,15,"#72533b"); box(x+1,y,5,13,"#d2ae6b"); box(x+1,y,5,3,"#f3d88e"); box(x+2,y+3,1,8,"#edca7b")
            }
            function rail(x,y,w) {
                box(x,y+4,w,3,"#79583b"); box(x,y+2,w,3,"#e2bd74"); box(x,y+10,w,3,"#79583b"); box(x,y+8,w,3,"#d3a563")
                for(let p=x;p<=x+w-5;p+=19)post(p,y)
            }
            box(0,0,456,h,"#679f60")
            box(19,17,418,h-32,"#85bb65"); box(27,22,402,h-42,"#a0cb70")
            box(32,26,392,h-50,"#add77a")
            for(let i=0;i<Math.floor(h*1.8);++i) {
                const x=(i*73+19)%448, y=(i*41+3)%(h-4)
                tuft(x,y,i%3===0 ? "#90bf67" : "#78ae60")
            }
            box(81,h*.3,70,34,"#b9d98b"); box(73,h*.3+7,86,20,"#b9d98b")
            box(274,h*.58,75,23,"#b9d98b"); box(283,h*.58-7,60,36,"#b9d98b")
            for(let i=0;i<25;++i)tuft(82+(i*17)%66,h*.3+3+(i*13)%27,"#a5cb79")
            // Worn path enters through the open gate.
            box(211,h-32,34,32,"#c4a46a"); box(214,h-34,28,34,"#e0c58b"); box(218,h-36,20,36,"#e8cf98")
            for(let i=0;i<14;++i)box(216+(i*7)%23,h-28+(i*11)%26,2,1,"#c8ab75")
            rail(25,17,406); rail(25,bottomFence,178); rail(254,bottomFence,177)
            for(let y=26;y<bottomFence;y+=17) { post(25,y); post(425,y); box(28,y,2,17,"#d9b472"); box(428,y,2,17,"#d9b472") }
            for(let x=-10;x<465;x+=35)tree(x,-14-(x%3)*2)
            tree(-10,28); tree(432,33); tree(-15,h-56); tree(434,h-48)
            for(let i=0;i<20;++i) {
                const x=44+i*19, y=i%2 ? 32 : h-28
                box(x,y+2,1,4,"#578353"); box(x-1,y,3,3,i%3 ? "#f4da8a" : "#e6a1a2"); box(x,y+1,1,1,"#fff3c9")
            }
            box(188,h-11,12,3,"#6e8871"); box(190,h-13,8,3,"#b7bea0")
            box(255,h-10,13,3,"#6e8871"); box(257,h-12,9,3,"#b7bea0")
        }
    }
}
