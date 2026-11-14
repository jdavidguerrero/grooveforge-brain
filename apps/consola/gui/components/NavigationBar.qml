import QtQuick 2.15
import PushClone 1.0

// ═══════════════════════════════════════════════════════════
// NAVIGATION BAR - Tab bar for switching between views
// ═══════════════════════════════════════════════════════════
// Emits viewChanged(viewIndex) signal when view changes
// 0=Session, 1=Mix, 2=Device, 3=Browse, 4=Note/Seq
// ═══════════════════════════════════════════════════════════

Rectangle {
    id: root

    height: PushCloneTheme.tabBarHeight
    color: PushCloneTheme.surface

    // ═══════════════════════════════════════════════════════
    // PROPERTIES
    // ═══════════════════════════════════════════════════════
    property int currentView: 0  // Active view (0-4)

    // ═══════════════════════════════════════════════════════
    // SIGNALS
    // ═══════════════════════════════════════════════════════
    signal viewChanged(int viewIndex)

    // ═══════════════════════════════════════════════════════
    // BOTTOM BORDER
    // ═══════════════════════════════════════════════════════
    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: 1
        color: PushCloneTheme.border
    }

    // ═══════════════════════════════════════════════════════
    // TABS ROW
    // ═══════════════════════════════════════════════════════
    Row {
        anchors.fill: parent
        spacing: 0

        // Tab Button Component (reusable)
        component TabButton: Rectangle {
            property string label: ""
            property int index: 0
            property bool isActive: root.currentView === index

            width: parent.width / 5  // 5 tabs
            height: parent.height
            color: isActive ? PushCloneTheme.surfaceActive : "transparent"

            // Hover effect
            Rectangle {
                anchors.fill: parent
                color: PushCloneTheme.surfaceHover
                opacity: touchArea.pressed ? 0.5 : (touchArea.containsMouse ? 0.3 : 0)

                Behavior on opacity {
                    NumberAnimation { duration: PushCloneTheme.animationFast }
                }
            }

            // Label
            Text {
                text: label
                anchors.centerIn: parent
                color: isActive ? PushCloneTheme.primary : PushCloneTheme.text
                font.pixelSize: PushCloneTheme.fontSizeNormal
                font.family: PushCloneTheme.fontFamily
                font.bold: isActive

                Behavior on color {
                    ColorAnimation { duration: PushCloneTheme.animationFast }
                }
            }

            // Active indicator (bottom line)
            Rectangle {
                visible: isActive
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                }
                height: 3
                color: PushCloneTheme.primary
            }

            // Touch interaction
            MouseArea {
                id: touchArea
                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    console.log("View changed to:", label, index)
                    root.currentView = index
                    root.viewChanged(index)
                }
            }

            // Scale feedback
            scale: touchArea.pressed ? 0.97 : 1.0
            Behavior on scale {
                NumberAnimation {
                    duration: PushCloneTheme.animationFast
                    easing.type: Easing.OutQuad
                }
            }
        }

        // ═══════════════════════════════════════════════════
        // DEFINED TABS
        // ═══════════════════════════════════════════════════
        TabButton {
            label: "SESSION"
            index: 0
        }

        TabButton {
            label: "MIX"
            index: 1
        }

        TabButton {
            label: "DEVICE"
            index: 2
        }

        TabButton {
            label: "BROWSE"
            index: 3
        }

        TabButton {
            label: "NOTE"
            index: 4
        }
    }
}
