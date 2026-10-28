import QtQuick

Rectangle {
    id:root
    required property var profile
    property string appearance:""
    property bool showAvatar:true
    property bool thumbnail:false
    radius:12;clip:true
    gradient:Gradient { GradientStop {position:0;color:Theme.dark?"#182536":"#bdd9dc"} GradientStop {position:1;color:Theme.dark?"#293f40":"#dce8ce"} }
    Rectangle { visible:!root.appearance;x:parent.width*.60;y:parent.height*.10;width:parent.width*.32;height:width;radius:width/2;color:Theme.dark?"#b8c6ab":"#fff1c9";opacity:.88 }
    Canvas {
        anchors.fill:parent;visible:!root.appearance
        property bool night:Theme.dark
        onNightChanged:requestPaint()
        onWidthChanged:requestPaint()
        onHeightChanged:requestPaint()
        onPaint:{let c=getContext("2d");c.reset();const w=width,h=height;c.fillStyle=night?"#293b47":"#96bab3";c.beginPath();c.moveTo(0,h*.47);c.bezierCurveTo(w*.38,h*.12,w*.52,h*.77,w,h*.34);c.lineTo(w,h);c.lineTo(0,h);c.fill();c.fillStyle=night?"#36504b":"#b9cdb1";c.beginPath();c.moveTo(0,h*.73);c.bezierCurveTo(w*.4,h*.4,w*.6,h*.6,w,h*.68);c.lineTo(w,h);c.lineTo(0,h);c.fill()}
    }
    Image {
        anchors.fill:parent;visible:!!root.appearance
        source:root.appearance?"qrc:/profile-scenes/"+root.appearance+".png":""
        sourceSize:root.thumbnail?Qt.size(120,180):Qt.size(768,1152)
        fillMode:Image.PreserveAspectCrop;asynchronous:true
    }
    Loader {
        id:avatar;x:0;y:root.appearance?parent.height*.03:22;width:parent.width;height:root.appearance?parent.height*.90:parent.height-37
        active:root.showAvatar&&!root.thumbnail&&!!root.profile.avatarPreview&&root.profile.avatarRendererAvailable
        source:active?"AvatarPreview3D.qml":""
        onLoaded:item.modelSource=Qt.binding(function(){return root.profile.avatarPreview})
    }
    TrainerEmblem { anchors.centerIn:parent;width:Math.min(110,parent.width*.55);height:width;visible:root.showAvatar&&!avatar.active&&!root.thumbnail;emblem:root.profile.profile.emblem||"compass" }
}
