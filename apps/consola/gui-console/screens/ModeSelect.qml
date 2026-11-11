import QtQuick
import "../theme"
import "../components"
Item {
    Repeater { model: ['LIVE · CTRL','LIVE','SYNTH','DRUM','SEQ','MIX']
        Panel {
            required property int index
            required property string modelData
            x: 24+index*207; y: 64; width: 197; height: 400
            color: Theme.gpBgCard; radius: Theme.rOs
            border.width: 2; border.color: buttons.menuIndex === index ? Theme.gpTeal : Theme.gpBorder
            Label { x: 16; y: 22; text: 'ENC '+(index+1); color: Theme.mon; font.pixelSize: 12 }
            Label { x: 16; y: 78; text: modelData; face: Theme.osFont; font.pixelSize: 25 }
            Label { x: 16; y: 140; width: 165; height: 140; wrapMode: Text.WordWrap; elide: Text.ElideNone; font.pixelSize: 15; color: Theme.gpTextSec
                text: ['Control Live scenes and racks. Connection pending.', 'Perform the set. Núcleo integration pending.', 'Play four synth parts. Edit engines and presets.', 'Play sample racks, sequence steps and jam.', 'Sequence steps and piano roll. Design preview.', 'Mix controller tracks. Connection pending.'][index] }
            Label { x:16; y:326; width:165; text: index===2?'SYNTH HOST':index===3?'DRUM HOST':'DESIGN PREVIEW'; font.pixelSize:12; color:Theme.gpTeal }
            TouchTarget { hardwareAction:'choice'; callback:function(){ console.triggerAction('mode:'+parent.index) } }
        }
    }
    Label { y: 510; width:1000; horizontalAlignment:Text.AlignHCenter; text:'JOG TO SELECT · CLICK OK · ENCODER 1–6 OR TOUCH'; font.pixelSize:12; color:Theme.mon }
    ActionButton { x:1059; y:492; width:197; height:60; text:'SETTINGS'; border.color:buttons.menuIndex===6?Theme.gpTeal:Theme.gpBorder; callback:function(){console.navigate('Settings')} }
}
