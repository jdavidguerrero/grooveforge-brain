import QtQuick
import "../theme"
import "../components"
Item {
    id:root
    property bool states: console.screenName==='SynthPartsStates'
    Panel {x:8;y:8;width:1264;height:44
        Label {x:16;y:0;height:44;text:console.synthMode?'SYNTH · FOUR PARTS':'SET   Selva Nocturna';font.pixelSize:14}
        Label {anchors.right:parent.right;anchors.rightMargin:16;height:44;text:synth.demo?'DESIGN PREVIEW · 4 SOFTWARE PARTS + OPTIONAL HARDWARE':'4 SOFTWARE PARTS · HOST '+synth.status.toUpperCase();font.pixelSize:11;color:Theme.grayN}
    }
    Repeater {model:synth.parts
        Panel {id:card;required property int index;required property var entry;x:8+index*160;y:60;width:152;height:392;border.width:2;border.color:entry.visual==='offline'?Theme.amber:entry.selected?Theme.roleColor(entry.role):Theme.line
            Rectangle {x:2;y:2;width:148;height:40;color:Theme.roleColor(card.entry.role)
                Label {x:10;y:0;height:40;text:card.entry.role;face:Theme.displayFont;font.pixelSize:28;color:Theme.bg}
                Label {anchors.right:parent.right;anchors.rightMargin:8;height:40;text:(card.index+1)+'/4';font.pixelSize:10;color:Theme.bg}
            }
            Label {x:12;y:58;text:'ENGINE';font.pixelSize:10;color:Theme.mon}
            Label {x:12;y:78;width:128;height:56;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:card.entry.label;face:Theme.displayFont;font.pixelSize:26}
            Label {x:12;y:144;text:'PRESET';font.pixelSize:10;color:Theme.mon}
            Label {x:12;y:166;width:128;height:52;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:card.entry.visual==='empty'?'ADD A SOUND':card.entry.preset;font.pixelSize:14;color:card.entry.visual==='offline'?Theme.amber:Theme.white}
            Label {x:12;y:228;width:128;text:card.entry.visual==='loading'?'LOADING':card.entry.visual==='offline'?'NO RESPONSE':card.entry.modified?'● MODIFIED':'CHANNEL '+(card.index+1);font.pixelSize:10;color:card.entry.visual==='idle'?Theme.mon:Theme.amber}
            Rectangle {x:14;y:266;width:8;height:84;color:Theme.line2;Rectangle{anchors.bottom:parent.bottom;width:8;height:84*Math.max(0,Math.min(1,(card.entry.peak_db+60)/60));color:Theme.roleColor(card.entry.role)}}
            Rectangle {x:26;y:266;width:8;height:84;color:Theme.line2;Rectangle{anchors.bottom:parent.bottom;width:8;height:84*Math.max(0,Math.min(1,(card.entry.rms_db+60)/60));color:Theme.roleColor(card.entry.role)}}
            Label {x:44;y:278;width:94;text:card.entry.mute?'MUTED':card.entry.level_db.toFixed(1)+' dB';font.pixelSize:12;color:Theme.grayN}
            Label {x:44;y:312;width:94;text:card.entry.visual==='empty'?'EMPTY':'ROOM';font.pixelSize:12;color:Theme.tealPlus}
            Label {x:12;y:364;width:128;text:card.entry.selected?'PRESS TO OPEN':'TURN TO SELECT';font.pixelSize:10;color:Theme.roleColor(card.entry.role)}
            DashedOutline {anchors.fill:parent;visible:card.entry.visual==='offline'||card.entry.visual==='empty';stroke:card.entry.visual==='offline'?Theme.amber:Theme.line2}
            Rectangle {x:0;y:388;width:parent.width*.55;height:4;visible:card.entry.visual==='loading';color:Theme.roleColor(card.entry.role)}
            TouchTarget { hardwareAction: "choice";anchors.fill:parent;callback: function() { console.triggerAction('synth:open:'+(card.index+1)) };onDoubleClicked:console.triggerAction('synth:open:'+(card.index+1))}
        }
    }
    Panel {x:648;y:60;width:152;height:392;border.color:Theme.amber;border.width:2
        Rectangle {x:2;y:2;width:148;height:40;color:Theme.surface2;Label{anchors.centerIn:parent;text:'BRAIN';face:Theme.displayFont;font.pixelSize:28}}
        Label {x:12;y:62;text:'HARDWARE';font.pixelSize:10;color:Theme.grayN}
        Label {x:12;y:92;width:128;text:'MODULAR';face:Theme.displayFont;font.pixelSize:28}
        Label {x:12;y:150;width:128;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:'USB STATUS\nUNAVAILABLE';font.pixelSize:13;color:Theme.amber}
        Label {x:12;y:244;width:128;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:'Optional synth\nTeensy +\nanalog filter';font.pixelSize:12;lineHeight:1.6;color:Theme.grayN}
        ActionButton {x:8;y:340;width:136;height:40;text:'CONNECTIONS';action:'nav:SynthSettingsConnections'}
        DashedOutline {anchors.fill:parent;stroke:Theme.amber}
    }
    Panel {x:808;y:60;width:472;height:500;border.width:2;border.color:root.states?Theme.amber:Theme.roleColor(synth.partInfo.role || 'BASS')
        Label {x:16;y:12;text:'SELECTED PART · '+(synth.partInfo.role || '—');font.pixelSize:11;color:Theme.grayN}
        Label {x:16;y:36;width:440;text:root.states?'BRAIN HARDWARE NOT CONNECTED':synth.partInfo.preset || 'Waiting for host';face:Theme.displayFont;font.pixelSize:32;color:root.states?Theme.amber:Theme.white}
        Label {x:16;y:82;text:root.states?'Software parts are independent of USB.':'ENGINE · TURN ENC 6 TO CHANGE';font.pixelSize:11;color:Theme.mon}
        Repeater {model:root.states?[]:synth.engineOptions
            ActionButton {required property int index;required property string modelData;x:16;y:108+index*38;width:440;height:34;text:modelData==='dx7'?'FM 4-OP':modelData==='dx7_6op'?'DX7 6-OP':modelData.toUpperCase();active:synth.engine===modelData;callback:function(){synth.changeEngine(modelData)}}
        }
        Label {x:20;y:140;width:424;visible:root.states;wrapMode:Text.WordWrap;elide:Text.ElideNone;lineHeight:1.8;text:'The host reports software parts only.\nUSB detection and CUE routing require\nthe console connection service.\n\nCheck USB cable and Teensy power.\n\nLAST SEEN   —';font.pixelSize:14;color:Theme.grayN}
        ActionButton {x:16;y:436;width:200;height:44;text:root.states?'RETRY':'OPEN PART';callback:function(){root.states?synth.retry():console.triggerAction('synth:open:'+synth.selectedPart)}}
        Label {x:236;y:436;width:210;height:44;text:synth.partInfo.level_db!==undefined?synth.partInfo.level_db.toFixed(1)+' dB  ·  ROOM':'—';font.pixelSize:16;color:Theme.grayN}
    }
    Panel {x:8;y:460;width:472;height:100;border.color:Theme.teal
        Label {x:16;y:10;text:'ROOM    AUDIO+ · MAIN OUT';font.pixelSize:13;color:Theme.tealPlus}
        Label {x:16;y:44;width:440;text:'BASS   CHORDS   LEAD   PAD';font.pixelSize:14}
        Label {x:16;y:76;text:synth.status==='not_responding'?'Host not responding':'Four stereo software outputs';font.pixelSize:10;color:Theme.grayN}
    }
    Panel {x:488;y:460;width:312;height:100;border.color:Theme.amber
        Label {x:16;y:10;text:'CUE    TEENSY · HEADPHONES';font.pixelSize:12;color:Theme.amber}
        Label {x:16;y:44;width:280;text:'ROUTING UNAVAILABLE';font.pixelSize:14}
        Label {x:16;y:76;text:'USB service required';font.pixelSize:10;color:Theme.grayN}
    }
}
