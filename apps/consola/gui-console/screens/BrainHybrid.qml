import QtQuick
import "../theme"
import "../components"
Item {
    BrainTabs { y: Theme.s2; width: parent.width; height: Layout.brain.tabH; hybrid: true }
    Repeater { model: console.standalone.hybridEngines
        Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; required property int index; required property var entry; x: index ? Layout.brain.hybridRightX : Theme.s2; y: Layout.brain.engineY; width: Layout.brain.hybridSideW; height: Layout.brain.hybridH; border.color: Theme.hyb
            Label { x: Theme.s4; y: Theme.s3; text: entry.l+'  '+entry.n; face: Theme.displayFont; font.pixelSize: Theme.dL; weight: Font.ExtraBold; color: Theme.hybHi }
            Label { x: Theme.s4; y: Theme.dXl; text: entry.d; color: Theme.grayN; font.pixelSize: Theme.m12 }
            GraphPlot { x: Theme.s4; y: Theme.layerCardsY; width: parent.width-2*x; height: Theme.dXl; tint: Theme.hybHi; points: index ? [40,5,90,10,80,20,70,30,60,40] : [90,70,50,30,10,90,70,50,30,10] }
            Label { x: Theme.s4; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s4; text: 'OCT 0 · LEVEL '+(index ? '−5 dB' : '−2 dB'); color: Theme.grayN; font.pixelSize: Theme.m12 }
        }
    }
    Panel { x: Layout.brain.hybridMidX; y: Layout.brain.engineY; width: Layout.brain.hybridMidW; height: Layout.brain.hybridH; color: Theme.hybDim; border.color: Theme.hyb
        ChoiceRow { x: Theme.s4; y: Theme.s4; width: parent.width-2*x; height: Theme.s6; tileHeight: Theme.s6; columns: 3; rows: console.standalone.hybModes }
        Label { x: Theme.s4; y: Theme.mixerStripsY; text: 'SPLIT POINT'; color: Theme.hybHi; font.pixelSize: Theme.m11 }
        Label { x: Theme.s4; y: Theme.layerCardsY; text: 'C3'; face: Theme.displayFont; font.pixelSize: Theme.dXl; color: Theme.hybHi; weight: Font.ExtraBold }
        Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; y: Theme.layerCardsY; text: 'A 62  /  B 38'; face: Theme.displayFont; weight: Font.ExtraBold; font.pixelSize: Theme.dL; color: Theme.hybHi }
        Meter { x: Theme.s4; y: parent.height-Theme.s6; width: parent.width-2*x; height: Theme.s3; value: .62; tint: Theme.hybHi }
    }
    ParameterPage { y: Layout.brain.hybridArcY; width: parent.width; height: Layout.brain.arcH; rows: console.standalone.hybArcs; tint: Theme.hyb; arcSize: Layout.brain.hybridArcSize }
}
