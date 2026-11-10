import QtQuick
import "../theme"
Panel {
    id: strip
    property string roleName: "BASS"
    property real level: 0.77
    property real meter: 0.74
    property real peak: 0
    property var eq: [0,0,0]
    property real filterValue: 0
    property real echo: 0
    property real verb: 0
    property bool cue: false
    property bool selected: false
    property color tint: Theme.roleColor(roleName)
    property string filterLabel: Math.abs(filterValue)<0.03 ? "OPEN" : filterValue < 0 ? "LP "+(20*Math.pow(10,filterValue*2.5)).toFixed(1)+"k" : "HP "+Math.round(20*Math.pow(10,filterValue*3))
    border.color: selected ? tint : Theme.line; border.width: selected ? Theme.edge : Theme.hairline
    Rectangle { x: Theme.hairline; y: Theme.hairline; width: parent.width-Theme.edge; height: Theme.mixerHeader; color: strip.level > 0 ? strip.tint : Theme.roleDim(strip.roleName)
        Label { x: Theme.s2; anchors.verticalCenter: parent.verticalCenter; text: strip.roleName; color: strip.level > 0 ? Theme.bg : Theme.white; font.pixelSize: Theme.m12 }
        Label { anchors.right: parent.right; anchors.rightMargin: Theme.s2; anchors.verticalCenter: parent.verticalCenter; text: strip.cue ? "CUE" : strip.level === 0 ? "MUTE" : ""; color: Theme.bg; font.pixelSize: Theme.tagSize }
    }
    Column { x: Theme.s3; y: Theme.mixerEqY; width: parent.width-2*x; spacing: Theme.s1
        Repeater { model: 3
            Row { width: parent.width; spacing: Theme.rCell
                Label { width: Theme.s5; text: ["HI","MID","LO"][index]; font.pixelSize: Theme.tiny; color: Theme.mon }
                Rectangle { width: strip.width-2*Theme.s3-Theme.s5-Theme.rCell; height: Theme.m14; color: Theme.rollBg; radius: Theme.edge
                    Rectangle { x: parent.width/2; width: Theme.hairline; height: parent.height; color: Theme.line2 }
                    Rectangle { x: parent.width*(strip.eq[index]<0 ? (1+strip.eq[index])/2 : .5); width: parent.width*Math.abs(strip.eq[index])/2; height: Theme.rCell; y: Theme.s1; color: strip.tint; visible: strip.eq[index]>-1 }
                    Label { anchors.centerIn: parent; text: "KILL"; visible: strip.eq[index]<=-1; font.pixelSize: Theme.tagSize; color: Theme.grayN }
                }
            }
        }
    }
    Panel { x: Theme.s2; y: Theme.mixerFilterY; width: parent.width-2*x; height: Theme.topHeight+Theme.s3; color: Theme.rollBg; border.color: Theme.white; radius: Theme.s1
        Label { x: Theme.rCell; y: Theme.s1; text: "FILTER"; font.pixelSize: Theme.tiny; color: Theme.mon }
        Label { anchors.right: parent.right; anchors.rightMargin: Theme.rCell; y: Theme.s1; text: strip.filterLabel; font.pixelSize: Theme.tiny }
        Canvas { x: Theme.rCell; y: Theme.s5; width: parent.width-2*x; height: Theme.s5+Theme.s2
            onPaint: { let c=getContext('2d'); c.reset(); c.strokeStyle=strip.tint; c.lineWidth=Theme.edge; c.beginPath(); if(Math.abs(strip.filterValue)<.03) {c.moveTo(0,Theme.s2);c.lineTo(width,Theme.s2)} else if(strip.filterValue<0){c.moveTo(0,Theme.s2);c.lineTo(width*(1+strip.filterValue)-Theme.s3,Theme.s2);c.quadraticCurveTo(width*(1+strip.filterValue),0,width,height)} else {c.moveTo(0,height);c.quadraticCurveTo(width*strip.filterValue,0,width*strip.filterValue+Theme.s3,Theme.s2);c.lineTo(width,Theme.s2)} c.stroke() }
        }
    }
    Column { x: Theme.s3; y: Theme.mixerSendsY; width: parent.width-2*x; spacing: Theme.s2
        Repeater { model: [{n:"ECHO",v:strip.echo},{n:"VERB",v:strip.verb}]
            Row { spacing: Theme.s2
                Label { width: Theme.dL; text: modelData.n; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.tiny }
                Rectangle { width: strip.width-2*Theme.s3-Theme.dL-Theme.s2; height: Theme.rCell; color: Theme.rollBg; anchors.verticalCenter: parent.verticalCenter; Rectangle { width: parent.width*modelData.v; height: parent.height; color: Theme.grayN } }
            }
        }
    }
    Panel { x: Theme.s2; y: Theme.mixerCueY; width: parent.width-2*x; height: Theme.dM; color: strip.cue ? Theme.white : "transparent"; radius: Theme.s1
        Label { anchors.centerIn: parent; text: "CUE"; color: strip.cue ? Theme.bg : Theme.grayN; font.pixelSize: Theme.m11 }
    }
    Item { x: Theme.topHeight; y: Theme.mixerFaderY; width: Theme.dL+Theme.s4; height: Theme.mixerFaderHeight
        Rectangle { x: Theme.s3; height: parent.height; width: Theme.s1; color: Theme.line; radius: Theme.edge }
        Rectangle { x: 0; y: (1-strip.level)*(parent.height-Theme.s3); width: Theme.m28; height: Theme.s3; radius: Theme.edge; color: Theme.white; Rectangle { x: Theme.s1; y: Theme.rCell; width: parent.width-2*x; height: Theme.edge; color: Theme.bg } }
        Repeater { model: 2
            Rectangle { x: Theme.dL+index*(Theme.rCell+Theme.edge); width: Theme.rCell; height: parent.height; color: Theme.rollBg
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: parent.height*strip.meter*(index ? .94 : 1); gradient: Gradient { GradientStop {position:0;color:Theme.tealPlus} GradientStop {position:1;color:Theme.tealDim} } }
            }
        }
        Rectangle { visible: strip.peak>=1; x: Theme.dL; y: -Theme.s2; width: Theme.tagSize; height: Theme.s1; color: Theme.red }
    }
    Label { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s2; text: strip.level<=0 ? "−∞" : (20*Math.log(strip.level/.8)/Math.LN10).toFixed(1)+" dB"; font.pixelSize: Theme.m14 }
}
