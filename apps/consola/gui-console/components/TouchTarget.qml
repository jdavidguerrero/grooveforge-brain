import QtQuick
MouseArea {
    id: root
    anchors.fill: parent
    property bool clickable: enabled
    property string hardwareAction: "navigation"
    property var callback: null
    property bool hardwareFocused: false
    property bool inScrollArea: {
        let item = parent
        while (item) {
            if (item.contentY !== undefined && item.contentHeight !== undefined) return true
            item = item.parent
        }
        return false
    }
    property bool activateOnPress: !inScrollArea && ["soft-key", "navigation", "choice", "text"].indexOf(hardwareAction) >= 0
    objectName: "touchTarget"
    preventStealing: false
    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    function activate() { if (enabled && typeof callback === 'function') callback() }
    onPressed: if (activateOnPress) activate()
    onClicked: if (!activateOnPress) activate()
    Component.onCompleted: touch.registerTarget(root)
    Component.onDestruction: touch.unregisterTarget(root)
    Rectangle { anchors.fill: parent; color: "transparent"; border.width: 2; border.color: "#FFFFFF"; visible: root.hardwareFocused; z: 100 }
    Rectangle { objectName: "pressedFeedback"; anchors.fill: parent; color: "#30FFFFFF"; border.width: 2; border.color: "#FFFFFF"; visible: root.pressed && root.containsMouse; z: 101 }
}
