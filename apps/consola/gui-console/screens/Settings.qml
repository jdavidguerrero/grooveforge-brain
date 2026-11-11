import QtQuick
import "../theme"
import "../components"
Item {
    Rectangle { anchors.fill: parent; color: Theme.gpBgBase }
    Column { x:24; y:24; spacing:8
        Repeater { model:["Audio","Connections","Surface","System"]
            Panel { required property int index; required property string modelData
                width:216; height:56; color:modelData==='Surface'?Theme.gpBgCard:'transparent'; border.color:modelData==='Surface'?Theme.gpTeal:Theme.gpBorder
                Label { x:16; height:56; text:modelData; face:Theme.osFont; font.pixelSize:18 }
                TouchTarget { hardwareAction:'navigation'; callback:function(){console.triggerAction('settings:'+parent.modelData)} }
            }
        }
    }
    Label { x:288; y:24; text:'Surface / Hands'; face:Theme.osFont; font.pixelSize:26 }
    Label { x:288; y:66; text:'Local preferences · applied in SYNTH, DRUM and LIVE'; font.pixelSize:13; color:Theme.gpTextSec }
    Repeater { model:[{title:'Hands · master',on:preferences.handsEnabled},{title:'Left IR',on:preferences.leftHand},{title:'Right IR',on:preferences.rightHand}]
        Panel { required property int index; required property var modelData
            x:288+index*324; y:110; width:308; height:148; color:Theme.gpBgCard; border.color:modelData.on?Theme.gpTeal:Theme.gpBorder
            Label { x:20; y:18; text:modelData.title; face:Theme.osFont; font.pixelSize:20 }
            Label { x:20; y:60; text:modelData.on?'ON':'OFF'; font.pixelSize:28; color:modelData.on?Theme.gpTeal:Theme.gpTextSec }
            Label { x:20; y:112; text:'TAP OR PRESS ENC '+(parent.index+1); font.pixelSize:11; color:Theme.mon }
            TouchTarget { objectName:'handsToggle'+parent.index; hardwareAction:'choice'; callback:function(){preferences.toggleHand(parent.index)} }
        }
    }
    Label { x:288; y:272; width:940; text:'Master and the individual hand must be ON. Disabled sensors ignore hand movement and presence in every mode. Expression pedals remain active.'; wrapMode:Text.WordWrap; font.pixelSize:14; color:Theme.gpTextSec }
    Panel { x:288; y:322; width:956; height:122; color:Theme.gpBgCard; border.color:Theme.gpBorder
        Label { x:20; y:16; text:'DRUM · M4 pad velocity'; face:Theme.osFont; font.pixelSize:20 }
        Label { x:20; y:58; text:'Global fixed velocity: '+preferences.padVelocity+'   ·   FSR pads keep their measured velocity'; font.pixelSize:14; color:Theme.gpTextSec }
        Row { x:20; y:78; spacing:12
            Repeater { model:[64,100,127]
                ActionButton { required property int modelData; width:140; height:38; text:String(modelData)+(preferences.padVelocity===modelData?' · ACTIVE':''); callback:function(){preferences.setPadVelocity(modelData)} }
            }
        }
    }
    Panel { x:288; y:456; width:956; height:70; color:Theme.gpBgCard; border.color:Theme.gpBorder
        Label { x:20; y:12; text:'NeoTrellis LEDs'; face:Theme.osFont; font.pixelSize:18 }
        ActionButton { x:256; y:16; width:300; height:40; text:'Brightness · '+Math.round(preferences.ledBrightness*100/127)+'% · ENC 5'; callback:function(){preferences.cycleLedBrightness()} }
        ActionButton { x:572; y:16; width:364; height:40; text:'Animations · '+(preferences.ledAnimations?'ON':'OFF')+' · ENC 6'; callback:function(){preferences.toggleLedAnimations()} }
    }
    Label { x:288; y:540; width:956; text:preferences.error || 'Saved on this console · hands default OFF · pad velocity default 100'; font.pixelSize:12; color:preferences.error?Theme.amber:Theme.mon }
}
