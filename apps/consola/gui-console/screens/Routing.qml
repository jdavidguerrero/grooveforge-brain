import QtQuick
import "../theme"
import "../components"
Item {
    Repeater { model: console.standalone.routeHead
        Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; required property int index; required property var entry; objectName: 'column'+index; x: Theme.columnX(index); y: Theme.s2; width: Theme.columnWidth; height: Layout.route.headH; color: entry.c
            Label { x: Theme.s3; height: parent.height; text: entry.k; color: Theme.bg; font.pixelSize: Theme.m12; weight: Font.Bold }
        }
    }
    Repeater { model: console.standalone.routeRows
        Item { required property int index; required property var entry; property var route: entry; y: Layout.route.rowsY+index*Layout.route.rowPitch; width: parent.width; height: Layout.route.rowPitch
            Label { x: Theme.s3; text: route.n; color: Theme.grayN; font.pixelSize: Theme.m11 }
            Repeater { model: route.cells
                Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; required property int index; required property var modelData; x: Theme.columnX(index); y: Layout.route.cellY; width: Theme.columnWidth; height: Layout.route.cellH; color: modelData.bg; border.color: modelData.bd
                    Label { x: Theme.s3; y: Theme.s2; text: modelData.v; color: modelData.fg; font.pixelSize: Theme.m14 }
                    Label { x: Theme.s3; y: Theme.s2+Theme.m20; text: modelData.s; color: Theme.mon; weight: Font.Normal; font.pixelSize: Theme.tiny }
                }
            }
        }
    }
    Repeater { model: console.standalone.routeCards
        Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; required property int index; required property var entry; x: Theme.s2+index*Layout.route.cardPitch; y: Layout.route.cardsY; width: Layout.route.cardW; height: Layout.route.cardsH
            Label { x: Theme.s4; y: Theme.s3; text: entry.n; face: Theme.displayFont; font.pixelSize: Theme.m28; weight: Font.ExtraBold }
            Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; y: Theme.s4; text: '● '+entry.s; color: entry.sc; font.pixelSize: Theme.m11 }
            Repeater { model: entry.rows
                Item { required property int index; required property var modelData; x: Theme.s4; y: Theme.topHeight+index*Theme.dL; width: parent.width-2*x; height: Theme.dL
                    Rectangle { width: parent.width; height: Theme.hairline; color: Theme.line }
                    Label { y: Theme.s2; text: modelData.a; color: Theme.grayN; font.pixelSize: Theme.m12 }
                    Label { y: Theme.s2; anchors.right: parent.right; text: modelData.b; font.pixelSize: Theme.m12 }
                }
            }
        }
    }
}
