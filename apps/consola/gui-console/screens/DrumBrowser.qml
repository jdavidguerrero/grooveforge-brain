import QtQuick
import "../theme"
import "../components"
Item {
    Panel {x:8;y:0;width:312;height:44;border.color:drum.tint
        Label {x:12;y:5;width:288;text:'→ PAD '+('0'+(drum.selectedPad+1)).slice(-2)+' · '+drum.padInfo.name;face:Theme.displayFont;font.pixelSize:28;color:drum.tint}
    }
    Row {x:328;y:0;spacing:6
        Repeater {model:['ALL','kick','snare','clap','hat','skin','wood','perc']
            Panel {required property string modelData;width:94;height:44;border.color:drum.category===modelData?Theme.white:Theme.line2
                Label {anchors.centerIn:parent;text:parent.modelData.toUpperCase();font.pixelSize:11}
                TouchTarget {hardwareAction:'choice';callback:function(){drum.setCategory(parent.modelData)}}
            }
        }
    }
    Panel {x:1136;y:0;width:136;height:44
        Label {anchors.centerIn:parent;width:116;text:drum.query||'SEARCH';font.pixelSize:11}
        TouchTarget {hardwareAction:'text';callback:function(){drum.editText('query')}}
    }
    Panel {x:8;y:52;width:312;height:500
        Label {x:16;y:12;text:'LOCAL SOURCES';font.pixelSize:10;color:Theme.grayN}
        Panel {x:8;y:36;width:296;height:36;border.color:drum.source==='ALL'?Theme.white:Theme.line2
            Label {x:10;anchors.verticalCenter:parent.verticalCenter;text:'ALL SOUNDS';font.pixelSize:12}
            TouchTarget {hardwareAction:'choice';callback:function(){drum.setSource('ALL')}}
        }
        Repeater {model:drum.sources
            Panel {required property var modelData;required property int index;x:8;y:80+index*64;width:296;height:56;border.color:drum.source===modelData.id?Theme.white:Theme.line2
                Label {x:10;y:7;width:276;text:parent.modelData.name;font.pixelSize:12}
                Label {x:10;y:31;width:276;text:parent.modelData.kind+' · '+parent.modelData.licence_note;font.pixelSize:10;color:parent.modelData.licence_note.toLowerCase().indexOf('pending')>=0?Theme.amber:Theme.grayN}
                TouchTarget {hardwareAction:'choice';callback:function(){drum.setSource(parent.modelData.id)}}
            }
        }
        Label {x:16;y:358;text:'RACKS';font.pixelSize:10;color:Theme.grayN}
        Flickable {x:8;y:380;width:296;height:108;contentHeight:rackRows.height;clip:true
            Column {id:rackRows;spacing:4
                Repeater {model:drum.racks
                    Panel {required property var modelData;width:296;height:34
                        Label {x:10;anchors.verticalCenter:parent.verticalCenter;width:276;text:parent.modelData.name;font.pixelSize:11}
                        TouchTarget {hardwareAction:'list';callback:function(){drum.loadRack(parent.modelData.path)}}
                    }
                }
            }
        }
    }
    Item {x:328;y:52;width:944;height:500
        Label {x:8;y:4;text:drum.dataDir+' / '+drum.category.toUpperCase();font.pixelSize:11;color:Theme.grayN}
        Label {x:8;y:29;text:'NAME                                      SOURCE            FORMAT';font.pixelSize:10;color:Theme.mon}
        ListView {x:0;y:54;width:944;height:402;clip:true;spacing:4;model:drum.soundCount>0?drum.sounds:null;visible:drum.soundCount>0
            currentIndex:drum.sampleIndex
            onCurrentIndexChanged:if(currentIndex>=0 && currentIndex<count)positionViewAtIndex(currentIndex,ListView.Contain)
            delegate:Panel {id:sound;required property var entry;required property int index;width:944;height:48;border.width:entry.selected?2:1;border.color:entry.selected?Theme.white:Theme.line2
                Label {x:12;y:7;width:344;text:sound.entry.name;font.pixelSize:13;color:drum.tint}
                Label {x:12;y:28;width:344;text:sound.entry.category.toUpperCase();font.pixelSize:9;color:Theme.grayN}
                DrumWaveform {x:368;y:9;width:188;height:30;bars:sound.entry.layer.waveform||[];tint:drum.tint}
                Label {x:580;y:8;width:184;text:sound.entry.source.kind;font.pixelSize:11}
                Label {x:580;y:28;width:184;text:sound.entry.source.licence_note;font.pixelSize:9;color:sound.entry.source.licence_note==='pending'?Theme.amber:Theme.grayN}
                Label {x:788;y:18;width:142;text:'48k · FLOAT';font.pixelSize:11;color:Theme.grayN}
                TouchTarget {hardwareAction:'list';callback:function(){drum.selectSound(sound.index)}}
            }
        }
        Panel {x:0;y:48;width:944;height:408;visible:drum.soundCount===0;border.color:Theme.line2
            Label {x:24;y:28;text:'YOUR SOUNDS LIVE HERE';face:Theme.displayFont;font.pixelSize:48}
            Label {x:24;y:112;width:850;height:154;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:'1   Choose a local sample folder.\n\n2   Add a licence note and import.\n\n3   Audition in CUE, then load a pad.';font.pixelSize:17;color:Theme.grayN}
            Label {x:24;y:286;width:850;text:drum.dataDir+' · LOCAL ONLY';font.pixelSize:12;color:Theme.tealPlus}
            Panel {x:24;y:332;width:320;height:48;border.color:Theme.tealPlus
                Label {anchors.centerIn:parent;text:'IMPORT A FOLDER';font.pixelSize:14;color:Theme.tealPlus}
                TouchTarget {hardwareAction:'navigation';callback:function(){console.navigate('DrumImport')}}
            }
        }
        Panel {x:0;y:468;width:944;height:32;border.color:Theme.white
            Label {anchors.centerIn:parent;text:'AUDITION → CUE ONLY · NEVER THE ROOM · JOG TO BROWSE · PRESS TO LOAD';font.pixelSize:11;color:Theme.white}
        }
    }
    DrumKeyboard {x:8;y:270;width:1264;height:280;z:20;visible:drum.textTarget==='query'}
}
