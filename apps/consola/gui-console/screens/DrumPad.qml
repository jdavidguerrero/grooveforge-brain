import QtQuick
import "../theme"
import "../components"
Item {
    Panel {x:8;y:0;width:152;height:44;border.color:drum.tint
        Label {anchors.centerIn:parent;text:'PAD '+('0'+(drum.selectedPad+1)).slice(-2);face:Theme.displayFont;font.pixelSize:28;color:drum.tint}
    }
    Panel {x:168;y:0;width:1112;height:44
        Label {x:12;anchors.verticalCenter:parent.verticalCenter;text:drum.padInfo.name+' · '+drum.pageName;font.pixelSize:16}
        Label {x:710;anchors.verticalCenter:parent.verticalCenter;width:390;text:'TOUCH SELECTS · ENCODERS EDIT';font.pixelSize:11;color:Theme.grayN}
    }
    Panel {x:8;y:52;width:792;height:296
        Label {x:12;y:10;width:768;text:drum.padLayers.length?drum.padLayers[drum.selectedLayer].path:'EMPTY · BROWSE TO LOAD';font.pixelSize:11;color:Theme.grayN}
        Item {id:wave;x:12;y:42;width:768;height:164;clip:true
            Rectangle {anchors.fill:parent;color:Theme.bg}
            DrumWaveform {anchors.fill:parent;anchors.margins:4;bars:drum.waveform;tint:drum.tint}
            Rectangle {width:parent.width*drum.params.start;height:parent.height;color:'#BD000000'}
            Rectangle {x:parent.width*drum.params.end;width:parent.width-x;height:parent.height;color:'#BD000000'}
            Repeater {model:[['START','start',0],['END','end',1],['FADE IN','fade_in',2],['FADE OUT','fade_out',3]]
                Item {required property var modelData;required property int index;x:index===0?wave.width*drum.params.start:index===1?wave.width*drum.params.end-4:index===2?wave.width*.1:wave.width*.8;width:Math.max(20,0);height:164
                    Rectangle {width:drum.focusedCell===parent.index?4:2;height:parent.index<2?164:14;y:parent.index<2?0:78;color:drum.focusedCell===parent.index?Theme.tealPlus:Theme.white}
                    Label {x:6;y:parent.index<2?4:88;text:parent.modelData[0];font.pixelSize:9;color:Theme.white}
                    TouchTarget {hardwareAction:'parameter';callback:function(){drum.focusHandle(parent.modelData[2])}}
                }
            }
        }
        Label {x:12;y:211;text:'0 ms';font.pixelSize:10;color:Theme.grayN}
        Label {x:550;y:211;width:230;horizontalAlignment:Text.AlignRight;text:drum.padInfo.layer_mode.toUpperCase();font.pixelSize:10;color:Theme.grayN}
        Row {x:12;y:234;spacing:8
            Label {width:104;height:50;text:'LAYERS\n'+drum.padLayers.length+' / 4';font.pixelSize:11;color:Theme.grayN}
            Repeater {model:4
                Panel {required property int index;width:156;height:50;border.width:drum.selectedLayer===index?2:1;border.color:drum.selectedLayer===index?drum.tint:Theme.line2
                    Label {x:8;y:5;text:'L'+(parent.index+1);font.pixelSize:10;color:drum.tint}
                    Label {x:8;y:25;text:parent.index<drum.padLayers.length?drum.padLayers[parent.index].velocity_min+'–'+drum.padLayers[parent.index].velocity_max:'EMPTY';font.pixelSize:11;color:Theme.grayN}
                    TouchTarget {hardwareAction:'choice';enabled:parent.index<drum.padLayers.length;callback:function(){drum.selectLayer(parent.index)}}
                }
            }
        }
    }
    Repeater {model:['PITCH','AMP ENV','FILTER','VELOCITY CURVES']
        Panel {required property int index;required property string modelData;x:808+(index%2)*236;y:52+Math.floor(index/2)*152;width:228;height:144
            Label {x:12;y:10;text:parent.modelData;font.pixelSize:10;color:Theme.grayN}
            Label {x:12;y:33;width:204;text:parent.index===0?drum.params.tune+' st  '+drum.params.cents+' ct':parent.index===1?(drum.params.env_mode?'HD':'AD')+' · '+drum.params.decay+' ms':parent.index===2?(drum.params.filter_mode?'HP':'LP')+' · '+Math.round(drum.params.cutoff)+' Hz':['LINEAR','SOFT','HARD','FIXED'][drum.params.velocity_curve];font.pixelSize:parent.index===0?24:14;color:drum.tint}
            DrumResponse {x:12;y:64;width:204;height:46;visible:parent.index>0;kind:parent.index-1;values:drum.params}
            Label {x:12;y:121;width:204;visible:parent.index!==1;text:parent.index===0?'RACK ROOT · '+drum.rack.root:parent.index===2?'RES '+Math.round(drum.params.resonance*100)+'%':'LEVEL '+Math.round(drum.params.velocity_level*100)+'% · TONE '+Math.round(drum.params.velocity_tone*100)+'%';font.pixelSize:11;color:Theme.grayN}
            TouchTarget {hardwareAction:'choice';callback:function(){if(parent.index===1)drum.setParam('env_mode',1-drum.params.env_mode);else if(parent.index===2)drum.setParam('filter_mode',1-drum.params.filter_mode);else if(parent.index===3)drum.setParam('velocity_curve',(drum.params.velocity_curve+1)%4);else drum.action('bank')}}
        }
    }
    Label {x:12;y:360;text:'BANK '+(drum.page+1)+' / 4 · '+drum.pageName+' · '+(drum.fine?'FINE':'NORMAL');font.pixelSize:11;color:drum.tint}
    Repeater {model:drum.cells
        ParameterCell {required property int index;required property var model;x:Theme.columnX(index);y:384;width:152;height:168;entry:model.entry;encoderIndex:index;tint:drum.tint;focused:drum.focusedCell===index;focusCallback:function(i){drum.focusParameter(i)}}
    }
}
