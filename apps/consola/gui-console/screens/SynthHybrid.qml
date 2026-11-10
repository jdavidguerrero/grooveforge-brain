import QtQuick
import "../theme"
import "../components"
Item {
    property var vals:synth.values
    Panel {x:8;y:8;width:152;height:44;color:Theme.roleColor('PAD');Label{anchors.centerIn:parent;text:'PAD  ·  '+synth.selectedPart+'/4';face:Theme.displayFont;font.pixelSize:28;color:Theme.bg}}
    Repeater {model:['Woodbass','Kalimba','Woodglass','French stab','Acid arp','Desert lead','Roller sub','Retro brass','CUSTOM']
        Panel {required property int index;required property string modelData;x:168+index*123;y:8;width:115;height:44;border.color:index===(synth.values['hybrid.layer']||0)?Theme.hybHi:Theme.line2
            Label {x:6;y:0;width:103;height:18;text:index===8?'CUSTOM':'F'+(index+1);font.pixelSize:10;color:Theme.hybHi}
            Label {x:6;y:18;width:103;height:20;text:modelData;font.pixelSize:11}
        }
    }
    Panel {x:8;y:60;width:1264;height:100;color:Theme.hybDim;border.color:Theme.hyb
        Label {x:20;y:8;text:'MORPH · ENC 4';font.pixelSize:12;color:Theme.hybHi}
        Label {x:20;y:22;height:76;text:String(synth.values['hybrid.morph']||0);face:Theme.displayFont;font.pixelSize:68}
        Rectangle {x:168;y:34;width:640;height:24;color:Theme.surface2
            Repeater {model:5;Rectangle{required property int index;x:index*159;y:0;width:1;height:24;color:Theme.hyb}}
            Rectangle {x:(parent.width-8)*(synth.values['hybrid.morph']||0)/100;y:-14;width:8;height:52;color:Theme.white}
        }
        Label {x:168;y:72;text:'A       0           25           50           75           100       B';font.pixelSize:11;color:Theme.hybHi}
        Label {x:848;y:14;width:392;text:'MORPHS TIMBRE A → B';face:Theme.displayFont;font.pixelSize:28;color:Theme.hybHi}
        Label {x:848;y:52;width:392;height:40;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:'Shared parameters move between halves. Both halves keep sounding.';font.pixelSize:12;color:Theme.grayN}
    }
    Repeater {model:['A','B']
        Panel {required property int index;required property string modelData;x:index===0?8:808;y:168;width:472;height:200;border.width:2;border.color:index===0?Theme.hybHi:Theme.hyb
            Label {x:16;y:8;text:modelData;face:Theme.displayFont;font.pixelSize:48;color:Theme.hybHi}
            Label {x:72;y:16;width:380;text:parent.index===0?'SEM POLY':'VINTAGE POLY';face:Theme.displayFont;font.pixelSize:30}
            Label {x:72;y:58;width:380;text:parent.index===0?'OB Pad Warm':'Prophet Air';font.pixelSize:16;color:Theme.grayN}
            Label {x:16;y:108;width:440;text:parent.index===0?'Body · weight · slow attack':'Air · shimmer · poly-mod';font.pixelSize:13;color:Theme.hybHi}
            ActionButton {x:16;y:148;width:440;height:36;text:'EDIT HALF '+modelData+'  ▸';action:'synth:nav:SynthHybrid'+modelData}
        }
    }
    Panel {x:488;y:168;width:312;height:200
        Label {x:16;y:12;text:'LAYER 03 · FACTORY';font.pixelSize:11;color:Theme.hybHi}
        Label {x:16;y:42;width:280;text:synth.partInfo.preset || 'HYBRID';face:Theme.displayFont;font.pixelSize:40}
        Label {x:16;y:106;width:280;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:'Two synth halves\nLayer controls below\n'+(synth.demo?'DESIGN PREVIEW':'HOST PARAMETERS');lineHeight:1.6;font.pixelSize:12;color:Theme.grayN}
    }
    ParameterPage {x:0;y:376;width:1280;height:184;hostDriven:true;tint:Theme.hybHi}
}
