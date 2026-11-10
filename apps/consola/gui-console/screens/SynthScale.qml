import QtQuick
import "../theme"
import "../components"
Item {
    Label { x:24;y:12;text:'SCALE KEYBOARD · '+surface.roots[surface.root]+' '+surface.scale+' · OCT '+surface.octave;face:Theme.displayFont;font.pixelSize:32 }
    Row { x:24;y:64;spacing:8
        Repeater { model:surface.roots
            ActionButton {hardwareAction:"choice"; required property int index;required property string modelData;width:94;height:48;text:modelData;active:surface.root===index;callback:function(){surface.setRoot(index)}}
        }
    }
    Row { x:24;y:128;spacing:12
        Repeater { model:surface.scales
            ActionButton {hardwareAction:"choice"; required property string modelData;width:206;height:48;text:modelData;active:surface.scale===modelData;callback:function(){surface.setScale(modelData)}}
        }
    }
    Repeater { model:surface.layout
        Panel { required property int index;required property var modelData;x:24+index%8*154;y:208+Math.floor(index/8)*66;width:146;height:58;color:modelData.root?Theme.tealDim:Theme.surface;border.color:modelData.root?Theme.white:Theme.line
            Label { anchors.centerIn:parent;text:modelData.note===null?'—':surface.roots[modelData.note%12]+(Math.floor(modelData.note/12)-1);font.pixelSize:18;color:modelData.root?Theme.white:Theme.grayN }
        }
    }
    Label { x:24;y:492;text:'PHYSICAL PADS PLAY · TOUCH CHANGES SCALE ONLY · OCT −/+ ON SURFACE';font.pixelSize:12;color:Theme.grayN }
    Label { x:24;y:528;width:1232;text:surface.status;font.pixelSize:12;color:Theme.tealPlus }
}
