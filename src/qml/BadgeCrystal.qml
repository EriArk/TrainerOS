import QtQuick

Item {
    id: root
    required property int badgeIndex
    required property bool earned
    property string badgeSet: ""
    implicitWidth: 39
    implicitHeight: 46
    Accessible.role: Accessible.StaticText
    Accessible.name: "Badge " + (badgeIndex + 1) + (earned ? " · earned" : " · not earned")

    // Original cut-crystal interpretations, drawn as vectors rather than game sprites.
    readonly property var kantoCuts: [
        [[28,18],[62,13],[84,33],[81,66],[59,86],[25,78],[13,49]],
        [[50,9],[76,41],[84,61],[72,80],[50,89],[28,80],[16,61],[24,41]],
        [[50,8],[62,31],[85,21],[76,44],[94,53],[73,63],[80,85],[57,75],[43,93],[36,72],[13,78],[24,56],[8,41],[34,37]],
        [[50,8],[62,24],[81,20],[80,40],[94,51],[79,63],[81,83],[61,79],[50,94],[38,78],[18,83],[22,62],[7,51],[23,39],[20,20],[39,24]],
        [[50,29],[68,14],[87,31],[86,52],[50,89],[14,52],[13,31],[32,14]],
        [[31,13],[69,13],[87,32],[87,69],[69,88],[31,88],[13,69],[13,32]],
        [[53,8],[60,38],[76,24],[87,55],[75,80],[49,90],[24,78],[13,55],[33,29],[34,53]],
        [[78,9],[85,44],[66,77],[27,89],[16,58],[34,28]]
    ]
    readonly property var hoennCuts: [
        [[31,12],[75,21],[88,54],[65,85],[24,79],[12,43]],
        [[19,18],[42,16],[50,33],[58,16],[81,18],[90,49],[74,83],[52,73],[28,88],[10,51]],
        [[51,9],[78,30],[62,47],[87,63],[53,89],[24,71],[39,53],[14,34]],
        [[51,10],[79,32],[88,57],[72,84],[50,77],[28,84],[12,57],[21,32]],
        [[14,26],[36,26],[50,11],[64,26],[86,26],[88,67],[63,67],[50,89],[37,67],[12,67]],
        [[85,13],[79,49],[61,79],[24,88],[11,70],[37,55],[22,49],[51,39],[42,28]],
        [[50,9],[63,28],[85,25],[78,48],[92,65],[69,71],[62,92],[44,78],[22,86],[23,63],[8,48],[30,39],[29,17]],
        [[30,13],[48,35],[61,10],[83,43],[87,66],[73,86],[46,88],[20,75],[12,53]]
    ]
    readonly property var colors: badgeSet === "hoenn"
        ? ["#a0aab5", "#e26446", "#f4c641", "#e76a38", "#aaaec4", "#79cdd1", "#e1ad40", "#4b9bdd"]
        : ["#9babc0", "#4bbae5", "#edbd36", "#bc80d9", "#e98aae", "#e5b745", "#df6451", "#65ba77"]
    readonly property var cut: (badgeSet === "hoenn" ? hoennCuts : kantoCuts)[Math.max(0, Math.min(7, badgeIndex))]
    readonly property color jewel: colors[Math.max(0, Math.min(7, badgeIndex))]
    onEarnedChanged: if (crystal) crystal.requestPaint()
    onCutChanged: if (crystal) crystal.requestPaint()
    onJewelChanged: if (crystal) crystal.requestPaint()

    Canvas {
        id: crystal
        width: root.width * 2; height: root.height * 2
        scale: .5; transformOrigin: Item.TopLeft
        antialiasing: true
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.scale(width / 100, height / 112);
            const pts = root.cut;
            const path = function(points, dy) {
                ctx.beginPath();
                ctx.moveTo(points[0][0], points[0][1] + dy);
                for (let i = 1; i < points.length; ++i) ctx.lineTo(points[i][0], points[i][1] + dy);
                ctx.closePath();
            };
            ctx.lineJoin = "round";
            // The shaped recess and its bottom lip anchor each jewel in the tray.
            path(pts, 8); ctx.lineWidth = 9; ctx.strokeStyle = "#fafff5"; ctx.stroke();
            path(pts, 3); ctx.lineWidth = 9; ctx.strokeStyle = "#a4b5a4"; ctx.stroke();
            ctx.fillStyle = "#c0cdbc"; ctx.fill();
            if (!root.earned) {
                const shade = ctx.createLinearGradient(0, 16, 0, 88);
                shade.addColorStop(0, "#a2b09f"); shade.addColorStop(1, "#dbe4d4");
                path(pts, 3); ctx.fillStyle = shade; ctx.fill();
                return;
            }
            path(pts, 6); ctx.lineWidth = 3; ctx.strokeStyle = "#52685f"; ctx.stroke();
            ctx.fillStyle = "#617668"; ctx.fill();
            path(pts, 0); ctx.lineWidth = 3; ctx.strokeStyle = Qt.darker(root.jewel, 1.8); ctx.stroke();
            ctx.fillStyle = root.jewel; ctx.fill();
            const inner = pts.map(function(p) { return [50 + (p[0] - 50) * .59, 47 + (p[1] - 47) * .59]; });
            for (let i = 0; i < pts.length; ++i) {
                const next = (i + 1) % pts.length;
                path([pts[i], pts[next], inner[next], inner[i]], 0);
                if (root.badgeSet !== "hoenn" && root.badgeIndex === 3) {
                    const spectrum = ["#f4c556", "#ed8f57", "#e9778f", "#b680cb", "#799bd9", "#61beb4", "#8bc777", "#d1cc63"];
                    ctx.fillStyle = spectrum[Math.floor(i * spectrum.length / pts.length)];
                } else ctx.fillStyle = i < pts.length / 2 ? Qt.darker(root.jewel, 1.12 + i * .065) : Qt.lighter(root.jewel, 1.15 + (i % 3) * .16);
                ctx.fill();
            }
            const face = ctx.createLinearGradient(28, 25, 70, 78);
            face.addColorStop(0, Qt.lighter(root.jewel, 1.55));
            face.addColorStop(.45, Qt.lighter(root.jewel, 1.08));
            face.addColorStop(1, Qt.darker(root.jewel, 1.22));
            path(inner, 0); ctx.fillStyle = face; ctx.fill();
            ctx.lineWidth = 1.1; ctx.strokeStyle = "#70ffffff"; ctx.stroke();
            path([inner[0], inner[1], [47, 45]], 0); ctx.fillStyle = "#65ffffff"; ctx.fill();
        }
    }
}
