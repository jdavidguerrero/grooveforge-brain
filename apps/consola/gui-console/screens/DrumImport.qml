import QtQuick
import "../theme"
import "../components"
Item {
    Panel {x:8;y:0;width:792;height:72
        Label {x:16;y:10;text:'LOCAL FOLDER';font.pixelSize:10;color:Theme.grayN}
        Label {x:16;y:32;width:756;text:drum.folder||'Choose a folder on this machine';font.pixelSize:16;color:drum.folder?Theme.white:Theme.tealPlus}
        TouchTarget {hardwareAction:'text';callback:function(){drum.editText('folder')}}
    }
    Panel {x:8;y:80;width:792;height:472
        Label {x:20;y:18;text:drum.job.status==='preview'?'DESIGN PREVIEW':drum.job.status==='running' || drum.job.status==='progress'?'IMPORTING IN BACKGROUND':drum.job.status==='done'?'IMPORT COMPLETE':'READY TO IMPORT';font.pixelSize:12;color:Theme.tealPlus}
        Label {x:20;y:54;text:String(drum.job.done||0);face:Theme.displayFont;font.pixelSize:88}
        Label {x:184;y:104;text:'/ '+(drum.job.total||0)+' FILES';font.pixelSize:18;color:Theme.grayN}
        Rectangle {x:20;y:162;width:752;height:10;radius:3;color:Theme.line2
            Rectangle {width:parent.width*(drum.job.total?(drum.job.done||0)/drum.job.total:0);height:10;radius:3;color:Theme.tealPlus}
        }
        Label {x:20;y:186;width:752;text:drum.job.current||'WAV · AIFF · FLAC · OGG · local decode to 48k float';font.pixelSize:12;color:Theme.grayN}
        Row {x:20;y:228;spacing:8
            Repeater {model:[['IMPORTED',drum.job.imported||0],['SKIPPED',(drum.job.skipped||[]).length],['RATE','48k'],['CHANNELS','1 / 2']]
                Panel {required property var modelData;width:182;height:80
                    Label {x:10;y:8;text:parent.modelData[0];font.pixelSize:10;color:Theme.grayN}
                    Label {x:10;y:28;text:String(parent.modelData[1]);face:Theme.displayFont;font.pixelSize:40}
                }
            }
        }
        Label {x:20;y:326;text:'SKIPPED · REASON';font.pixelSize:10;color:Theme.amber}
        Flickable {x:20;y:352;width:752;height:100;contentHeight:skippedRows.height;clip:true
            Column {id:skippedRows;spacing:4
                Repeater {model:drum.job.skipped||[]
                    Label {required property var modelData;width:744;text:modelData.file+' · '+modelData.reason;font.pixelSize:11;color:Theme.grayN}
                }
            }
        }
    }
    Panel {x:808;y:0;width:464;height:196
        Label {x:16;y:14;text:'SAMPLES RAM · 256 MiB BUDGET';font.pixelSize:12;color:Theme.grayN}
        Label {x:16;y:48;text:drum.ramText;face:Theme.displayFont;font.pixelSize:40}
        Rectangle {x:16;y:106;width:432;height:12;radius:3;color:Theme.line2
            Rectangle {height:12;width:parent.width*drum.ramRatio;color:Theme.tealPlus}
            Rectangle {x:parent.width*.8;width:2;height:12;color:Theme.amber}
        }
        Label {x:16;y:138;width:432;height:42;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:'Imports are stored on disk. RAM is checked when a rack loads; the current rack keeps playing.';font.pixelSize:12;color:Theme.grayN}
    }
    Panel {x:808;y:204;width:464;height:240;border.color:drum.licence.toLowerCase().indexOf('pending')>=0?Theme.amber:Theme.line2
        Label {x:16;y:14;text:'SOURCE LICENCE NOTE';font.pixelSize:12;color:Theme.amber}
        Panel {x:16;y:48;width:432;height:48;border.color:Theme.amber
            Label {x:12;anchors.verticalCenter:parent.verticalCenter;width:408;text:drum.licence||'Add a note';font.pixelSize:14}
            TouchTarget {hardwareAction:'text';callback:function(){drum.editText('licence')}}
        }
        Label {x:16;y:116;width:432;height:100;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:'Keep the source and its licence note with every rack.\n\nPending stays visible. No samples leave this machine.';font.pixelSize:13;color:Theme.grayN}
    }
    Panel {x:808;y:452;width:464;height:100;border.color:Theme.tealPlus
        Label {x:16;y:12;text:'IMPORT FOLDER';face:Theme.displayFont;font.pixelSize:28;color:Theme.tealPlus}
        Label {x:16;y:54;text:'Trim silence · Keep original files';font.pixelSize:12;color:Theme.grayN}
        TouchTarget {hardwareAction:'choice';callback:function(){drum.importFolder()}}
    }
    DrumKeyboard {x:8;y:278;width:1264;height:274;z:20;visible:drum.textTarget==='folder'||drum.textTarget==='licence'}
}
