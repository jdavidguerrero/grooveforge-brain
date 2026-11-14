import QtQuick
import "../theme"
Item {
    id: root
    property bool repeatWindow: false
    default property alias body: panel.data
    Repeater { model: console.screenName === 'Tempo' ? console.controller.ghost48c : console.controller.ghost32
        Rectangle { required property int index; required property var entry; x: Theme.columnX(index%Theme.columns); y: Theme.s2+Math.floor(index/Theme.columns)*(root.height-Theme.s4)/(console.screenName === 'Tempo' ? 6 : 4)
            width: Theme.columnWidth; height: (root.height-Theme.s4)/(console.screenName === 'Tempo' ? 6 : 4)-Theme.s2; radius: Theme.rCard; color: entry; opacity: .25 }
    }
    Panel { id: panel; objectName: 'reviewModal'; x: root.repeatWindow ? Layout.modal.repeatX : Layout.modal.x; y: root.repeatWindow ? Layout.modal.repeatY : Layout.modal.y
        width: root.repeatWindow ? Layout.modal.repeatW : Layout.modal.w; height: root.repeatWindow ? Layout.modal.repeatH : Layout.modal.h; border.color: Theme.line2; radius: Theme.rWindow }
}
