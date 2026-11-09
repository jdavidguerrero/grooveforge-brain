import QtQuick 2.15
import PushClone 1.0

// ═══════════════════════════════════════════════════════════
// CLIP PAD - Individual component for session grid
// ═══════════════════════════════════════════════════════════
// Represents a clip in the SessionView (8×4 grid)
// States: 0=empty, 1=stopped, 2=playing, 3=queued, 4=recording
// ═══════════════════════════════════════════════════════════

Rectangle {
    id: root

    // ═══════════════════════════════════════════════════════
    // PUBLIC PROPERTIES
    // ═══════════════════════════════════════════════════════
    property string clipName: ""                    // Clip name
    property int clipState: 0                       // 0-4 (see above)
    property color clipColor: PushCloneTheme.clipColors[0]  // Base color
    property int trackIndex: 0                      // Track index (0-7)
    property int sceneIndex: 0                      // Scene index (0-3)
    property bool isSelected: false                 // Selected clip

    // ═══════════════════════════════════════════════════════
    // SIGNALS
    // ═══════════════════════════════════════════════════════
    signal clipTriggered                          // Tap: trigger clip
    signal clipStopped                            // Stop clip
    signal clipLongPressed                        // Long press: context menu

    // ═══════════════════════════════════════════════════════
    // APPEARANCE
    // ═══════════════════════════════════════════════════════
    width: 80
    height: 60
    radius: PushCloneTheme.radius
    antialiasing: true  // Smooth edges (RPi 5 optimization)

    readonly property bool hasColorInfo: !Qt.colorEqual(clipColor, PushCloneTheme.clipEmpty)

    // Use the color directly from the model - it's already processed by the Python script
    // with the correct state-based color logic (green for playing/queued, red for recording, etc.)
    color: clipColor

    border.color: isSelected ? PushCloneTheme.primary : PushCloneTheme.border
    border.width: isSelected ? 2 : 1

    // ═══════════════════════════════════════════════════════
    // SMOOTH COLOR TRANSITIONS
    // ═══════════════════════════════════════════════════════
    Behavior on color {
        ColorAnimation {
            duration: PushCloneTheme.animationFast
            easing.type: Easing.InOutQuad
        }
    }

    Behavior on border.color {
        ColorAnimation {
            duration: PushCloneTheme.animationFast
        }
    }

    // ═══════════════════════════════════════════════════════
    // CLIP NAME (if exists)
    // ═══════════════════════════════════════════════════════
    Text {
        id: nameText
        text: clipName
        visible: clipName.length > 0  // Show whenever we have a name

        anchors {
            centerIn: parent
            margins: PushCloneTheme.spacingSmall
        }

        color: "#000000"

        font.pixelSize: PushCloneTheme.fontSizeMedium
        font.family: PushCloneTheme.fontFamily
        font.bold: clipState === 2  // Bold when playing
        style: Text.Raised
        styleColor: "#000000"

        elide: Text.ElideRight
        width: parent.width - 8
        horizontalAlignment: Text.AlignHCenter
    }

    // ═══════════════════════════════════════════════════════
    // STATE INDICATOR (icon)
    // ═══════════════════════════════════════════════════════
    Text {
        id: stateIcon
        visible: clipState === 2 || clipState === 4

        anchors {
            right: parent.right
            top: parent.top
            margins: 4
        }

        text: clipState === 2 ? "▶" : "●"  // Playing: ▶, Recording: ●
        color: "#ffffff"
        font.pixelSize: 8
        font.bold: true
    }

    // ═══════════════════════════════════════════════════════
    // PULSING ANIMATION (queued and recording)
    // ═══════════════════════════════════════════════════════
    SequentialAnimation {
        running: clipState === 3 || clipState === 4
        loops: Animation.Infinite

        NumberAnimation {
            target: root
            property: "opacity"
            to: 0.6
            duration: 500
            easing.type: Easing.InOutQuad
        }

        NumberAnimation {
            target: root
            property: "opacity"
            to: 1.0
            duration: 500
            easing.type: Easing.InOutQuad
        }
    }

    // ═══════════════════════════════════════════════════════
    // TOUCH INTERACTION
    // ═══════════════════════════════════════════════════════
    MouseArea {
        id: touchArea
        anchors.fill: parent

        // Enable long press detection
        pressAndHoldInterval: 600

        // Visual feedback on press
        onPressed: {
            root.scale = 0.95;
        }

        onReleased: {
            root.scale = 1.0;
        }

        onCanceled: {
            root.scale = 1.0;
        }

        // Tap: Trigger or stop clip
        onClicked: {
            console.log("Clip triggered:", trackIndex, sceneIndex, clipName);
            root.clipTriggered();
        }

        // Long press: Context menu
        onPressAndHold: {
            console.log("Clip long pressed:", clipName);
            root.clipLongPressed();
        }
    }

    // Smooth scale animation
    Behavior on scale {
        NumberAnimation {
            duration: PushCloneTheme.animationFast
            easing.type: Easing.OutQuad
        }
    }

    // ═══════════════════════════════════════════════════════
    // DEFINED STATES (alternative to color: logic)
    // ═══════════════════════════════════════════════════════
    states: [
        State {
            name: "empty"
            when: clipState === 0
            PropertyChanges {
                target: root
                opacity: 0.3
            }
        },
        State {
            name: "playing"
            when: clipState === 2
            // Color already handled in binding
        },
        State {
            name: "queued"
            when: clipState === 3
        },
        State {
            name: "recording"
            when: clipState === 4
        },
        State {
            name: "stopped"
            when: clipState === 1
        }
    ]
}
