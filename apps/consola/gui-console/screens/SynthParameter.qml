import QtQuick
import "../theme"
import "../components"
Item {
    id: root
    property color accent: synth.engine === 'hybrid' ? Theme.hybHi : Theme.tealPlus
    Panel {x:8;y:8;width:152;height:44;color:Theme.roleColor(synth.partInfo.role || 'BASS');border.width:0
        Label {x:12;y:2;height:40;text:synth.partInfo.role || 'PART';face:Theme.displayFont;font.pixelSize:30;color:Theme.bg}
        Label {anchors.right:parent.right;anchors.rightMargin:8;y:12;text:synth.pageName.indexOf('A ')===0?'HALF A':synth.pageName.indexOf('B ')===0?'HALF B':synth.selectedPart+'/4';font.pixelSize:10;color:Theme.bg}
    }
    Flickable {x:168;y:8;width:1112;height:44;contentWidth:tabs.width;clip:true
        Row {id:tabs;spacing:8
            Repeater {model:synth.pages.filter(n=>!/^OP [1-6]$/.test(n))
                ActionButton {required property string modelData;width:152;height:44;text:modelData;active:synth.pageName===modelData;tint:active?root.accent:Theme.grayN;onClicked:synth.selectPage(modelData)}
            }
        }
    }
    Label {x:8;y:60;width:1248;height:20;text:synth.engineLabel+'    ·    '+(synth.partInfo.preset || 'Init')+(synth.partInfo.modified?'    ·    MODIFIED':'')+'    /    '+synth.pageName+'    /    '+(synth.demo?'DESIGN PREVIEW':'HOST');font.pixelSize:12;color:root.accent}
    SynthGraph {x:8;y:88;width:1264;height:296;visible:synth.status==='ready'||synth.status==='preview'}
    Panel {x:8;y:88;width:1264;height:296;visible:synth.status!=='ready'&&synth.status!=='preview';border.color:synth.status==='not_responding'?Theme.amber:Theme.line2
        Label {x:32;y:46;text:synth.status==='loading'?'LOADING PRESET':synth.status==='not_responding'?'BRAIN NOT RESPONDING':'CONNECTING TO BRAIN';face:Theme.displayFont;font.pixelSize:44;color:synth.status==='not_responding'?Theme.amber:Theme.white}
        Label {x:32;y:122;width:1180;text:synth.status==='not_responding'?'Last values are shown. Check the synth host connection.':'Waiting for the host response.';font.pixelSize:14;color:Theme.grayN}
        ActionButton {x:32;y:184;width:200;height:48;visible:synth.status==='not_responding';text:'RETRY';tint:Theme.amber;onClicked:synth.retry()}
        Rectangle {x:32;y:196;width:320;height:4;visible:synth.status==='loading';color:Theme.line2;Rectangle{width:160;height:4;color:root.accent}}
    }
    ParameterPage {x:0;y:392;width:1280;height:168;hostDriven:true;tint:root.accent;opacity:synth.status==='not_responding'?.45:1}
    Label {x:8;y:562;width:1264;height:6;text:synth.error;font.pixelSize:10;color:Theme.amber;visible:synth.error.length>0}
}
