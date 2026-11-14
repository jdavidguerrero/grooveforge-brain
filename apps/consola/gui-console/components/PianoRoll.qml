import QtQuick
import "../theme"
Item {
    id: root
    property bool clipView: false
    property var keys: clipView ? console.controller.keys : console.standalone.keys
    property var notes: clipView ? console.controller.clipNotes : console.standalone.rollNotes
    property var lines: clipView ? console.controller.clipLines : console.standalone.rollLines
    property color noteColor: clipView ? "#6E9BE8" : Theme.roleColor("BASS")
    Column { width: Layout.roll.keyW; height: parent.height; spacing: Theme.hairline
        Repeater { model: root.keys
            Rectangle { required property int index; required property var entry; width: Layout.roll.keyW; height: (root.height-(keyRepeater.count-1)*Theme.hairline)/keyRepeater.count; color: entry.bg
                Label { x: Theme.rCell; anchors.verticalCenter: parent.verticalCenter; text: entry.n; color: entry.fg; font.pixelSize: root.clipView ? Theme.m11 : Theme.tiny }
            }
            id: keyRepeater
        }
    }
    Flickable { id: viewport; objectName: "rollViewport"; x: Layout.roll.keyW+Layout.roll.gap; width: parent.width-x; height: parent.height; clip: true
        contentWidth: width*(root.clipView ? 1 : console.rollZoom); contentHeight: height; boundsBehavior: Flickable.StopAtBounds
        Rectangle { width: viewport.contentWidth; height: viewport.height; color: Theme.rollBg
            Repeater { model: root.keys
                Rectangle { required property int index; required property var entry; y: entry.t*parent.height/100; width: parent.width; height: entry.h*parent.height/100; color: entry.row
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Theme.hairline; color: Theme.bg }
                }
            }
            Repeater { model: root.lines
                Rectangle { required property int index; required property var entry; x: entry.x*parent.width/100; height: parent.height; width: Theme.hairline; color: entry.c }
            }
            Rectangle { objectName: "padWindow"; visible: !root.clipView; x: console.padStep/64*parent.width; y: (Layout.roll.padY-console.padNote/25)*parent.height; width: Layout.roll.padW*parent.width; height: Layout.roll.padH*parent.height; color: Qt.rgba(1,1,1,.05); border.color: Theme.white; border.width: Theme.edge; radius: Theme.edge
                Label { y: -Theme.s4; x: Theme.rCell; text: "PADS 8 STEPS × 4 NOTES"; color: Theme.white; font.pixelSize: Theme.tagSize }
            }
            Repeater { model: root.notes
                Rectangle { required property int index; required property var entry; objectName: "rollNote"+index
                    x: entry.l*parent.width/100+Theme.hairline; width: Math.max(Theme.edge, entry.w*parent.width/100-Theme.edge)
                    y: entry.t*parent.height/100+Theme.edge; height: entry.h*parent.height/100-Theme.s1
                    color: entry.c || root.noteColor; opacity: entry.o === undefined ? 1 : entry.o; radius: Theme.edge
                    border.color: entry.sh !== 'none' ? Theme.white : color; border.width: entry.sh !== 'none' ? Theme.edge : 0
                }
            }
            Rectangle { x: parent.width*(root.clipView ? Layout.roll.clipHead : Layout.roll.playhead); width: Theme.edge; height: parent.height; color: Theme.tealPlus }
        }
        TouchTarget { hardwareAction:"navigation"; callback:function(){ touch.inspect("Piano roll · drag to navigate · pinch to zoom") } }
        PinchHandler { id: pinch; target: null; enabled: !root.clipView; onActiveScaleChanged: console.zoomRoll(activeScale) }
    }
}
