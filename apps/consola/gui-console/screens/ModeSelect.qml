import QtQuick
import "../theme"
import "../components"
Item {
    Repeater { model: ['Controller','Standalone','Synth','Drum']
        Panel {
            required property int index
            required property string modelData
            x: 48+index*300; y: 40; width: 284; height: 448
            color: Theme.gpBgCard; radius: Theme.rOs
            border.width: 2; border.color: console.selectedMode === index ? Theme.gpTeal : Theme.gpBorder
            Label { x: 22; y: 22; text: 'ENC '+(index+1); color: Theme.mon; font.pixelSize: 12 }
            Label { x: 22; y: 78; text: modelData; face: Theme.osFont; font.pixelSize: 34 }
            Label { x: 22; y: 140; width: 240; height: 130; wrapMode: Text.WordWrap; elide: Text.ElideNone; font.pixelSize: 15; color: Theme.gpTextSec
                text: index===3 ? 'Play sample racks. Shape each pad, sequence polymeters and jam.' : index === 2 ? 'Play four synth parts. Pads, keys, faders and hands shape the sound.' : index === 1 ? 'The engine offers layers. You decide and perform the set.' : 'Push-style control of Ableton Live, plus the Mac engine.' }
            Label { x:22; y:300; text:index===3?'RACK':index===2?'PARTS':'CONNECTION';font.pixelSize:10;color:Theme.mon }
            Label { x:22; y:322; width:240;text:index===3?drum.rack.name:index===2?'4 · gf-synth-host':'DESIGN DEMO';font.pixelSize:12;color:Theme.gpTeal }
            Label { x:22; y:366; text:index===3?'SAMPLES RAM':index===2?'PLAY DIRECTLY':'MODE';font.pixelSize:10;color:Theme.mon }
            Label { x:22; y:388; width:240;text:index===3?drum.ramText:index===2?'PARTS → ENGINE → PRESETS':'CONTROLLER / STANDALONE';font.pixelSize:12;color:Theme.gpTextSec }
            TouchTarget { hardwareAction:'choice'; callback:function(){ console.triggerAction('mode:'+parent.index) } }
        }
    }
    Label { y: 520; width:parent.width; horizontalAlignment:Text.AlignHCenter; text:'ENC 1–4 TO SELECT · PRESS TO ENTER · OR TAP'; font.pixelSize:12; color:Theme.mon }
    ActionButton { x:1080; y:512; width:176; height:44; text:'SETTINGS'; callback:function(){console.navigate('Settings')} }
}
