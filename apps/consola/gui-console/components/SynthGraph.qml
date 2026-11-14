import QtQuick
import "../theme"
Item {
    id: root
    property string mode: synth.pageName.replace(/^[ab]\./,'')
    property var values: synth.values
    property color tint: synth.engine === 'hybrid' ? Theme.hybHi : Theme.tealPlus
    property bool fm: synth.fmSupported && (mode === 'OPS' || mode === 'ALGO/FB' || /^OP [1-6]$/.test(mode))
    property bool simple: ['FILTER','ENV','LFO'].indexOf(mode)>=0 && synth.engine!=='dx7_6op'
    property string prefix: /^[ab]\./.test(synth.pageName)?synth.pageName.slice(0,2):''
    function v(key) {return values[prefix+key];}
    FmPanel {anchors.fill:parent;visible:root.fm}
    Panel {width:952;height:296;visible:root.simple
        SynthPlot {anchors.fill:parent;mode:root.mode;tint:root.tint}
        Label {x:24;y:12;width:904;text:root.mode==='FILTER' && root.v('cutoff')!==undefined?'CUTOFF '+root.v('cutoff').toFixed(0)+' Hz · RES '+(100*root.v('resonance')).toFixed(0)+' %':root.mode==='ENV'?'AMP / FILTER · CONTROL ENVELOPES':'LFO · CONTROL WAVEFORM';font.pixelSize:12;color:root.tint}
    }
    Panel {x:960;width:304;height:296;visible:root.simple
        Label {x:20;y:20;width:264;text:root.mode;face:Theme.displayFont;font.pixelSize:42;color:root.tint}
        Label {x:20;y:94;width:264;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:root.mode==='FILTER'?'Filter controls from the active engine.':root.mode==='ENV'?'Attack / decay / sustain / release.':'Shape and rate from the active engine.';font.pixelSize:14;color:Theme.grayN}
        Label {x:20;y:220;width:264;text:'CONTROL VISUALIZATION';font.pixelSize:11;color:Theme.mon}
    }
    FmEnvelope {width:1264;height:296;visible:root.mode==='PITCH EG' && synth.engine==='dx7_6op';six:true
        envelopeData:[1,2,3,4].map(i=>({rate:root.values['pitch.r'+i],level:root.values['pitch.l'+i]}))
    }
    Repeater {model:root.mode==='MACROS'?['drift','grit','pocket','move']:[]
        Panel {required property int index;required property string modelData;x:index*320;width:304;height:296
            ArcKnob {x:84;y:12;width:136;height:136;value:(root.values[modelData] || 0)/100;text:String(root.values[modelData] || 0);textSize:38;tint:root.tint}
            Label {y:160;width:parent.width;text:modelData.toUpperCase();face:Theme.displayFont;font.pixelSize:36;horizontalAlignment:Text.AlignHCenter;color:root.tint}
        }
    }
    Item {anchors.fill:parent;visible:!root.fm && !root.simple && root.mode!=='MACROS' && root.mode!=='PITCH EG'
        Repeater {model:synth.cells
            Panel {required property int index;required property var entry;x:index*160;width:152;height:296;visible:!entry.empty
                Label {x:12;y:16;width:128;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:entry.name;font.pixelSize:13;color:root.tint}
                Label {x:12;y:78;width:128;text:entry.text;face:Theme.displayFont;font.pixelSize:32}
                Rectangle {x:12;y:160;width:128;height:8;color:Theme.line2
                    Rectangle {width:parent.width*entry.amount;height:8;color:root.tint}
                }
                Label {x:12;y:204;width:128;text:entry.unit;font.pixelSize:13;color:Theme.grayN}
            }
        }
    }
}
