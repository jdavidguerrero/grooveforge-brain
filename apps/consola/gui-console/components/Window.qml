import QtQuick
import "../theme"
Item {
    id: root
    default property alias body: panel.data
    Rectangle { y: Theme.contentY; width: Theme.rootWidth; height: Theme.contentHeight; color: Theme.bg; opacity: Theme.scrimOpacity; MouseArea { anchors.fill: parent } }
    Panel { id: panel; objectName: "modal"; x: Theme.modalX; y: Theme.modalY; width: Theme.modalWidth; height: Theme.modalHeight; radius: Theme.rWindow; border.color: Theme.line2 }
}
