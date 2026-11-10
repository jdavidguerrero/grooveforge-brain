import QtQuick
import "../theme"
Canvas {
    id: outline
    property color stroke: Theme.white
    onStrokeChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onVisibleChanged: requestPaint()
    onPaint: {
        let c = getContext("2d")
        c.reset()
        c.strokeStyle = stroke
        c.lineWidth = Theme.edge
        c.setLineDash([Theme.s1, Theme.s1])
        c.strokeRect(Theme.hairline, Theme.hairline, width-Theme.edge, height-Theme.edge)
    }
}
