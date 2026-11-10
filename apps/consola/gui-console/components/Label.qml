import QtQuick
import "../theme"
Text {
    property string face: console.screen <= 2 || console.screen === 4 ? Theme.osMonoFont : Theme.monoFont
    property int weight: face === Theme.displayFont ? Font.ExtraBold : Font.DemiBold
    color: Theme.white
    font.family: face
    font.pixelSize: Theme.m14
    font.weight: weight
    font.variableAxes: face === Theme.monoFont ? ({}) : face === Theme.archivoFont ? ({"wght": weight, "wdth": 125}) : ({"wght": weight})
    elide: Text.ElideRight
    verticalAlignment: Text.AlignVCenter
}
