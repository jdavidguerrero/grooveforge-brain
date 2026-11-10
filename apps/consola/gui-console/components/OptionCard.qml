import QtQuick
import "../theme"
Panel {
    id: card
    property bool compact: false
    property int rollHeight: Theme.rollHeight
    property string roleName: "BASS"
    touchKind:"choice"
    touchCallback:function(){card.optionState="cue";touch.inspect(card.title+" · design cue")}
    property string letter: "A"
    property string title: "Sub Prophet"
    property string subtitle: "A1 · pluck · 16 steps"
    property string optionState: "option"
    property int energy: 3
    property var notes: []
    property string hint: "PRESS ENC TO CUE"
    property string encoder: "ENC 1–2"
    DashedOutline { anchors.fill: parent; visible: card.optionState === "cue"; stroke: Theme.white }
    border.width: optionState === "cue" ? 0 : Theme.edge
    border.color: optionState === "cue" ? Theme.white : optionState === "room" ? Theme.teal : optionState === "loading" ? Theme.purpleDim : Theme.purple
    Label { x: Theme.m14; y: Theme.m14; text: card.letter; face: Theme.displayFont; weight: Font.ExtraBold; font.pixelSize: Theme.dL }
    Label { x: Theme.topHeight; y: Theme.s5; text: card.optionState === "cue" ? "CUE" : card.optionState === "loading" ? "GENERATING" : "ENGINE"; color: card.optionState === "cue" ? Theme.white : Theme.purpleBright; font.pixelSize: Theme.tiny }
    Label { anchors.right: parent.right; anchors.rightMargin: Theme.m14; y: Theme.s5; text: card.encoder; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
    Rectangle {
        id: roll
        x: Theme.m14; y: Theme.topHeight+Theme.s3; width: parent.width-2*x; height: card.rollHeight
        color: Theme.rollBg; radius: Theme.s1; clip: true
        Repeater { model: Theme.rollRows
            Rectangle { x: 0; y: index*roll.height/Theme.rollRows; width: roll.width; height: roll.height/Theme.rollRows; color: index%2 ? Theme.rollAlt : Theme.rollBg }
        }
        Repeater { model: 3
            Rectangle { x: (index+1)*roll.width/4; width: Theme.hairline; height: roll.height; color: index === 1 ? Theme.line2 : Theme.line }
        }
        Repeater { model: card.optionState === "loading" ? [] : card.notes
            Rectangle { x: modelData.s/Theme.rollSteps*roll.width+Theme.hairline; y: (Theme.rollRows-1-modelData.n)/Theme.rollRows*roll.height+Theme.hairline; width: modelData.l/Theme.rollSteps*roll.width-Theme.edge; height: roll.height/Theme.rollRows-Theme.edge; color: Theme.roleColor(card.roleName); radius: Theme.edge }
        }
        Label { anchors.centerIn: parent; visible: card.optionState === "loading"; text: "GENERATING · 2/3"; color: Theme.purpleBright; font.pixelSize: Theme.m11 }
    }
    Label { x: Theme.m14; y: card.compact ? Theme.topHeight+Theme.s3+card.rollHeight+Theme.s3 : Theme.optionNameY; width: parent.width-2*x; text: card.title; font.pixelSize: Theme.m18; color: card.optionState === "loading" ? Theme.mon : Theme.white }
    Label { x: Theme.m14; y: (card.compact ? Theme.topHeight+Theme.s3+card.rollHeight+Theme.s3 : Theme.optionNameY)+Theme.s5; width: parent.width-2*x; text: card.subtitle; font.pixelSize: Theme.m12; color: Theme.gray }
    Row { x: Theme.m14; y: card.compact ? card.height-Theme.dXl-Theme.s5 : Theme.optionEnergyY; spacing: Theme.s2
        Label { text: "ENERGY"; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.tiny }
        Repeater { model: 5; Rectangle { width: Theme.s5; height: Theme.s2; color: index < card.energy && card.optionState !== "loading" ? Theme.white : Theme.line2; anchors.verticalCenter: parent.verticalCenter } }
        Label { text: card.optionState === "loading" ? "–" : card.energy; font.pixelSize: Theme.m14 }
    }
    Rectangle { x: Theme.m14; y: (card.compact ? card.height-Theme.dL : Theme.optionHintY)-Theme.s2; width: parent.width-2*x; height: Theme.hairline; color: Theme.line }
    Label { x: Theme.m14; y: card.compact ? card.height-Theme.dL : Theme.optionHintY; width: parent.width-2*x; text: card.hint; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.m11 }
}
