import QtQuick
import "../theme"
Rectangle {
    id: panelRoot
    property var touchCallback: null
    property bool reviewSelected: false
    property string touchKind: "detail"
    Loader { anchors.fill:parent;z:-1;active:panelRoot.touchCallback !== null
        sourceComponent:TouchTarget { hardwareAction:panelRoot.touchKind;callback:function(){panelRoot.touchCallback(panelRoot)} }
    }

    Rectangle { anchors.fill:parent;color:"transparent";border.width:2;border.color:Theme.tealPlus;visible:panelRoot.reviewSelected;z:10 }
    color: Theme.surface
    border.color: Theme.line
    border.width: Theme.hairline
    radius: Theme.rCard
}
