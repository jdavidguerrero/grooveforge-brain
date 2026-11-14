import QtQuick
import "../theme"
Item {
    id: root
    property var entry
    Panel {touchKind:"navigation";touchCallback:function(){touch.inspect(root.entry.k+" · lane selected")};width: Layout.arrange.labelW; height: parent.height; color: root.entry.d
        Label { x: Theme.s3; y: Theme.s1; text: root.entry.k; color: root.entry.c; font.pixelSize: Theme.m12 }
        Label { x: Theme.s3; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s1; text: root.entry.lv; font.pixelSize: Theme.m11 }
    }
    Panel {touchKind:"navigation";touchCallback:function(){console.navigate("Layer")};x: Layout.arrange.riverX; width: parent.width-x; height: parent.height; color: Theme.rollAlt; clip: true
        Rectangle { visible: root.entry.hasNext; x: root.entry.nx*parent.width/100; width: parent.width-x; height: parent.height; color: Theme.surface2
            DashedOutline { width: Theme.edge; height: parent.height; stroke: Theme.white }
        }
        Waveform { anchors.fill: parent; anchors.margins: Theme.s1; bars: root.entry.bars }
        Label { visible: root.entry.hasNext; x: root.entry.nx*parent.width/100+Theme.s2; y: Theme.edge; text: "KALIMBA DUSK ▸"; font.pixelSize: Theme.tagSize }
    }
}
