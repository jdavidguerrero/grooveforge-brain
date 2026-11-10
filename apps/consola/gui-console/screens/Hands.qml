import QtQuick
import "../theme"
import "../components"
Item {
    Repeater { model: console.hands
        Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; id: hand
            required property int index
            required property string side
            required property string target
            required property real distance
            required property bool inRange
            required property string value
            required property int rangeMin
            required property int rangeMax
            x: Theme.handsPanelX+index*(Theme.handsPanelWidth+Theme.s5); y: Theme.handsPanelY; width: Theme.handsPanelWidth; height: Theme.handsPanelHeight
            Label { x: Theme.s5; y: Theme.s5; text: hand.side; font.pixelSize: Theme.m12; color: Theme.mon }
            Label { x: Theme.s5; y: Theme.topHeight; text: hand.target; font.pixelSize: Theme.dXl; face: Theme.displayFont }
            Label { x: Theme.s5; y: Theme.modalTileHeight; width: hand.index ? Theme.handsLadderX-2*x : Theme.handsRulerX-2*x; height: Theme.topHeight; wrapMode: Text.WordWrap; text: hand.index ? "Theremin in A minor · plays the LEAD layer" : "One-knob filter on the master · near = closed"; font.pixelSize: Theme.m14; color: Theme.grayN }
            Label { x: Theme.s5; y: Theme.handsReadoutY-Theme.s5; text: hand.inRange ? "● HAND IN RANGE" : "NO HAND"; color: Theme.tealPlus; font.pixelSize: Theme.m12 }
            Label { x: Theme.s5; y: Theme.handsReadoutY; text: hand.index ? hand.value : hand.distance+" CM"; color: hand.index ? Theme.roleColor("LEAD") : Theme.tealPlus; font.pixelSize: Theme.handsReadoutSize; face: Theme.displayFont }
            Label { x: Theme.s5; y: Theme.handsValueY; text: hand.index ? hand.distance+" cm" : hand.value; font.pixelSize: Theme.trackSize }
            Label { x: Theme.s5; y: Theme.handsFootY; width: hand.width-2*x; text: hand.index ? "RANGE 6–28 CM · 1 OCTAVE · QUANTISE ON" : "RANGE 6–26 CM · CURVE EXP · RETURN 300 MS"; font.pixelSize: Theme.m11; color: Theme.mon }
            Item { visible: hand.index === 0; x: Theme.handsRulerX; y: Theme.s5; width: Theme.topHeight+Theme.s5; height: parent.height-2*y
                Rectangle { width: Theme.hairline; height: parent.height; color: Theme.line }
                Repeater { model: Theme.rulerTicks
                    Item { y: (1-index/(Theme.rulerTicks-1))*(parent.height-Theme.line11); width: parent.width; height: Theme.line11
                        Rectangle { width: index%2 ? Theme.rCell : Theme.m14; height: Theme.hairline; color: Theme.off }
                        Label { x: Theme.m28; y: -Theme.rCell; text: index%2 ? "" : (Theme.handMin+index*2)+" cm"; font.pixelSize: Theme.tiny; color: Theme.mon }
                    }
                }
                Rectangle { x: -Theme.s1; y: (1-(hand.distance-Theme.handMin)/(Theme.handMax-Theme.handMin))*parent.height; width: parent.width; height: Theme.edge+Theme.hairline; color: Theme.tealPlus }
            }
            Column { visible: hand.index === 1; x: Theme.handsLadderX; y: Theme.s5; width: Theme.handsLadderWidth; spacing: Theme.s1
                Repeater { model: ["A4","G4","F4","E4","D4","C4","B3","A3"]
                    Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; width: Theme.handsLadderWidth; height: (Theme.handsPanelHeight-2*Theme.s5-7*Theme.s1)/Theme.columns; color: modelData === hand.value ? Theme.roleColor("LEAD") : Theme.rollBg; border.color: modelData.startsWith("A") ? Theme.teal : Theme.line; radius: Theme.s1
                        Label { x: Theme.s3; anchors.verticalCenter: parent.verticalCenter; text: modelData; font.pixelSize: Theme.m18; color: modelData === hand.value ? Theme.bg : Theme.white }
                        Label { anchors.right: parent.right; anchors.rightMargin: Theme.s3; anchors.verticalCenter: parent.verticalCenter; text: Math.round(hand.rangeMax-index*Theme.noteCmStep)+" cm"; font.pixelSize: Theme.tiny; color: modelData === hand.value ? Theme.bg : Theme.mon }
                    }
                }
            }
        }
    }
    Repeater { model: 2
        Image { x: index ? Theme.rootWidth-Theme.handHaloWidth : 0; y: index ? Theme.handsReadoutY-Theme.s6 : Theme.handsReadoutY+Theme.s6; width: Theme.handHaloWidth; height: Theme.handHaloHeight; mirror: index === 1; source: "../assets/halo.png" }
    }
}
