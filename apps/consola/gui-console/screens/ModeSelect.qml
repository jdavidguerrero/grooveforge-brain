import QtQuick
import "../theme"
import "../components"
Item {
    Repeater { model: ['Controller','Standalone','Synth']
        Panel {
            required property int index
            required property string modelData
            x: 24+index*420; y: 40; width: 392; height: 448
            color: Theme.gpBgCard; radius: Theme.rOs
            border.width: 2; border.color: console.selectedMode === index ? Theme.gpTeal : Theme.gpBorder
            Label { x: 24; y: 24; text: 'ENC '+(index+1); color: Theme.mon; font.pixelSize: 12 }
            Label { x: 24; y: 84; text: modelData; face: Theme.osFont; font.pixelSize: 40 }
            Label { x: 24; y: 160; width: 344; height: 160; wrapMode: Text.WordWrap; elide: Text.ElideNone; font.pixelSize: 18; color: Theme.gpTextSec
                text: index === 2 ? 'Play four synth parts. Pads, keys, faders and hands control the sound directly.' : index === 1 ? 'The engine generates layers; you perform the set.' : 'Push-style control of Ableton Live, plus the Mac engine.' }
            Label { x:24; y:350; width:344; height:72; wrapMode:Text.WordWrap; elide:Text.ElideNone; font.pixelSize:13; color:Theme.gpTeal; text: index===2 ? 'PARTS → ENGINE → PRESETS\nPlay directly. Shape every part.' : 'DESIGN DEMO' }
            TouchTarget { hardwareAction:'choice'; callback:function(){ console.triggerAction('mode:'+parent.index) } }
        }
    }
    Label { y: 520; width:parent.width; horizontalAlignment:Text.AlignHCenter; text:'ENC 1–3 TO SELECT · PRESS TO ENTER · OR TAP'; font.pixelSize:12; color:Theme.mon }
}
