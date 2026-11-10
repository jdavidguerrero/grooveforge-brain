import QtQuick
MouseArea {
    id: root
    anchors.fill: parent
    property bool clickable: enabled
    property string hardwareAction: "navigation"
    property var callback: null
    property bool hardwareFocused: false
    objectName: "touchTarget"
    preventStealing: false
    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    function activate() { if (enabled && typeof callback === 'function') callback() }
    onClicked: activate()
    Component.onCompleted: touch.registerTarget(root)
    Component.onDestruction: touch.unregisterTarget(root)
    Rectangle { anchors.fill: parent; color: "transparent"; border.width: 2; border.color: "#FFFFFF"; visible: root.hardwareFocused; z: 100 }
}
