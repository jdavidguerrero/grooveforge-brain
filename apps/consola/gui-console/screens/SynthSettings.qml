import QtQuick
import "../theme"
import "../components"
Item {
    id:root
    property int section:console.screenName==='SynthSettingsAudio'?0:console.screenName==='SynthSettingsConnections'?1:2
    property bool preview:synth.demo
    property var cards:section===0 ? [
        {title:'Sample rate',pill:'HOST',choices:['44.1 kHz','48 kHz','96 kHz'],active:[44100,48000,96000].indexOf(synth.hostInfo.sample_rate || 48000),note:'Audio settings are read only.',value:(synth.hostInfo.sample_rate || 48000)/1000+' kHz'},
        {title:'Room out → Audio+',pill:'MAIN',rows:['Audio+ · line 1–2|48 kHz · 24 bit'],meters:[.72,.69],note:'Main stereo mix'},
        {title:'Buffer',pill:'HOST',choices:['64','128','256','512'],active:[64,128,256,512].indexOf(synth.hostInfo.block || 128),value:String(synth.hostInfo.block || '—'),note:'Block size reported by the synth host.'},
        {title:'Cue out → Teensy headphones',pill:'CUE',meters:[.38,.36],note:preview?'Preview · 64 %':'USB routing status unavailable'},
        {title:'Input gain · IN',pill:preview?'+6 dB':'UNAVAILABLE',meters:[.46],note:'Line input on the Teensy · read only'},
        {title:'Brain USB link',pill:preview?'SAMPLE':'UNAVAILABLE',rows:preview?['Audio blocks|1 204 331 · 2 late','Dropped MIDI|0','Underruns|0']:['USB counters|Unavailable','Synth host|'+synth.status],note:preview?'Design sample counters':'USB connection service required'}
    ] : section===1 ? [
        {title:'Ethernet → Mac',pill:preview?'SAMPLE':'UNAVAILABLE',rows:preview?['This console|10.0.0.24 · 1 Gb','Mac · GroovePilot|10.0.0.1 · 0.4 ms','Ableton Live remote|Not answering']:['Ethernet|Unavailable'],note:'Live remote and network status are independent of BRAIN.'},
        {title:'USB devices',pill:preview?'SAMPLE':'UNAVAILABLE',rows:preview?['GF Superficie|Pads · encoders · faders · IR','GrooveForge Brain|Audio + MIDI · fw 2.4.1']:['Teensy|USB service required'],note:'USB devices shown from the design sample.'},
        {title:'Ableton Link',pill:preview?'3 PEERS':'UNAVAILABLE',rows:preview?['Mac · Live 12|124.0 BPM','iPad · drum app|124.0 BPM','Console|This device']:['Peers|Unavailable'],note:'With no peers, tempo stays local.'},
        {title:'MIDI devices',pill:preview?'SAMPLE':'UNAVAILABLE',rows:preview?['Keystep 37|IN · ch 1','USB MIDI out|OUT · clock + notes','DIN in|Nothing received']:['MIDI devices|Unavailable'],note:'Synth host MIDI channels 1–4 address parts 1–4.'}
    ] : [
        {title:'Versions',pill:preview?'UPDATE READY':'HOST',rows:['GUI|0.1.0','Synth host|'+(synth.hostInfo.version || '—'),'Engine|'+(preview?'0.4.2':'Unavailable'),'Surface firmware|'+(preview?'1.3.0':'Unavailable')],note:''},
        {title:'Storage · NVMe',pill:preview?'SAMPLE':'UNAVAILABLE',value:preview?'148 GB free':'—',bar:.42,rows:[],note:preview?'108 / 256 GB used · samples 71 GB · sets 34 GB':'Storage status unavailable'},
        {title:'Update from the Mac',pill:'UNAVAILABLE',buttons:['Install 0.1.1','Later'],note:'Update service unavailable'},
        {title:'Temperature',pill:preview?'NORMAL':'UNAVAILABLE',value:preview?'54 °C':'—',bar:.54,note:'Throttles at 80 °C'},
        {title:'Synth performance',pill:'CPU',rows:['Mean CPU|'+(synth.cpu.mean_pct || 0).toFixed(1)+' %','Max CPU|'+(synth.cpu.max_pct || 0).toFixed(1)+' %','Xruns|'+(synth.cpu.xruns || 0)],note:'One core · host CPU event'},
        {title:'Power',pill:'UNAVAILABLE',buttons:['Restart','Shut down'],note:'System service unavailable · 2 s hold required when connected'}
    ]
    Rectangle {anchors.fill:parent;color:Theme.gpBgBase}
    Repeater {model:['Audio','Connections','Surface','System']
        Panel {required property int index;required property string modelData;x:24;y:24+index*64;width:216;height:56;color:console.screenTitle===modelData?Theme.gpBgCard:'transparent';border.color:console.screenTitle===modelData?Theme.gpTeal:'transparent'
            Label {x:16;y:0;height:56;text:modelData;face:Theme.osFont;font.pixelSize:18;color:Theme.gpText}
            Label {anchors.right:parent.right;anchors.rightMargin:12;height:56;text:String(parent.index+1);face:Theme.osMonoFont;font.pixelSize:12;color:Theme.gpTextSec}
            TouchTarget { hardwareAction: "navigation";anchors.fill:parent;callback: function() { console.navigate(['SynthSettingsAudio','SynthSettingsConnections','Settings','SynthSettingsSystem'][parent.index]) }}
        }
    }
    Label {x:288;y:10;width:968;height:22;text:preview?'DESIGN PREVIEW · SAMPLE DATA':'SYNTH HOST · '+synth.status.toUpperCase()+' · OTHER SERVICES UNAVAILABLE';face:Theme.osMonoFont;font.pixelSize:11;color:Theme.gpTextSec}
    Repeater {model:root.cards
        Panel {required property int index;required property var modelData
            touchCallback:function(){touch.inspect(modelData.title+" · "+modelData.note)}
            x:288+index%2*492;y:40+Math.floor(index/2)*(root.cards.length>4?172:254);width:476;height:root.cards.length>4?160:238;color:Theme.gpBgCard;border.color:Theme.gpBorder
            Label {x:16;y:10;width:300;height:24;text:modelData.title;face:Theme.osFont;font.pixelSize:16;color:Theme.gpText}
            Label {x:318;y:10;width:142;height:24;text:modelData.pill;face:Theme.osMonoFont;font.pixelSize:10;color:modelData.pill==='UNAVAILABLE'?Theme.gpTextSec:Theme.gpTeal;horizontalAlignment:Text.AlignRight}
            Row {x:16;y:46;spacing:8;visible:modelData.choices!==undefined
                Repeater {model:modelData.choices || []
                    Panel {required property int index;required property string modelData;touchCallback:function(){touch.inspect("Host audio setting · read only")};width:100;height:44;color:index===parent.parent.modelData.active?'#003D33':Theme.gpBgBase;border.color:index===parent.parent.modelData.active?Theme.gpTeal:Theme.gpBorder
                        Label {anchors.centerIn:parent;text:modelData;face:Theme.osMonoFont;font.pixelSize:12}
                    }
                }
            }
            Column {x:16;y:44;spacing:8;visible:modelData.rows!==undefined
                Repeater {model:modelData.rows || []
                    Panel {required property string modelData;touchCallback:function(){touch.inspect(modelData.replace("|", " · ")+" · read only")};width:444;height:root.cards.length>4?20:32
                        Rectangle {x:0;anchors.verticalCenter:parent.verticalCenter;width:8;height:8;radius:4;color:root.preview?Theme.gpTeal:Theme.gpTextSec}
                        Label {x:20;y:0;height:parent.height;width:218;text:modelData.split('|')[0];face:Theme.osFont;font.pixelSize:12;color:Theme.gpText}
                        Label {x:242;y:0;width:202;height:parent.height;text:modelData.split('|')[1] || '';face:Theme.osMonoFont;font.pixelSize:11;horizontalAlignment:Text.AlignRight;color:Theme.gpTextSec}
                    }
                }
            }
            Column {x:16;y:78;spacing:12;visible:modelData.meters!==undefined
                Repeater {model:modelData.meters || []
                    Item {required property int index;required property real modelData;width:444;height:12
                        Label {x:0;y:-4;text:index===0?'L':'R';face:Theme.osMonoFont;font.pixelSize:11;color:Theme.gpTextSec}
                        Rectangle {x:28;y:0;width:320;height:10;radius:3;color:Theme.gpBorder;Rectangle {width:parent.width*(root.preview?modelData:0);height:10;radius:3;color:Theme.gpTeal}}
                        Label {x:364;y:-4;text:root.preview?(-6-index)+' dB':'—';face:Theme.osMonoFont;font.pixelSize:11;color:Theme.gpTextSec}
                    }
                }
            }
            Label {x:16;y:44;visible:modelData.value!==undefined&&modelData.choices===undefined;width:444;height:38;text:modelData.value || '';face:Theme.osFont;font.pixelSize:26;color:Theme.gpTeal}
            Rectangle {x:16;y:90;width:444;height:8;visible:modelData.bar!==undefined;color:Theme.gpBorder;Rectangle{width:parent.width*(root.preview?modelData.bar || 0:0);height:8;color:Theme.gpTeal}}
            Row {x:16;y:52;spacing:12;visible:modelData.buttons!==undefined
                Repeater {model:modelData.buttons || []
                    ActionButton {required property string modelData;width:210;height:48;text:modelData;opacity:.6;callback:function(){touch.inspect(modelData+" · service unavailable")}}
                }
            }
            Label {x:16;y:parent.height-34;width:444;height:28;text:modelData.note || '';wrapMode:Text.WordWrap;face:Theme.osFont;font.pixelSize:11;color:Theme.gpTextSec}
        }
    }
}
