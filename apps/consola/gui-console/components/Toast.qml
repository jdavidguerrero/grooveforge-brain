import QtQuick
import "../theme"
Panel {
    id: root
    property string icon: "KICK"
    property string message: "KICK enters on the phrase"
    property string value: "IN 4 BARS"
    property string tone: "KICK"
    property bool animate: false
    property color tint: tone === "link" ? Theme.tealDim : tone === "warning" ? Theme.amber : tone === "engine" ? Theme.purple : Theme.roleColor(tone)
    width: Theme.toastWidth; height: Theme.toastHeight; color: Theme.surface2; border.color: Theme.line2
    Rectangle { x: Theme.s3; anchors.verticalCenter: parent.verticalCenter; width: Theme.toastIcon; height: Theme.toastIcon; radius: Theme.s1; color: root.tint
        Label { anchors.centerIn: parent; text: root.icon; font.pixelSize: Theme.tiny; color: root.tone === "link" ? Theme.tealPlus : Theme.bg }
    }
    Label { x: Theme.toastIcon+Theme.s3+Theme.m14; width: parent.width-x-Theme.columnWidth; anchors.verticalCenter: parent.verticalCenter; text: root.message; font.pixelSize: Theme.m14 }
    Label { anchors.right: parent.right; anchors.rightMargin: Theme.s4; anchors.verticalCenter: parent.verticalCenter; text: root.value; font.family: Theme.displayFont; font.pixelSize: Theme.s5 }
    SequentialAnimation {
        running: root.animate
        OpacityAnimator { target: root; from: 0; to: 1; duration: Theme.enterMs }
        PauseAnimation { duration: Theme.holdMs }
        OpacityAnimator { target: root; from: 1; to: 0; duration: Theme.exitMs }
    }
}
