import QtQuick
import "../theme"
import "../components"
Item {
    id: live
    Panel {touchKind:"navigation";touchCallback:function(){console.navigate("Transition")};x: Theme.summaryX; y: Theme.summaryY; width: Theme.summaryWidth; height: Theme.infoHeight
        Canvas { x: Theme.ringX; anchors.verticalCenter: parent.verticalCenter; width: Theme.ringSize; height: width
            onPaint: { let c=getContext('2d'); c.reset(); c.lineWidth=Theme.ringStroke; for(let i=0;i<Theme.columns;i++){ c.strokeStyle=i<console.phrase-1?Theme.teal:i===console.phrase-1?Theme.white:Theme.line; c.beginPath();c.arc(width/2,height/2,(width-Theme.ringStroke)/2,(i*Theme.ringSegment+Theme.ringStart)*Math.PI/180,(i*Theme.ringSegment+Theme.ringEnd)*Math.PI/180);c.stroke() } }
            Label { anchors.centerIn: parent; text: console.phrase; face: Theme.displayFont; weight: Font.ExtraBold; font.pixelSize: Theme.ringCount }
        }
        Column { x: Theme.phraseX; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.edge
            Label { text: "BAR 5 OF 8"; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
            Label { text: console.section; face: Theme.displayFont; weight: Font.ExtraBold; font.pixelSize: Theme.sectionSize }
            Label { text: "NEXT PHRASE IN 4 BARS"; color: Theme.gray; font.pixelSize: Theme.m12 }
        }
        Column { x: Theme.roomTitleX; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.s1
            Label { text: "● ROOM"; color: Theme.tealPlus; font.pixelSize: Theme.m11 }
            Label { text: console.roomTitle; font.pixelSize: Theme.trackSize }
            Label { text: "LIBRARY · A MIN · 124"; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m12 }
        }
        Label { x: Theme.nextTitleX-Theme.dL; anchors.verticalCenter: parent.verticalCenter; text: "→"; color: Theme.off; font.pixelSize: Theme.dM }
        Column { x: Theme.nextTitleX; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.s1
            Label { text: "NEXT"; font.pixelSize: Theme.m11 }
            Label { text: console.nextTitle; font.pixelSize: Theme.trackSize }
            Label { text: "GENERATED   A MIN ✓   124 ✓"; color: Theme.tealPlus; font.pixelSize: Theme.m12 }
        }
        Column { x: Theme.transitionSummaryX; anchors.verticalCenter: parent.verticalCenter; spacing: Theme.s1
            Label { text: console.liveState === "offline" ? "ENGINE OFFLINE" : "TRANSITION"; font.pixelSize: Theme.m11; color: console.liveState === "offline" ? Theme.amber : Theme.mon }
            Label { text: console.liveState === "offline" ? "RETRY · 4 S" : "SWAP · 8 PH"; face: Theme.displayFont; weight: Font.ExtraBold; font.pixelSize: Theme.m28; color: console.liveState === "offline" ? Theme.amber : Theme.white }
            Label { text: "READY · TRANSITION"; font.pixelSize: Theme.m11; color: Theme.grayN }
        }
    }
    Repeater { model: [
        {y:Theme.roomY,t:"ROOM",s:"SELVA NOCTURNA · WHAT THE ROOM HEARS",c:Theme.tealPlus},
        {y:Theme.nextY,t:"NEXT",s:"KALIMBA DUSK · KICK ENTERS NEXT PHRASE · BASS IN CUE",c:Theme.white},
        {y:Theme.optionAY,t:"ENGINE OPTIONS",s:console.liveState === "offline" ? "PAUSED · ENGINE OFFLINE" : "PATTERN + SOUND + ENERGY · PRESS ENC TO CUE",c:Theme.purpleBright}]
        Row { x: Theme.s3; y: modelData.y-Theme.rowLabelHeight-Theme.rowLabelGap; spacing: Theme.s3
            Label { text: modelData.t; color: modelData.c; font.pixelSize: Theme.m11 }
            Label { text: modelData.s; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        }
    }
    Repeater { model: console.cells
        GridCell {
            required property int row
            required property int channel
            required roleName
            required title
            required subtitle
            required cellState
            required steps
            required level
            objectName: "cell"+row+"_"+channel
            x: Theme.columnX(channel); y: [Theme.roomY,Theme.nextY,Theme.optionAY,Theme.optionBY][row]
            width: Theme.columnWidth; height: [Theme.roomHeight,Theme.nextHeight,Theme.optionHeight,Theme.optionHeight][row]
            compact: row>=2
            TouchTarget { hardwareAction: "navigation"; anchors.fill: parent; callback: function() { console.setScreen(6) }}
        }
    }
    Image { y: Theme.handsReadoutY; width: Theme.handHaloWidth; height: Theme.handHaloHeight; source: "../assets/halo.png" }
    Rectangle { width: Theme.s1; height: Theme.handHaloHeight; y: Theme.handsReadoutY; opacity: 0.6; gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: .5; color: Theme.tealPlus } GradientStop { position: 1; color: "transparent" } } }
}
