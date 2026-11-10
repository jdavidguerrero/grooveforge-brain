import QtQuick
import "../theme"
import "../components"
Item {
    Label { x: Theme.s3; y: Theme.s2; text: "DRUMS + PERC · 1 BAR · 16 STEPS"; color: Theme.grayN; font.pixelSize: Theme.m12 }
    Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; y: Theme.s2; text: "■ YOU     ■ ENGINE (VIOLET)     PADS 8×4"; color: Theme.purpleBright; font.pixelSize: Theme.m11 }
    Repeater { model: console.standalone.ruler
        Label { required property int index; required property var entry; x: Theme.s2+Layout.steps.gridX+index*Layout.steps.stepPitch; y: Layout.steps.rulerY; width: Layout.steps.stepW; text: entry.n; color: entry.fg; horizontalAlignment: Text.AlignHCenter; font.pixelSize: Theme.m11 }
    }
    Repeater { model: console.standalone.stepLanes
        StepLane { x: Theme.s2; y: Layout.steps.laneY+index*Layout.steps.pitch; width: Theme.summaryWidth; height: Layout.steps.laneH; entry: model.entry }
    }
    Rectangle { x: Theme.s2+Layout.steps.gridX+Math.min(8, console.padStep-16)*Layout.steps.stepPitch; y: Layout.steps.laneY+Math.min(4, console.padNote)*Layout.steps.pitch; width: Layout.steps.windowW; height: Layout.steps.windowH; color: "transparent"; border.color: Theme.white; border.width: Theme.edge; radius: Theme.rCell
        Label { x: Theme.s3; y: -Theme.m14; text: "PADS 8×4"; font.pixelSize: Theme.tagSize }
    }
}
