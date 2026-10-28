import QtQuick

Item {
    id:root
    objectName:"profile-support-page"
    required property var shell
    readonly property var progress:shell.backendAccount.progression
    readonly property var preview:progress.support_preview||({})
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
    Text { id:intro;y:title.height+9;width:parent.width-current.width-24;text:"Support TrainerOS. See what your earned rewards would become.";wrapMode:Text.WordWrap;font.pixelSize:12;color:Theme.muted }
    Row {
        id:tiers;y:Math.max(intro.y+intro.height,current.height)+20;width:parent.width;height:root.compact?153:190;spacing:28
        Repeater {
            model:[2,3]
            delegate:Item {
                id:tier
                required property int modelData
                readonly property var quote:root.option(modelData)
                width:(tiers.width-tiers.spacing)/2;height:tiers.height
                Rectangle { width:parent.width;height:2;color:Theme.focus;opacity:modelData===3?.8:.4 }
                Text { y:12;text:"SUPPORT";font.pixelSize:10;font.letterSpacing:2;color:Theme.muted }
                Text { anchors.right:parent.right;y:2;text:"\u00d7"+tier.modelData;font.family:Theme.displayFamily;font.pixelSize:42;color:Theme.ink }
                Row { y:root.compact?47:65;spacing:8
                    SparkIcon { width:root.compact?31:38;height:width }
                    Text { objectName:"support-potential-"+tier.modelData;text:root.known?String(tier.quote.potential_balance):"\u2014";font.family:Theme.displayFamily;font.pixelSize:root.compact?31:39;color:Theme.ink }
                }
                Text { y:root.compact?85:110;width:parent.width;text:"Potential balance";font.pixelSize:12;color:Theme.muted }
                Text { objectName:"support-topup-"+tier.modelData;y:root.compact?110:140;width:parent.width;text:root.known?"+"+tier.quote.top_up+" Sparks from past rewards":"Calculation unavailable";font.pixelSize:root.compact?12:14;color:Theme.ink;elide:Text.ElideRight }
                Text { y:root.compact?131:165;width:parent.width;text:"\u00d7"+tier.modelData+" Sparks per eligible reward";font.pixelSize:10;color:Theme.muted }
            }
        }
    }
    Text {
        id:explanation;y:tiers.y+tiers.height+12;width:parent.width
        text:root.known?preview.base_sparks+" base Sparks across "+preview.eligible_rewards+" verified rewards.\nPurchases do not reduce this bonus. Account XP stays the same.":root.progress.busy?"Refreshing your reward history\u2026":root.shell.profileAccount.signedIn?"Connect and refresh to see your calculation.":"Sign in to see a calculation from your verified reward history."
        wrapMode:Text.WordWrap;font.pixelSize:root.compact?11:12;color:Theme.muted
    }
    Item {
        anchors.bottom:parent.bottom;width:parent.width;height:root.compact?55:70
        Rectangle { width:parent.width;height:1;color:Theme.divider }
        Text { y:12;width:parent.width-refresh.width-20;text:"Preview only. Support activation is not available yet.\nThese extra Sparks have not been credited.";wrapMode:Text.WordWrap;font.pixelSize:10;color:Theme.muted }
        Item {
            id:refresh;anchors.right:parent.right;y:9;width:140;height:34
            Rectangle { anchors.fill:parent;radius:5;color:Theme.selection;border.width:root.shell.profileContentActive?1:0;border.color:Theme.focus }
            Text { anchors.centerIn:parent;text:root.progress.busy?"Refreshing\u2026":root.shell.profileAccount.signedIn?"Refresh":"Sign in";font.pixelSize:12;color:Theme.ink }
            MouseArea { anchors.fill:parent;enabled:!root.progress.busy;onClicked:root.shell.activate(0,"profile") }
            Accessible.role:Accessible.Button;Accessible.name:root.shell.profileAccount.signedIn?"Refresh":"Sign in";Accessible.onPressAction:root.shell.activate(0,"profile")
        }
    }
}
