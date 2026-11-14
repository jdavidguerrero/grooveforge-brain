import QtQuick
import "../theme"
import "../components"
Item {
    id: root
    property bool offline: console.screenName === 'SessionNoLive'
    Repeater { model: console.controller.tracks
        Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; required property int index; required property var entry; objectName: 'column'+index; x: Theme.columnX(index); y: Theme.s2; width: Theme.columnWidth; height: Layout.session.headH; visible: !root.offline; clip: true
            Rectangle { width: parent.width; height: Theme.s1; color: entry.c }
            Label { x: Theme.s3; y: Theme.s2; text: entry.n; font.pixelSize: Theme.keyLabel }
            Label { x: Theme.s3; y: Theme.s2+Theme.m18; text: entry.d; font.pixelSize: Theme.tiny; color: Theme.mon }
        }
    }
    Repeater { model: console.controller.scenes
        Item { required property int index; required property var entry; property var scene: entry; y: Layout.session.sceneY+index*Layout.session.scenePitch; width: parent.width; height: Layout.session.scenePitch; visible: !root.offline
            Label { x: Theme.s3; height: Layout.session.labelH; text: scene.n+'   '+scene.sub; font.pixelSize: Theme.tiny; color: scene.lc }
            Repeater { model: scene.clips
                Panel { touchKind:"detail"; touchCallback:function(panel){panel.reviewSelected=!panel.reviewSelected;touch.inspect(console.screenTitle+" · selection · design demo")}; required property int index; required property var modelData; x: Theme.columnX(index); y: Layout.session.clipY; width: Theme.columnWidth; height: Layout.session.clipH; color: modelData.bg; border.color: modelData.bd; border.width: Theme.edge
                    DashedOutline { anchors.fill: parent; visible: modelData.bs === 'dashed'; stroke: modelData.bd }
                    Label { x: Theme.s2; y: Theme.s1; width: parent.width-Theme.s5; text: modelData.n; color: modelData.fg; font.pixelSize: Theme.m11 }
                    Label { anchors.right: parent.right; anchors.rightMargin: Theme.s1; y: Theme.s1; text: modelData.g; color: modelData.fg; font.pixelSize: Theme.tiny }
                    Waveform { x: Theme.s2; y: Layout.session.waveformY; width: parent.width-2*x; height: Layout.session.waveformH; bars: modelData.w; tint: modelData.wc }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width*modelData.pr/100; height: Theme.rCell/2; color: Theme.tealPlus; visible: modelData.play }
                }
            }
        }
    }
    Repeater { model: root.offline ? Theme.columns*6 : 0
        Item { x: Theme.columnX(index%Theme.columns); y: Theme.s2+Math.floor(index/Theme.columns)*(Theme.contentHeight-Theme.s4)/6; width: Theme.columnWidth; height: (Theme.contentHeight-Theme.s4)/6-Theme.s2
            DashedOutline { anchors.fill: parent; stroke: Theme.surface2 }
        }
    }
    Panel { x: Layout.session.noLiveX; y: Layout.session.noLiveY; width: Layout.session.noLiveW; height: Layout.session.noLiveH; visible: root.offline; border.color: Theme.line2; radius: Theme.rWindow
        Label { x: Theme.m28; y: Theme.s5; text: '●  LOOKING FOR LIVE'; color: Theme.amber; face: Theme.displayFont; font.pixelSize: Theme.dL; weight: Font.ExtraBold }
        Label { x: Theme.m28; y: Theme.mixerStripsY; width: parent.width-2*x; height: Theme.s6; text: 'Open Live on the Mac and pick GrooveForge as a control surface. The console is on 10.0.0.24.'; wrapMode: Text.WordWrap; color: Theme.grayN; font.pixelSize: Theme.m14 }
        Repeater { model: console.controller.checks
            Label { required property int index; required property var entry; x: Theme.m28; y: Theme.roomY+index*Theme.m28; text: entry.i+'   '+entry.n+'   '+entry.v; color: entry.c; font.pixelSize: Theme.keyLabel }
        }
        Label { x: Theme.m28; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s5; text: 'Searching · 0:14 · STANDALONE is one press away (MODE)'; color: Theme.gray; font.pixelSize: Theme.m12 }
    }
}
