import QtQuick
import "../theme"
import "../components" as C
Item {
    id:transitionRoot
    property int transitionIndex:0
    LiveSession { anchors.fill: parent }
    C.Window {
        y: -Theme.contentY; width: Theme.rootWidth; height: Theme.rootHeight
        C.Label { x: Theme.s5; y: Theme.s5; text: "TRANSITION"; face: Theme.displayFont; weight: Font.ExtraBold; font.pixelSize: Theme.dM }
        C.Label { x: Theme.settingsNavWidth; y: Theme.s5; text: console.roomTitle+" → "+console.nextTitle; font.pixelSize: Theme.m14 }
        C.Label { anchors.right: parent.right; anchors.rightMargin: Theme.s5; y: Theme.s5; text: "A MIN ✓ · 124 ✓"; color: Theme.tealPlus; font.pixelSize: Theme.m12 }
        Repeater { model: [{n:"SWAP",d:"Layer by layer, one per phrase. Low end swaps as one move."},{n:"BREAK",d:"Room strips to pad and tops; next track lands on the drop."},{n:"CUT",d:"Hard cut on the next phrase line. Every layer at once."}]
            C.Panel { touchKind:"choice";touchCallback:function(){transitionRoot.transitionIndex=index;touch.inspect(modelData.n+" · design demo")};x: Theme.s5+index*(Theme.optionWidth-Theme.s2); y: Theme.modalTileY; width: Theme.optionWidth-Theme.s5; height: Theme.modalTileHeight; color: index === transitionRoot.transitionIndex ? Theme.tealDim : Theme.surface; border.color: index === transitionRoot.transitionIndex ? Theme.tealPlus : Theme.line; border.width: Theme.edge
                C.Label { x: Theme.s4; y: Theme.s3; text: modelData.n; face: Theme.displayFont; weight: Font.ExtraBold; font.pixelSize: Theme.dM }
                C.Label { x: Theme.s4; y: Theme.topHeight; width: parent.width-2*x; wrapMode: Text.WordWrap; text: modelData.d; font.pixelSize: Theme.keyLabel; color: Theme.grayN }
                C.Meter { x: Theme.s4; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s3; width: parent.width-2*x; height: Theme.rCell; value: index === 0 ? .31 : 0 }
            }
        }
        C.Label { x: Theme.s5; y: Theme.modalOrderY; text: "ORDER · ONE LAYER PER PHRASE"; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m12 }
        C.Label { anchors.right: parent.right; anchors.rightMargin: Theme.s5; y: Theme.modalOrderY; text: "STEP 3 OF 8 · DRUMS SWAPS IN 4 BARS"; font.pixelSize: Theme.m12 }
        Repeater { model: ["KICK","BASS","DRUMS","PERC","TOP","CHORDS","PAD","LEAD"]
            C.Panel { touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(modelData+" · transition order · design demo")};x: Theme.s5+index*((Theme.modalWidth-2*Theme.s5+Theme.s2)/Theme.columns); y: Theme.modalChipY; width: (Theme.modalWidth-2*Theme.s5-7*Theme.s2)/Theme.columns; height: Theme.modalChipHeight; color: index < 2 ? Theme.roleColor(modelData) : Theme.roleDim(modelData); border.color: index === 2 ? "transparent" : color
                C.DashedOutline { anchors.fill: parent; visible: index === 2; stroke: Theme.white }
                C.Label { x: Theme.s2; y: Theme.s2; text: modelData; font.pixelSize: Theme.m12; color: index < 2 ? Theme.bg : Theme.white }
                C.Label { x: Theme.s2; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s2; text: "PH "+(index+1)+(index<2 ? " ✓" : index === 2 ? " · NOW" : ""); font.pixelSize: Theme.m11; color: index < 2 ? Theme.bg : Theme.white }
            }
        }
        C.Meter { x: Theme.s5; y: Theme.modalProgressY; width: parent.width-2*x; height: Theme.rCell; value: .31 }
        C.Label { x: Theme.s5; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s4; text: "The engine runs it. Touch a fader and you take over that group."; color: Theme.gray; font.pixelSize: Theme.m14 }
    }
}
