import QtQuick
import "../theme"
Item {
    id: root
    property string mode: synth.pageName.replace(/^[AB] /,'')
    property var values: synth.values
    property color tint: synth.engine === 'hybrid' ? Theme.hybHi : Theme.tealPlus
    property bool fm: mode === 'OPS' || /^OP [1-6]$/.test(mode)
    property bool simple: ['OSC','FILTER','ENV','LFO'].indexOf(mode)>=0
    property bool supported: {
        let prefix=synth.pageName.indexOf('A ')===0?'a.':synth.pageName.indexOf('B ')===0?'b.':'';
        let keys=mode==='FILTER'?['filter.cutoff','filter.resonance']:mode==='OSC'?['osc.1.wave','osc.2.wave']:mode==='ENV'?['amp.attack','filter.attack']:mode==='LFO'?['lfo.shape']:fm?['fm.algorithm','op.1.ratio']:mode==='GROOVE'?['arp.on','pump.depth']:mode==='FX'?['fx.tape.wet']:mode==='MACROS'?['macro.drift']:[];
        return keys.length>0 && keys.every(k=>values[prefix+k]!==undefined);
    }
    function v(key, fallback) {let n=values[key];return n === undefined ? fallback : n;}
    function pf(key, fallback){return v((synth.pageName.indexOf('A ')===0?'a.':synth.pageName.indexOf('B ')===0?'b.':'')+key,fallback);}
    Panel { x:0;y:0;width:952;height:296;visible:root.simple && root.mode!=='OSC'
        SynthPlot { anchors.fill:parent; mode:root.mode; tint:root.tint }
        Label { x:24;y:12;width:parent.width-48;text:root.mode === 'FILTER' ? root.pf('filter.cutoff',1100).toFixed(0)+' Hz  ·  RES '+root.pf('filter.resonance',40)+' %' : root.mode === 'OSC' ? 'VCO 1 · '+['SAW','TRI','SINE','PULSE'][root.pf('osc.1.wave',0)]+'     /     VCO 2 · '+['SAW','TRI','SINE','PULSE'][root.pf('osc.2.wave',1)] : root.mode === 'ENV' ? 'AMP  /  FILTER     ·     ATTACK   DECAY   SUSTAIN   RELEASE' : 'LFO · TWO CYCLES';font.pixelSize:12;color:root.tint }
    }
    Repeater { model:root.mode==='OSC'?2:0
        Panel {required property int index;x:index*480;y:0;width:472;height:296;border.color:synth.focusedCell<4&&index===0||synth.focusedCell>=4&&index===1?root.tint:Theme.line
            SynthPlot {x:0;y:46;width:472;height:216;mode:'OSC';oscillator:parent.index+1;tint:root.tint}
            Label {x:20;y:12;width:432;text:'VCO '+(parent.index+1)+' · '+['SAW','TRI','SINE','PULSE'][root.pf('osc.'+(parent.index+1)+'.wave',0)];font.pixelSize:13;color:root.tint}
            Label {x:20;y:266;width:432;text:'LEVEL '+root.pf('osc.'+(parent.index+1)+'.level',80)+' %   ·   TUNE '+root.pf('osc.'+(parent.index+1)+'.tune',0)+' st';font.pixelSize:11;color:Theme.grayN}
        }
    }
    Panel {x:960;y:0;width:304;height:296;visible:root.simple
        Label {x:20;y:16;text:root.mode === 'FILTER' ? 'FILTER TYPE' : root.mode === 'OSC' ? 'MIX / ROUTING' : root.mode === 'ENV' ? 'ENVELOPES' : 'RATE';color:Theme.mon;font.pixelSize:12}
        Label {x:20;y:50;width:264;height:68;face:Theme.displayFont;font.pixelSize:root.mode==='LFO'?64:44;text:root.mode==='FILTER' ? (root.values[(synth.pageName.indexOf('A ')===0?'a.':synth.pageName.indexOf('B ')===0?'b.':'')+'filter.morph']!==undefined ? 'SEM MORPH' : ['LP 24','LP 12','HP','BP'][root.pf('filter.mode',0)]) : root.mode==='OSC' ? 'SYNC / RING' : root.mode==='ENV' ? 'AMP + FILTER' : root.v('lfo.sync',1) ? ['1/1','1/2','1/4','1/8','1/16','1/32','1/8T','1/16T'][root.v('lfo.rate',3)] : root.v('lfo.rate',3)+' Hz';color:root.tint}
        Label {x:20;y:126;width:264;wrapMode:Text.WordWrap;elide:Text.ElideNone;text:root.mode==='FILTER' ? 'CONTROL RESPONSE\n20 Hz – 20 kHz\nKEY TRACK '+root.pf('filter.keytrack',30)+' %' : root.mode==='OSC' ? 'VCO 1  '+root.pf('osc.1.level',80)+' %\nVCO 2  '+root.pf('osc.2.level',72)+' %\nNOISE  '+root.pf('osc.noise',10)+' %' : root.mode==='ENV' ? 'AMP     '+root.pf('amp.attack',4)+' / '+root.pf('amp.decay',220)+' / '+root.pf('amp.sustain',70)+' / '+root.pf('amp.release',620)+'\nFILTER  '+root.pf('filter.attack',4)+' / '+root.pf('filter.decay',220)+' / '+root.pf('filter.sustain',70)+' / '+root.pf('filter.release',620) : 'SYNC '+(root.v('lfo.sync',1)?'ON':'OFF')+'\n→ CUTOFF '+root.v('lfo.cutoff',40)+'\n→ PITCH '+root.v('lfo.pitch',0);lineHeight:1.7;font.pixelSize:12;color:Theme.grayN}
        Rectangle {x:20;y:246;width:264;height:20;color:Theme.line2;visible:root.mode==='FILTER' && root.values[(synth.pageName.indexOf('A ')===0?'a.':synth.pageName.indexOf('B ')===0?'b.':'')+'filter.morph']!==undefined
            Rectangle {x:(parent.width-6)*(root.pf('filter.morph',-40)+100)/200;y:-3;width:6;height:26;color:Theme.white}
        }
        Label {x:20;y:272;width:264;visible:root.mode==='FILTER';text:synth.engine==='hybrid' ? 'LP        NOTCH        HP' : '24 dB / oct · digital';font.pixelSize:10;color:Theme.mon}
    }
    Panel {x:0;y:0;width:1264;height:160;visible:root.fm
        FmAlgorithm {x:8;y:0;width:488;height:160}
        Item {x:512;y:12;width:528;height:140;visible:synth.selectedOperator===0
            Repeater {model:synth.operatorCount===6?32:8
                ActionButton {required property int index;x:(index%8)*65;y:Math.floor(index/8)*34;width:57;height:28;text:String(index+1);active:synth.algorithm.number===index+1;onClicked:synth.chooseAlgorithm(index+1)}
            }
        }
        Label {x:1060;y:20;width:188;text:'ALGORITHM '+synth.algorithm.number+'\nFEEDBACK '+root.v('fm.feedback',0)+'\n'+synth.operatorCount+' OPERATORS';lineHeight:1.8;font.pixelSize:13;color:root.tint;visible:synth.selectedOperator===0}
        SynthPlot {x:512;y:8;width:736;height:144;visible:synth.selectedOperator>0;mode:synth.operatorCount===6?'DX7 ENV':'ENV';tint:root.tint}
    }
    Repeater {model:root.fm ? (synth.selectedOperator ? 1 : Math.max(0,Math.min(4,synth.operatorCount-synth.bankIndex*4))) : 0
        Panel {required property int index;property int op:synth.selectedOperator || synth.bankIndex*4+index+1
            x:index*320;y:168;width:synth.selectedOperator?1264:312;height:128;border.color:synth.selectedOperator===op?Theme.white:Theme.line2
            Label {x:16;y:10;text:'OP '+parent.op+'  ·  '+((synth.algorithm.carriers || []).indexOf(parent.op)>=0?'CARRIER':'MODULATOR');font.pixelSize:12;color:root.tint}
            Label {x:16;y:39;face:Theme.displayFont;font.pixelSize:38;text:'× '+root.v('op.'+parent.op+'.ratio',1).toFixed(2)+'   LV '+root.v('op.'+parent.op+'.level',80)}
            Label {x:16;y:94;width:parent.width-32;text:synth.selectedOperator ? (synth.operatorCount===6?'RATE / LEVEL ENVELOPE · SUSTAIN AT L3 · RELEASE TO L4    |    BANK 1: TUNING / KEY SCALING    BANK 2: R1–R4 / L1–L4':'ADSR ENVELOPE · VELOCITY / RATIO / FINE') : 'PRESS TO FOCUS';font.pixelSize:10;color:Theme.grayN}
            MouseArea {anchors.fill:parent;onClicked:synth.selectedOperator?synth.selectPage('OPS'):synth.focusOperator(parent.op)}
        }
    }
    Item {anchors.fill:parent;visible:root.mode==='GROOVE'
        Repeater {model:['ARP','CHORD / STRUM','PUMP']
            Panel {required property int index;required property string modelData;x:index===0?0:index===1?640:960;width:index===0?632:304;height:296
                Label {x:16;y:12;text:modelData;face:Theme.displayFont;font.pixelSize:28;color:root.tint}
                Repeater {model:parent.index===0?16:parent.index===1?4:16
                    Rectangle {required property int index;property int group:parent.index;x:group===0?16+index*37:group===1?24+root.v('chord.strum',30)*index*.35:16+index*17;y:group===0?80+(4-index%5)*30:group===1?72+index*44:80+root.v('pump.depth',60)*Math.exp(-index*.4);width:group===0?Math.max(3,32*root.v('arp.gate',60)/100):group===1?200:14;height:group===2?130-root.v('pump.depth',60)*Math.exp(-index*.4):18;radius:2;color:root.tint;opacity:group===0&&!root.v('arp.on',1)?.35:1}
                }
                Label {x:16;y:260;width:parent.width-32;text:parent.index===0?'16 STEPS · GATE '+root.v('arp.gate',60)+' %':parent.index===1?'STRUM '+root.v('chord.strum',30)+' ms':'DEPTH '+root.v('pump.depth',60)+' %';font.pixelSize:12;color:Theme.grayN}
            }
        }
    }
    Item {anchors.fill:parent;visible:root.mode==='FX'
        Repeater {model:['SYNTH','DRIVE','TAPE','PHASER','CHORUS','DELAY','REVERB','MIXER']
            Panel {required property int index;required property string modelData;x:index*160;width:152;height:148;border.color:index===synth.bankIndex+1?root.tint:Theme.line
                Label {x:12;y:12;text:parent.index===0?'IN':parent.index===7?'OUT':String(parent.index);color:Theme.mon;font.pixelSize:10}
                Label {x:12;y:48;width:128;text:modelData;font.pixelSize:16;color:parent.index===synth.bankIndex+1?root.tint:Theme.white}
                Label {x:12;y:90;text:parent.index>0&&parent.index<7?'WET '+root.v('fx.'+modelData.toLowerCase()+'.wet',30)+' %':'STEREO';font.pixelSize:12;color:Theme.grayN}
                Rectangle {x:12;y:128;width:128;height:6;color:Theme.line2;Rectangle{height:6;width:parent.width*root.v('fx.'+modelData.toLowerCase()+'.wet',30)/100;color:root.tint}}
                MouseArea {anchors.fill:parent;enabled:parent.index>0&&parent.index<7;onClicked:synth.chooseBank(parent.index-1)}
            }
        }
        Panel {y:156;width:1264;height:140
            Label {x:24;y:16;face:Theme.displayFont;font.pixelSize:48;text:['DRIVE','TAPE','PHASER','CHORUS','DELAY','REVERB'][synth.bankIndex] || 'FX';color:root.tint}
            Label {x:24;y:88;text:'FIXED CHAIN · SELECT AN EFFECT ABOVE · ITS BANK OWNS THE EIGHT ENCODERS';font.pixelSize:12;color:Theme.grayN}
            SynthPlot {x:640;y:8;width:600;height:124;mode:'LFO';tint:root.tint}
        }
    }
    Repeater {model:root.mode==='MACROS'?['DRIFT','GRIT','POCKET','MOVE']:[]
        Panel {required property int index;required property string modelData;x:index*320;width:304;height:296
            ArcKnob {x:84;y:12;width:136;height:136;value:root.v('macro.'+modelData.toLowerCase(),0)/100;text:String(root.v('macro.'+modelData.toLowerCase(),0));textSize:38;tint:root.tint}
            Label {y:148;width:parent.width;text:modelData;face:Theme.displayFont;font.pixelSize:36;horizontalAlignment:Text.AlignHCenter;color:root.tint}
            Label {x:20;y:204;width:264;text:[['VCO detune','Filter cutoff','Env times'],['Drive','Tape saturation','Filter resonance'],['Arp swing','Strum','Note delay'],['LFO depth','Phaser mix','Pump depth']][parent.index].join('\n');lineHeight:1.7;font.pixelSize:12;color:Theme.grayN}
        }
    }
    Panel {anchors.fill:parent;visible:!root.supported
        Label {x:32;y:64;width:1200;text:'PARAMETER VISUALIZATION UNAVAILABLE';face:Theme.displayFont;font.pixelSize:36;color:Theme.grayN}
        Label {x:32;y:126;width:1200;text:'Use the controls below to edit this page.';font.pixelSize:14;color:Theme.grayN}
    }

}
