import QtQuick
import "../theme"
import "../components"
Item {
    id:root
    property bool trouble:console.screenName==='DrumRackStates' || drum.error.length>0
    property bool empty:console.screenName==='DrumRackEmpty' || drum.rack.id==='empty'
    Panel {x:8;y:0;width:1264;height:44;border.color:root.trouble?Theme.amber:Theme.line2
        Label {x:12;y:6;width:310;text:drum.rack.name;face:Theme.displayFont;font.pixelSize:28}
        Label {x:324;y:15;text:drum.modified?'MODIFIED':'';font.pixelSize:10;color:Theme.amber}
        Row {x:440;y:6;spacing:28
            Repeater {model:[['TUNED',drum.rack.root+' '+drum.rack.mode],['TEMPO',Number(drum.clock.bpm).toFixed(1)],['SWING',Math.round(drum.clock.swing*100)+'%']]
                Column {required property var modelData;spacing:2
                    Label {text:parent.modelData[0];font.pixelSize:9;color:Theme.grayN}
                    Label {text:parent.modelData[1];font.pixelSize:13}
                }
            }
        }
        Label {x:828;y:15;text:drum.rack.sources.length+' SOURCES';font.pixelSize:10;color:Theme.grayN}
        Item {x:1098;y:5;width:150;height:34
            Label {text:'SAMPLES RAM';font.pixelSize:9;color:Theme.grayN}
            Label {y:13;text:drum.ramText;font.pixelSize:10}
            Rectangle {y:29;width:150;height:5;color:Theme.line2
                Rectangle {width:150*drum.ramRatio;height:5;color:root.trouble?Theme.amber:Theme.tealPlus}
                Rectangle {x:120;width:2;height:5;color:Theme.white}
            }
        }
    }
    Repeater {model:drum.pads
        DrumPadCell {required property int index;required property var model;x:Theme.columnX(index%8);y:52+Math.floor(index/8)*152;width:152;height:144;entry: model.entry
        }
    }
    Label {x:12;y:356;text:'PAD '+('0'+(drum.selectedPad+1)).slice(-2)+' · '+drum.padInfo.name+'  /  '+drum.pageName;font.pixelSize:11;color:drum.tint}
    Label {x:820;y:356;width:448;horizontalAlignment:Text.AlignRight;text:'SELECT · DOUBLE TAP / JOG PRESS TO EDIT';font.pixelSize:10;color:Theme.grayN}
    Item {x:0;y:384;width:1280;height:168;visible:!root.empty && !root.trouble
        Repeater {model:drum.cells
            ParameterCell {required property int index;required property var model;x:Theme.columnX(index);width:152;height:168;entry:model.entry;encoderIndex:index;tint:drum.tint;focused:drum.focusedCell===index;focusCallback:function(i){drum.focusParameter(i)}}
        }
    }
    Panel {x:8;y:384;width:1264;height:168;visible:root.empty;border.color:Theme.line2
        Label {x:20;y:18;text:'NO RACK LOADED';face:Theme.displayFont;font.pixelSize:44}
        Label {x:20;y:76;width:650;height:62;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:'Load an engine kit or import a local sample folder.\nEvery pad can also be filled from BROWSE.';font.pixelSize:14;color:Theme.grayN}
        Row {x:760;y:26;spacing:12
            Repeater {model:[['BROWSE','DrumBrowser'],['IMPORT','DrumImport']]
                Panel {required property var modelData;width:224;height:52;border.color:Theme.tealPlus
                    Label {anchors.centerIn:parent;text:parent.modelData[0];font.pixelSize:14}
                    TouchTarget {hardwareAction:'navigation';callback:function(){console.navigate(parent.modelData[1])}}
                }
            }
        }
        Panel {x:760;y:90;width:460;height:52;border.color:Theme.purpleBright
            Label {anchors.centerIn:parent;text:'RENDER ENGINE KIT · '+drum.rack.root;font.pixelSize:14;color:Theme.purpleBright}
            TouchTarget {hardwareAction:'choice';callback:function(){drum.renderRack()}}
        }
    }
    Repeater {model:root.trouble&&!root.empty?2:0
        Panel {required property int index;x:8+index*636;y:384;width:628;height:168;border.color:Theme.amber
            Label {x:16;y:12;text:parent.index===0?'RACK NOT REPLACED':'LOCAL ASSET STATUS';face:Theme.displayFont;font.pixelSize:30;color:Theme.amber}
            Label {x:16;y:54;width:596;height:50;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:parent.index===0?drum.error:'Missing and loading pads cannot sound. Previous rack stays in RAM until a replacement validates.';font.pixelSize:13;color:Theme.grayN}
            Panel {x:16;y:116;width:280;height:38;border.color:Theme.amber
                Label {anchors.centerIn:parent;text:'BROWSE / REPLACE';font.pixelSize:12}
                TouchTarget {hardwareAction:'navigation';callback:function(){console.navigate('DrumBrowser')}}
            }
        }
    }
}
