import QtQuick
import "../theme"
Rectangle {
    id: cell
    property string roleName: "BASS"
    property string title: "Sub Model D"
    property string subtitle: "A1 · round"
    property string cellState: "playing"
    property var steps: []
    property real level: 0.7
    property bool compact: false
    property string encoding: console.encoding
    property color roleColor: Theme.roleColor(roleName)
    property color dim: Theme.roleDim(roleName)
    property bool sounds: cellState === "playing" || cellState === "queued"
    property bool subdued: cellState === "empty" || cellState === "muted" || cellState === "loading"
    property bool dashed: cellState === "cue" || cellState === "empty"
    property color ink: subdued ? Theme.mon : sounds && encoding === "fill" ? Theme.bg : Theme.white
    property color edgeColor: cellState === "queued" || cellState === "cue" ? Theme.white : cellState === "option" ? Theme.purpleBright : cellState === "loading" ? Theme.purpleDim : cellState === "empty" ? Theme.line : encoding === "fill" ? (sounds ? roleColor : dim) : sounds ? Theme.teal : Theme.line
    radius: Theme.rCell
    color: cellState === "empty" ? "transparent" : cellState === "muted" ? Theme.bg : cellState === "loading" ? Theme.surface : encoding === "fill" ? (sounds ? roleColor : dim) : sounds ? Theme.tealDim : cellState === "option" ? Theme.purpleDim : Theme.surface2
    border.width: dashed ? 0 : Theme.edge
    border.color: edgeColor
    Canvas {
        id: dashedEdge
        anchors.fill: parent; visible: cell.dashed
        onPaint: { let c=getContext("2d"); c.reset(); c.strokeStyle=cell.edgeColor; c.lineWidth=Theme.edge; c.setLineDash([Theme.s1,Theme.s1]); c.strokeRect(Theme.hairline,Theme.hairline,width-Theme.edge,height-Theme.edge) }
        Connections { target: cell; function onEdgeColorChanged() { dashedEdge.requestPaint() } }
    }
    Rectangle { visible: cell.encoding === "stripe"; x: Theme.edge; y: Theme.edge; width: parent.width-2*x; height: Theme.rCell; color: cell.subdued ? cell.dim : cell.roleColor; radius: Theme.edge }
    Label { x: Theme.cellPad; y: Theme.s2; width: parent.width-2*x; text: cell.roleName; color: cell.ink === Theme.white ? cell.roleColor : cell.ink; font.pixelSize: Theme.m12 }
    Label {
        anchors.right: parent.right; anchors.rightMargin: Theme.s2; y: Theme.s2
        visible: cell.cellState !== "playing" && cell.cellState !== "idle" && cell.cellState !== "empty"
        text: ({queued:"NEXT",cue:"CUE",option:"ENGINE",muted:"MUTED",loading:"GENERATING"})[cell.cellState] || ""
        font.pixelSize: Theme.tagSize; color: cell.cellState === "option" || cell.cellState === "loading" ? Theme.aiText : cell.ink
    }
    Column {
        x: Theme.cellPad; y: cell.compact ? Theme.s5+Theme.s1 : Theme.s5+Theme.s2
        width: parent.width-2*x; spacing: Theme.hairline
        Label { width: parent.width; text: cell.title; color: cell.ink; font.pixelSize: Theme.m14; height: Theme.line14 }
        Label { width: parent.width; visible: !cell.compact; text: cell.subtitle; color: cell.sounds && cell.encoding === "fill" ? Theme.bg : cell.subdued ? Theme.mon : Theme.gray; font.pixelSize: Theme.m11; height: Theme.line11 }
    }
    Row {
        x: Theme.cellPad; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s4; spacing: Theme.edge
        visible: !cell.compact && cell.steps.length > 0 && !cell.subdued
        Repeater { model: cell.steps
            Rectangle { width: (cell.width-2*Theme.cellPad-(Theme.rollSteps-1)*Theme.edge)/Theme.rollSteps; height: Theme.stepHeight; radius: Theme.hairline; color: modelData ? (cell.sounds && cell.encoding === "fill" ? Theme.bg : cell.roleColor) : cell.ink; opacity: modelData ? 1 : 0.15 }
        }
    }
    Rectangle { x: Theme.cellPad; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s2; width: parent.width-2*x; height: Theme.levelHeight; radius: Theme.edge; color: Theme.bg; visible: cell.cellState === "playing" && !cell.compact
        Rectangle { width: parent.width*cell.level; height: parent.height; color: Theme.tealPlus; radius: Theme.edge }
    }
}
