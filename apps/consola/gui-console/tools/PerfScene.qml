import QtQuick
import "../screens"
import "../theme"
Item {
    // Full-resolution content textures keep both real screens rendering concurrently.
    Item {
        width: 1280; height: 568; scale: .5; transformOrigin: Item.TopLeft
        y: 100; layer.enabled: true; layer.textureSize: Qt.size(1280, 568)
        LiveSession { anchors.fill: parent }
        Rectangle { id: playhead; y: 142; width: 3; height: 410; color: Theme.white
            XAnimator on x { from: 8; to: 1270; duration: 2000; loops: Animation.Infinite; running: true }
        }
    }
    Item {
        x: 640; y: 100; width: 1280; height: 568; scale: .5; transformOrigin: Item.TopLeft
        layer.enabled: true; layer.textureSize: Qt.size(1280, 568)
        Mixer { anchors.fill: parent }
        Rectangle { width: 1280; height: 4; y: 564; color: Theme.tealPlus
            SequentialAnimation on opacity {
                loops: Animation.Infinite; running: true
                OpacityAnimator { from: .2; to: 1; duration: 500 }
                OpacityAnimator { from: 1; to: .2; duration: 500 }
            }
        }
    }
}
