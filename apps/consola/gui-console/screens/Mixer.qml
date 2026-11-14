import QtQuick
import "../theme"
import "../components"
Item {
    Repeater { model: console.groups
        Panel { touchKind:"parameter"; touchCallback:function(){console.selectEncoder(index);touch.inspect(title+" · use physical fader")}; required property int index; required property string title; required property real level; required property real meter; required property var members
            x: Theme.s2+index*(Theme.mixerGroupWidth+Theme.s2); y: Theme.s2; width: Theme.mixerGroupWidth; height: Theme.mixerGroupHeight
            Label { x: Theme.s3; y: Theme.s2; text: title; font.pixelSize: Theme.m12 }
            Label { anchors.right: parent.right; anchors.rightMargin: Theme.s3; y: Theme.s2; text: "FADER "+Math.round(level*100); font.pixelSize: Theme.m11; color: Theme.grayN }
            Row { x: Theme.s3; y: Theme.s5+Theme.s1; spacing: Theme.edge; Repeater { model: members; Rectangle { width: Theme.s2; height: Theme.s2; color: Theme.roleColor(modelData) } } }
            Meter { x: Theme.s3; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s2; width: parent.width-2*x; height: Theme.s2; value: parent.meter }
        }
    }
    Panel { x: Theme.mixerMasterX; y: Theme.s2; width: Theme.mixerMasterWidth; height: Theme.mixerGroupHeight
        Label { x: Theme.s3; y: Theme.s2; text: "MASTER  ROOM 78  CUE 60"; font.pixelSize: Theme.m11 }
        Label { x: Theme.s3; y: Theme.s5; text: "MIX 40/60"; font.pixelSize: Theme.m11; color: Theme.grayN }
        Meter { x: Theme.s3; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s2; width: parent.width-2*x; height: Theme.rCell; value: .84 }
    }
    Repeater { model: console.channels
        MixerStrip { required property int index; required roleName; required level; required meter; required peak; required eq; required filterValue; required echo; required verb; required cue; required selected
            objectName: "strip"+index; x: Theme.columnX(index); y: Theme.mixerStripsY; width: Theme.columnWidth; height: Theme.mixerStripHeight
        }
    }
}
