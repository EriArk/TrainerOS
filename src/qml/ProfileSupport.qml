import QtQuick

Item {
    id:root
    objectName:"profile-support-page"
    required property var shell
    readonly property var progress:shell.backendAccount.progression
    readonly property var preview:progress.support_preview||({})
    readonly property var connection:shell.backendAccount.support
    readonly property bool known:!!preview.policy
    readonly property bool compact:height<450
    function option(multiplier) {
        const rows=preview.options||[]
        for(let i=0;i<rows.length;++i)if(rows[i].multiplier===multiplier)return rows[i]
        return ({})
    }
    Text { id:title;width:parent.width-current.width-24;text:"A little more spark";font.family:Theme.displayFamily;font.pixelSize:root.compact?28:36;color:Theme.ink }
    Column {
        id:current;anchors.right:parent.right;spacing:4
        Text { text:root.progress.stale?"LAST CONFIRMED BALANCE":"AVAILABLE NOW";font.pixelSize:9;font.letterSpacing:1;color:Theme.muted }
        Row { spacing:7;SparkIcon {width:25;height:25} Text {text:root.shell.profileAccount.sparksText;font.pixelSize:24;color:Theme.ink} }
    }
    Text { id:intro;y:title.height+9;width:parent.width-current.width-24;text:root.progress.support && !root.progress.stale?"Current earning rate: \u00d7"+root.progress.support.multiplier+". Compare the remaining bonus below.":"Support TrainerOS. See what your earned rewards would become.";wrapMode:Text.WordWrap;font.pixelSize:12;color:Theme.muted }
    Row {
        id:tiers;y:Math.max(intro.y+intro.height,current.height)+(root.compact?16:20);width:parent.width;height:root.compact?142:190;spacing:28
        Repeater {
            model:[2,3]
            delegate:Item {
                id:tier
                required property int modelData
                readonly property var quote:root.option(modelData)
                width:(tiers.width-tiers.spacing)/2;height:tiers.height
                Rectangle { width:parent.width;height:2;color:Theme.focus;opacity:modelData===3?.8:.4 }
                Text { y:12;text:tier.modelData===2?"SPARK · $3 / MONTH":"SUPERNOVA · $5 / MONTH";font.pixelSize:10;font.letterSpacing:1;color:Theme.muted }
                Text { anchors.right:parent.right;y:2;text:"\u00d7"+tier.modelData;font.family:Theme.displayFamily;font.pixelSize:42;color:Theme.ink }
                Row { y:root.compact?47:65;spacing:8
                    SparkIcon { width:root.compact?31:38;height:width }
                    Text { objectName:"support-potential-"+tier.modelData;text:root.known?String(tier.quote.potential_balance):"\u2014";font.family:Theme.displayFamily;font.pixelSize:root.compact?31:39;color:Theme.ink }
                }
                Text { y:root.compact?85:110;width:parent.width;text:"Potential balance";font.pixelSize:12;color:Theme.muted }
                Text { objectName:"support-topup-"+tier.modelData;y:root.compact?110:140;width:parent.width;text:root.known?"+"+tier.quote.top_up+" Sparks from past rewards":"Calculation unavailable";font.pixelSize:root.compact?12:14;color:Theme.ink;elide:Text.ElideRight }
                Text { y:root.compact?124:165;width:parent.width;text:"\u00d7"+tier.modelData+" Sparks per eligible reward";font.pixelSize:10;color:Theme.muted }
            }
        }
    }
    Text {
        id:explanation;y:tiers.y+tiers.height+12;width:parent.width
        text:root.known?preview.base_sparks+" base Sparks across "+preview.eligible_rewards+" verified rewards.\nPurchases do not reduce this bonus. Account XP stays the same.":root.progress.busy?"Refreshing your reward history\u2026":root.shell.profileAccount.signedIn?"Connect and refresh to see your calculation.":"Sign in to see a calculation from your verified reward history."
        wrapMode:Text.WordWrap;font.pixelSize:root.compact?11:12;color:Theme.muted
    }
    Item {
        anchors.bottom:parent.bottom;width:parent.width;height:76
        Rectangle { width:parent.width;height:1;color:Theme.divider }
        Text { y:8;width:parent.width;text:root.connection.message||"Connect Patreon to verify your supporter level.";wrapMode:Text.WordWrap;font.pixelSize:10;color:Theme.muted }
        Row {
            y:39;spacing:16
            Repeater {
                model:root.connection.state==="connected"||root.connection.state==="needs_relink"?3:2
                delegate:Item {
                    required property int index
                    width:index===0?160:index===1?115:155;height:32
                    readonly property string label:index===1?"Open Patreon":index===2?(root.connection.review?"Confirm disconnect":"Disconnect")
                        :!root.shell.profileAccount.signedIn?"Sign in":root.connection.pending?"Cancel connection":root.connection.busy?"Checking…"
                        :root.connection.state==="connected"?"Check membership":root.connection.available?"Connect Patreon":"Refresh"
                    Rectangle {anchors.fill:parent;radius:5;color:root.shell.profileContentActive&&root.shell.profileFocus===index?Theme.selection:"transparent";border.width:root.shell.profileContentActive&&root.shell.profileFocus===index?1:0;border.color:Theme.focus}
                    Text {anchors.centerIn:parent;text:parent.label;font.pixelSize:12;color:Theme.ink}
                    MouseArea {anchors.fill:parent;enabled:!root.connection.busy||(index===0&&root.connection.pending);onClicked:root.shell.activate(index,"profile")}
                    Accessible.role:Accessible.Button;Accessible.name:label;Accessible.onPressAction:root.shell.activate(index,"profile")
                }
            }
        }
    }
}
