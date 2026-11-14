import QtQuick
import "../theme"
import "../components"
Item {
    id:root
    property bool save:console.screenName==='SynthSaveAs'
    Panel {x:8;y:8;width:152;height:44;color:Theme.roleDim(synth.partInfo.role || 'BASS');Label{anchors.centerIn:parent;text:synth.partInfo.role || 'PART';face:Theme.displayFont;font.pixelSize:28}}
    Label {x:184;y:8;height:44;text:'ROLE LOCKED  /  '+synth.engineFilter+'  /  '+synth.tagFilter;font.pixelSize:13;color:Theme.tealPlus}
    Label {x:8;y:66;text:'    NAME                                 ENGINE             TAGS';font.pixelSize:11;color:Theme.mon}
    ListView {x:8;y:88;width:792;height:464;clip:true;spacing:4;model:synth.presets;currentIndex:synth.presetIndex
        highlightMoveDuration:0
        onCurrentIndexChanged:positionViewAtIndex(currentIndex,ListView.Contain)
        delegate:Panel {required property int index;required property var entry;width:792;height:48;color:entry.selected?Theme.tealDim:Theme.surface;border.color:entry.selected?Theme.tealPlus:Theme.line
            Label {x:12;y:0;height:48;text:entry.favorite?'★':'·';font.pixelSize:16;color:Theme.tealPlus}
            Label {x:40;y:0;width:340;height:48;text:entry.name;font.pixelSize:14}
            Label {x:388;y:0;width:144;height:48;text:entry.engine.toUpperCase();font.pixelSize:12;color:Theme.grayN}
            Label {x:540;y:0;width:238;height:48;text:entry.tags.join(' · ')+(entry.loaded?'  IN PART':'');font.pixelSize:11;color:entry.loaded?Theme.tealPlus:Theme.grayN}
            TouchTarget { hardwareAction: "list";anchors.fill:parent;callback: function() { synth.selectPreset(parent.index) };onDoubleClicked:synth.loadPreset()}
        }
    }
    Panel {x:808;y:60;width:472;height:230;border.color:Theme.roleColor(synth.partInfo.role || 'BASS')
        Label {x:20;y:16;text:'IN THE PART · ROOM';font.pixelSize:11;color:Theme.tealPlus}
        Label {x:20;y:48;width:432;text:synth.partInfo.preset || '—';face:Theme.displayFont;font.pixelSize:38}
        Label {x:20;y:104;width:432;text:synth.engineLabel+(synth.partInfo.modified?'  ·  MODIFIED':'');font.pixelSize:12;color:Theme.grayN}
        ActionButton {x:20;y:160;width:208;height:44;text:'SAVE AS';action:'synth:nav:SynthSaveAs'}
        ActionButton {x:240;y:160;width:208;height:44;text:'LOAD SELECTED';callback:function(){synth.loadPreset()}}
    }
    Panel {x:808;y:298;width:472;height:254
        Label {x:20;y:20;text:'AUDITION · CUE';font.pixelSize:12;color:Theme.white}
        Label {x:20;y:62;width:432;height:100;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:'Audition is unavailable.\nLoading changes the selected part.\n\nSelect a preset, then press encoder 1.';font.pixelSize:14;lineHeight:1.6;color:Theme.grayN}
        Label {x:20;y:192;width:432;height:42;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:synth.status==='loading'?'LOADING PRESET':synth.error || 'Loading replaces unsaved changes.';font.pixelSize:12;color:Theme.amber}
    }
    Rectangle {anchors.fill:parent;color:Qt.rgba(0,0,0,.72);visible:root.save;TouchTarget {hardwareAction:"navigation";callback:function(){synth.openScreen("SynthPresets")} }}
    Panel {objectName:'synthSaveWindow';x:160;y:40;width:960;height:488;visible:root.save;border.color:Theme.line2
        Label {x:24;y:12;text:'SAVE AS · '+(synth.partInfo.role || 'PART');face:Theme.displayFont;font.pixelSize:32}
        Label {x:24;y:54;width:912;text:'USER · '+synth.engineLabel+' · Original preset stays untouched';font.pixelSize:12;color:Theme.grayN}
        Panel {x:24;y:84;width:912;height:56;border.color:Theme.tealPlus
            Label {x:16;y:0;width:780;height:56;text:synth.saveName;font.pixelSize:22}
            Label {anchors.right:parent.right;anchors.rightMargin:16;height:56;text:synth.saveName.length+' / 24';font.pixelSize:12;color:Theme.grayN}
        }
        Label {x:24;y:152;width:912;text:synth.error || 'NEW USER PRESET';font.pixelSize:12;color:synth.error?Theme.amber:Theme.mon}
        SynthKeyboard {x:40;y:200;width:880;height:216}
        Label {x:24;y:444;width:912;text:'Touch to type · encoder 6 saves · encoder 8 cancels';font.pixelSize:12;color:Theme.grayN}
    }
}
