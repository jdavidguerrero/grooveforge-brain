import QtQuick
import "../theme"
import "../components"
Item {
    Row {x:8;y:0;spacing:8
        Repeater {model:['RACK','PATTERN']
            Panel {required property string modelData;width:152;height:44;border.color:drum.saveKind===modelData?Theme.tealPlus:Theme.line2
                Label {anchors.centerIn:parent;text:parent.modelData;font.pixelSize:13}
                TouchTarget {hardwareAction:'choice';callback:function(){drum.action(parent.modelData==='RACK'?'save-rack':'save-pattern')}}
            }
        }
    }
    Panel {x:328;y:0;width:944;height:44;border.color:Theme.amber
        Label {x:12;anchors.verticalCenter:parent.verticalCenter;width:920;text:'SAVE replaces an existing name · SAVE AS NEW keeps both';font.pixelSize:12;color:Theme.amber}
    }
    Panel {x:8;y:52;width:792;height:220
        Label {x:16;y:12;text:drum.saveKind+' NAME';font.pixelSize:11;color:Theme.grayN}
        Panel {x:16;y:40;width:760;height:56;border.width:drum.textTarget==='save'?2:1;border.color:Theme.tealPlus
            Label {x:12;anchors.verticalCenter:parent.verticalCenter;width:680;text:drum.saveName;font.pixelSize:22}
            Label {x:690;anchors.verticalCenter:parent.verticalCenter;text:drum.saveName.length+'/96';font.pixelSize:10;color:Theme.grayN}
            TouchTarget {hardwareAction:'text';callback:function(){drum.editText('save')}}
        }
        Label {x:16;y:116;text:'CONTENTS';font.pixelSize:10;color:Theme.grayN}
        Label {x:16;y:140;width:740;wrapMode:Text.WordWrap;height:60;elide:Text.ElideNone;text:drum.saveKind==='RACK'?'16 pads · 8 patterns · pad settings · groups · source licence notes\nKey '+drum.rack.root+' '+drum.rack.mode:'Pattern '+String.fromCharCode(65+drum.clock.pattern)+' · 16 lanes · lengths, velocity, probability, microtiming';font.pixelSize:13;color:Theme.grayN}
    }
    Panel {x:808;y:52;width:464;height:220
        Label {x:16;y:12;text:'SOURCES INCLUDED';font.pixelSize:11;color:Theme.grayN}
        Flickable {x:16;y:40;width:432;height:164;contentHeight:sources.height;clip:true
            Column {id:sources;spacing:12
                Repeater {model:drum.sources
                    Column {required property var modelData;spacing:4
                        Label {width:422;text:parent.modelData.kind+' · '+parent.modelData.name;font.pixelSize:12}
                        Label {width:422;text:parent.modelData.licence_note;font.pixelSize:11;color:parent.modelData.licence_note.toLowerCase().indexOf('pending')>=0?Theme.amber:Theme.grayN}
                    }
                }
            }
        }
    }
    DrumKeyboard {x:8;y:290;width:1264;height:254}
    Label {x:12;y:548;width:1256;text:drum.error||'LOCAL SAVE · explicit replace only · samples referenced by hash';font.pixelSize:10;color:drum.error?Theme.amber:Theme.grayN}
    Component.onCompleted: drum.editText('save')
}
