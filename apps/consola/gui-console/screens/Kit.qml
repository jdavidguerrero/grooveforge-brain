import QtQuick
import "../theme"
import "../components"
Item {
    Panel { x: Theme.s2; y: Theme.s2; width: Theme.summaryWidth; height: Layout.kit.summaryH
        Label { x: Theme.s4; y: Theme.s3; text: 'KIT · TUNED TO A'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
        Label { x: Theme.s4; y: Theme.dL; text: 'Selva Woods'; font.pixelSize: Theme.trackSize }
        ChoiceRow { x: Theme.settingsBodyX; y: Theme.s4; width: Theme.optionWidth; height: Theme.s6; columns: 3; tileHeight: Theme.s6; rows: console.standalone.mats }
        Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; anchors.verticalCenter: parent.verticalCenter; text: 'Selected: CONGA HI · GENERATE asks for 3 sounds'; color: Theme.grayN; font.pixelSize: Theme.m12 }
    }
    Repeater { model: console.standalone.voices
        Panel { required property int index; required property var entry; objectName: 'column'+index; x: Theme.columnX(index); y: Layout.kit.voicesY; width: Theme.columnWidth; height: Layout.kit.voicesH; border.color: entry.bd; border.width: Theme.edge; clip: true
            Rectangle { width: parent.width; height: Layout.kit.voiceHeaderH; color: entry.c
                Label { x: Theme.s2; height: parent.height; text: entry.n; color: Theme.bg; font.pixelSize: Theme.m11; weight: Font.Bold }
            }
            Label { x: Theme.s3; y: Theme.dL; text: entry.m; color: Theme.grayN; font.pixelSize: Theme.tiny }
            Label { x: Theme.s3; y: Theme.dXl; text: entry.p; face: Theme.displayFont; font.pixelSize: Theme.sectionSize; weight: Font.ExtraBold }
            Label { x: Theme.s3; y: Theme.layerCardsY; text: entry.o; color: Theme.gray; font.pixelSize: Theme.m11 }
            EnvelopeSketch { x: Theme.s3; y: Layout.kit.envelopeY; width: parent.width-2*x; height: Theme.dL; tint: entry.c; decay: entry.dec/100 }
            Label { x: Theme.s3; y: Theme.mixerSendsY; text: 'DECAY '+entry.dms; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.tiny }
            Label { x: Theme.s3; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s3; text: entry.gen ? 'ENGINE' : ''; color: Theme.purpleBright; font.pixelSize: Theme.tagSize }
        }
    }
    Label { x: Theme.s3; y: Layout.kit.alternativesTitleY; text: 'ENGINE · ALTERNATIVES FOR CONGA HI · TAP PAD TO AUDITION'; color: Theme.purpleBright; font.pixelSize: Theme.m11 }
    Repeater { model: console.standalone.alts
        Panel { required property int index; required property var entry; x: Theme.columnX(index*2); y: Layout.kit.alternativesY; width: Theme.optionWidth; height: Layout.kit.alternativesH; color: entry.bg; border.color: entry.bd; border.width: entry.bs === 'dashed' ? 0 : Theme.edge
            DashedOutline { anchors.fill: parent; visible: entry.bs === 'dashed'; stroke: entry.bd }
            Label { x: Theme.s3; y: Theme.s2; text: entry.l; face: Theme.displayFont; font.pixelSize: Theme.m28; weight: Font.ExtraBold }
            Label { anchors.right: parent.right; anchors.rightMargin: Theme.s3; y: Theme.s3; text: entry.t; color: entry.tc; font.pixelSize: Theme.tiny }
            Label { x: Theme.s3; y: Theme.topHeight; text: entry.n; font.pixelSize: Theme.m14 }
            Label { x: Theme.s3; y: Theme.layerHeader; width: parent.width-2*x; text: entry.d; font.pixelSize: Theme.m11; color: Theme.grayN }
        }
    }
}
