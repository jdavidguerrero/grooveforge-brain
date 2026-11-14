import QtQuick
import "../theme"
import "../components"
Item {
    BrainTabs { y: Theme.s2; width: parent.width; height: Layout.brain.tabH }
    Repeater { model: console.standalone.engines
        Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; required property int index; required property var entry; x: Theme.s2+index*Layout.brain.enginePitch; y: Layout.brain.engineY; width: Layout.brain.engineW; height: Layout.brain.engineH; border.color: entry.bd; border.width: Theme.edge
            Label { x: Theme.s3; y: Theme.s4; width: parent.width-2*x; text: entry.n; font.pixelSize: Theme.m14 }
            Label { x: Theme.s3; y: Theme.topHeight; width: parent.width-2*x; height: Theme.s6; text: entry.d; font.pixelSize: Theme.m11; color: Theme.mon; weight: Font.Normal; wrapMode: Text.WordWrap }
            GraphPlot { x: Theme.s3; y: Theme.layerCardsY; width: parent.width-2*x; height: Theme.dXl; points: index===4 ? [30,90,10,85,20,80,25,70,30] : [50,10,20,30,40,50,60,70,80,90,50]; tint: entry.on ? Theme.tealPlus : Theme.mon }
            Label { x: Theme.s3; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s4; text: entry.on ? '● ACTIVE' : 'SELECT ENGINE'; color: entry.on ? Theme.tealPlus : Theme.mon; font.pixelSize: Theme.m11 }
        }
    }
    ParameterPage { y: Layout.brain.arcY; width: parent.width; height: Layout.brain.arcH; rows: console.standalone.brainArcs }
}
