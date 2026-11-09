import QtQuick
import "../theme"
import "../components"
Item {
    Repeater { model: console.controller.steps14
        Panel { required property int index; required property var entry; x: Theme.s2+(index%7)*Layout.motor.stepPitch; y: Theme.s2+Math.floor(index/7)*Layout.motor.stepRowPitch; width: Layout.motor.stepW; height: Layout.motor.stepH; color: entry.bg; border.color: entry.bd.indexOf('#7A6FE0')>=0 ? Theme.purpleBright : Theme.line
            Label { x: Theme.s3; y: Theme.s1; text: entry.i; color: entry.ic; font.pixelSize: Theme.tiny }
            Label { x: Theme.s3; y: Theme.s5; text: entry.n; color: entry.fg; font.pixelSize: Theme.keyLabel }
        }
    }
    Label { x: Theme.s3; y: Layout.motor.titleY; text: 'STEP 4 · BASS'; face: Theme.displayFont; font.pixelSize: Theme.dL; weight: Font.ExtraBold }
    Label { x: Theme.settingsBodyX; y: Layout.motor.titleY+Theme.s3; text: 'Project: Selva Nocturna · A min · 124 · writes to track 3 in Live'; color: Theme.grayN; font.pixelSize: Theme.m12 }
    Repeater { model: console.controller.engCards
        OptionCard { x: Theme.columnX(index*2); y: Layout.motor.cardsY; width: Theme.optionWidth; height: Layout.motor.cardsH; compact: true; rollHeight: Layout.motor.rollH; roleName: Theme.liveBass; letter: model.entry.letter; title: model.entry.sound || 'Waiting for engine'; subtitle: model.entry.sub || ''; optionState: model.entry.state; energy: model.entry.energy || 1; notes: model.entry.notes || []; hint: model.entry.hint; encoder: 'ENC '+(index+1)+' CUE' }
    }
}
