import QtQuick
import "../theme"
import "../components"
Item {
    Repeater {model:drum.groups
        Panel {id:group;required property var modelData;required property int index;x:8+index*320;y:0;width:312;height:140;border.color:modelData.tint
            Label {x:12;y:8;text:group.modelData.name;face:Theme.displayFont;font.pixelSize:28;color:group.modelData.tint}
            Label {x:12;y:47;text:group.modelData.level? (20*Math.log10(group.modelData.level)).toFixed(1)+' dB':'−∞ dB';face:Theme.displayFont;font.pixelSize:34}
            Label {x:194;y:16;text:'FADER '+(group.index+1);font.pixelSize:10;color:Theme.grayN}
            Rectangle {x:250;y:44;width:28;height:76;radius:4;color:Theme.line2
                Rectangle {y:(1-group.modelData.level/2)*60;width:28;height:16;radius:3;color:group.modelData.tint}
            }
            Rectangle {x:289;y:44;width:7;height:76;color:Theme.line2;Rectangle {anchors.bottom:parent.bottom;width:7;height:76*Math.min(1,group.modelData.meter);color:group.modelData.tint}}
            Rectangle {x:289;y:38;width:7;height:3;color:Theme.red;visible:group.modelData.meter>1}
            Flickable {x:12;y:102;width:226;height:28;contentWidth:chips.width;clip:true
                Row {id:chips;spacing:4
                    Repeater {model:group.modelData.members
                        Panel {required property var modelData;width:70;height:24;border.color:group.modelData.tint
                            Label {anchors.centerIn:parent;width:60;text:parent.modelData.name;font.pixelSize:9}
                            TouchTarget {hardwareAction:'choice';callback:function(){drum.regroup(parent.modelData.index)}}
                        }
                    }
                }
            }
            TouchTarget {x:244;y:38;width:42;height:88;anchors.fill:null;hardwareAction:'parameter';callback:function(){touch.inspect('FADER '+(group.index+1)+' · '+group.modelData.name)}}
        }
    }
    Repeater {model:drum.strips
        Panel {id:strip;required property var modelData;required property int index;x:Theme.columnX(index);y:148;width:152;height:404;opacity:modelData.mute || modelData.dimmed?.35:1
            Rectangle {width:152;height:30;color:strip.modelData.tint
                Label {anchors.centerIn:parent;text:'PAD '+('0'+(strip.modelData.index+1)).slice(-2);font.pixelSize:12;color:Theme.bg}
            }
            Label {x:10;y:42;width:132;text:strip.modelData.name;font.pixelSize:12}
            Label {x:10;y:69;text:'PAN';font.pixelSize:10;color:Theme.grayN}
            Label {x:90;y:69;text:'SEND';font.pixelSize:10;color:Theme.grayN}
            Repeater {model:['pan','send']
                Panel {required property string modelData;required property int index;x:10+index*80;y:87;width:52;height:40;border.color:strip.modelData.tint
                    Label {anchors.centerIn:parent;text:Number(strip.modelData[parent.modelData]).toFixed(2);font.pixelSize:11}
                    TouchTarget {hardwareAction:'parameter';callback:function(){drum.selectPad(strip.modelData.index);drum.action(parent.modelData);console.selectEncoder(strip.index)}}
                }
            }
            Row {x:10;y:140;spacing:8
                Repeater {model:['mute','solo']
                    Panel {required property string modelData;width:62;height:28;color:strip.modelData[modelData]?strip.modelData.tint:Theme.surface2
                        Label {anchors.centerIn:parent;text:parent.modelData==='mute'?'M':'S';font.pixelSize:12;color:strip.modelData[parent.modelData]?Theme.bg:Theme.white}
                        TouchTarget {hardwareAction:'choice';callback:function(){drum.togglePad(strip.modelData.index,parent.modelData)}}
                    }
                }
            }
            Rectangle {x:60;y:186;width:28;height:154;radius:4;color:Theme.line2
                Rectangle {y:(1-strip.modelData.level/2)*138;width:28;height:16;radius:3;color:Theme.grayN}
            }
            Rectangle {x:103;y:186;width:7;height:154;color:Theme.line2;Rectangle {anchors.bottom:parent.bottom;width:7;height:154*Math.min(1,strip.modelData.meter);color:strip.modelData.tint}}
            Rectangle {x:114;y:186;width:7;height:154;color:Theme.line2;Rectangle {anchors.bottom:parent.bottom;width:7;height:154*Math.min(1,strip.modelData.meter);color:strip.modelData.tint}}
            Rectangle {x:103;y:180;width:18;height:3;color:Theme.red;visible:strip.modelData.meter>1}
            Label {x:10;y:356;width:132;horizontalAlignment:Text.AlignHCenter;text:strip.modelData.level?(20*Math.log10(strip.modelData.level)).toFixed(1)+' dB':'−∞ dB';font.pixelSize:16}
            Label {x:10;y:382;width:132;horizontalAlignment:Text.AlignHCenter;text:'ENC '+(strip.index+1);font.pixelSize:9;color:Theme.grayN}
            TouchTarget {x:24;y:178;width:70;height:170;anchors.fill:null;hardwareAction:'parameter';callback:function(){drum.selectPad(strip.modelData.index);console.selectEncoder(strip.index)}}
        }
    }
}
