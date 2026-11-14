import QtQuick
import "../theme"
import "../components"
ReviewWindow {
    Label { x: Theme.s5; y: Theme.s4; text: 'SCALE'; face: Theme.displayFont; font.pixelSize: Theme.dL; weight: Font.ExtraBold }
    Label { x: Theme.layerCardsHeight-Theme.s6; y: Theme.s5; text: 'A Minor · In key · 4ths'; font.pixelSize: Theme.m14 }
    Label { anchors.right: parent.right; anchors.rightMargin: Theme.s5; y: Theme.s5; text: 'SCALE BUTTON OR BACK CLOSES'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
    Label { x: Theme.s5; y: Layout.modal.bodyY; text: 'ROOT · ENC 1'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
    ChoiceRow { x: Theme.s5; y: Layout.modal.rootY; width: parent.width-2*x; columns: 12; rows: console.controller.roots }
    Label { x: Theme.s5; y: Layout.modal.scaleY-Theme.s5; text: 'SCALE · ENC 2'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
    ChoiceRow { x: Theme.s5; y: Layout.modal.scaleY; width: parent.width-2*x; columns: 4; rows: console.controller.scalesL }
    Label { x: Theme.s5; y: Layout.modal.modeY-Theme.s5; text: 'MODE · ENC 3'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
    ChoiceRow { x: Theme.s5; y: Layout.modal.modeY; width: (parent.width-3*Theme.s5)/2; columns: 2; rows: console.controller.inKey }
    Label { x: parent.width/2+Theme.s3; y: Layout.modal.modeY-Theme.s5; text: 'LAYOUT · ENC 4'; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
    ChoiceRow { x: parent.width/2+Theme.s3; y: Layout.modal.modeY; width: (parent.width-3*Theme.s5)/2; columns: 3; rows: console.controller.layouts }
}
