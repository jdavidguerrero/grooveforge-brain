import QtQuick
import "../theme"
import "../components"
Item {
    Panel {x:8;y:0;width:472;height:120;border.color:drum.tint
        Label {x:16;y:10;text:'ORIGINAL · PAD '+('0'+(drum.selectedPad+1)).slice(-2);font.pixelSize:10;color:drum.tint}
        Label {x:16;y:35;width:248;text:drum.padInfo.name;face:Theme.displayFont;font.pixelSize:34}
        DrumWaveform {x:288;y:24;width:164;height:72;bars:drum.waveform;tint:drum.tint}
        Label {x:16;y:91;width:440;text:'ENGINE · '+drum.rack.root+' '+drum.rack.mode+' · ORIGINAL KEPT';font.pixelSize:10;color:Theme.grayN}
    }
    Panel {x:488;y:0;width:784;height:120;border.color:drum.enginePad?Theme.purpleBright:Theme.amber
        Label {x:16;y:10;width:752;text:!drum.enginePad?'ENGINE PADS ONLY':drum.error?drum.error:'RENDER VARIATIONS · '+String(drum.job.status||'READY').toUpperCase();font.pixelSize:12;color:drum.error?Theme.amber:Theme.purpleBright}
        Row {x:16;y:41;spacing:6
            Repeater {model:8;Rectangle {required property int index;width:88;height:8;color:index<(drum.job.done||0)?Theme.purpleBright:Theme.purpleDim}}
        }
        Label {x:16;y:66;width:752;text:'Keep material, role and key · spread '+Math.round(drum.spread*100)+'% · '+drum.count+' candidates';font.pixelSize:13;color:Theme.grayN}
        Label {x:16;y:94;width:752;text:'ENC 1–8 CUE · SHIFT + ENC FOR SOFT KEYS · KEEP → PAD IS YOUR CHOICE';font.pixelSize:10;color:Theme.white}
    }
    Repeater {model:drum.variants
        Panel {id:variant;required property var modelData;required property int index;x:Theme.columnX(index);y:128;width:152;height:404
            border.width:modelData.selected?2:1;border.color:modelData.state==='kept'?Theme.tealPlus:modelData.state==='cue'?Theme.white:Theme.purpleBright
            opacity:modelData.state==='discarded'?.35:1
            Label {x:12;y:12;text:String.fromCharCode(65+variant.index);face:Theme.displayFont;font.pixelSize:34;color:variant.modelData.state==='kept'?Theme.tealPlus:Theme.purpleBright}
            Label {x:12;y:58;width:128;text:variant.modelData.state.toUpperCase();font.pixelSize:10;color:Theme.grayN}
            DrumWaveform {x:12;y:90;width:128;height:96;bars:variant.modelData.layer.waveform||[];tint:variant.modelData.state==='kept'?Theme.tealPlus:Theme.purpleBright}
            Label {x:12;y:202;width:128;height:46;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:variant.modelData.name;font.pixelSize:13}
            Repeater {model:variant.modelData.traits
                Item {required property var modelData;required property int index;x:12;y:258+index*26;width:128;height:24
                    Label {text:parent.modelData.name;font.pixelSize:9;color:Theme.grayN}
                    Label {x:78;width:50;horizontalAlignment:Text.AlignRight;text:parent.modelData.text;font.pixelSize:9;color:Theme.grayN}
                    Rectangle {y:16;width:128;height:4;color:Theme.purpleDim
                        Rectangle {width:parent.width*modelData.amount;height:4;color:Theme.purpleBright}
                    }
                }
            }
            Label {x:12;y:339;width:128;text:'KEY '+drum.rack.root+' · NEW SEED';font.pixelSize:9;color:Theme.grayN}
            Panel {x:10;y:360;width:132;height:32;border.color:Theme.purpleBright
                Label {anchors.centerIn:parent;text:variant.modelData.state==='kept'?'KEPT':'CUE';font.pixelSize:12;color:Theme.purpleBright}
            }
            DashedOutline {anchors.fill:parent;visible:variant.modelData.state==='cue'||variant.modelData.state==='discarded'||variant.modelData.state==='empty';stroke:variant.modelData.state==='cue'?Theme.white:Theme.line2}
            TouchTarget {hardwareAction:'choice';enabled:variant.modelData.state!=='empty'&&variant.modelData.state!=='discarded';callback:function(){drum.auditionVariant(variant.index)}}
        }
    }
    Label {x:12;y:546;width:1256;text:drum.demo?'DESIGN PREVIEW · NO AUDIO':'Worker renders offline · your steps, groups and edits stay in place';font.pixelSize:10;color:Theme.grayN}
}
