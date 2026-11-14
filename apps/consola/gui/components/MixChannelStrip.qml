import QtQuick 2.15
import PushClone 1.0

// ═══════════════════════════════════════════════════════════
// MIX CHANNEL STRIP - Shared by tracks and returns
// ═══════════════════════════════════════════════════════════
Rectangle {
    id: root

    // ═══════════════════════════════════════════════════════
    // PUBLIC API
    // ═══════════════════════════════════════════════════════
    property string trackName: "Track"
    property string trackTag: "DRM"
    property string clipName: ""
    property color trackColor: PushCloneTheme.primary

    property real meterLeft: 0.4      // 0.0 - 1.0
    property real meterRight: 0.4     // 0.0 - 1.0

    property string mainValue: "0.0 dB"
    property string mainLabel: "VOL"

    property string secondaryLabel: "PAN"
    property string secondaryValue: "C"
    property real secondaryRatio: 0.5

    property bool isMuted: false
    property bool isSolo: false
    property bool isArmed: false

    property bool isSelected: false
    property bool needsPickup: false

    property real faderValue: 0.5           // actual value, 0-1
    property real faderPhysicalValue: -1.0  // hardware position

    signal stripTapped()

    width: 140
    height: 300
    radius: PushCloneTheme.radius
    color: isSelected ? PushCloneTheme.surfaceActive : PushCloneTheme.surface
    border.color: isSelected ? PushCloneTheme.primary : PushCloneTheme.border
    border.width: isSelected ? 2 : 1

    Column {
        anchors.fill: parent
        anchors.margins: PushCloneTheme.spacing
        spacing: PushCloneTheme.spacingSmall

        // Track tag + name
        Column {
            spacing: 4

            Rectangle {
                width: 32
                height: 16
                radius: 3
                color: PushCloneTheme.colorWithAlpha(trackColor, 0.35)

                Text {
                    anchors.centerIn: parent
                    text: trackTag
                    font.pixelSize: PushCloneTheme.fontSizeSmall
                    font.family: PushCloneTheme.fontFamilyMono
                    color: PushCloneTheme.text
                }
            }

            Text {
                text: trackName
                width: parent.width
                color: PushCloneTheme.text
                font.pixelSize: PushCloneTheme.fontSizeNormal
                font.family: PushCloneTheme.fontFamily
                elide: Text.ElideRight
            }
        }

        // Dual meters
        Column {
            spacing: 4

            Repeater {
                model: 2
                Rectangle {
                    width: parent.width
                    height: 6
                    radius: 3
                    color: PushCloneTheme.surfaceHover

                    Rectangle {
                        anchors {
                            left: parent.left
                            top: parent.top
                            bottom: parent.bottom
                        }
                        width: parent.width * (index === 0 ? root.meterLeft : root.meterRight)
                        radius: 3
                        color: index === 0 ? PushCloneTheme.primary : PushCloneTheme.success
                    }
                }
            }
        }

        // Main value display
        Column {
            spacing: 2
            anchors.horizontalCenter: parent.horizontalCenter

            Text {
                text: mainValue
                color: PushCloneTheme.primary
                font.pixelSize: PushCloneTheme.fontSizeLarge
                font.family: PushCloneTheme.fontFamilyMono
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }

            Text {
                text: mainLabel
                color: PushCloneTheme.textDim
                font.pixelSize: PushCloneTheme.fontSizeSmall
                font.family: PushCloneTheme.fontFamily
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
            }
        }

        // State buttons (Mute / Solo / Arm)
        Row {
            spacing: PushCloneTheme.spacingSmall
            anchors.horizontalCenter: parent.horizontalCenter

            component StateBadge: Rectangle {
                property string label: "M"
                property bool active: false
                property color activeColor: PushCloneTheme.warning

                width: 26
                height: 20
                radius: PushCloneTheme.radius
                border.width: 1
                border.color: active ? activeColor : PushCloneTheme.border
                color: active ? PushCloneTheme.colorWithAlpha(activeColor, 0.3) : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: label
                    color: active ? activeColor : PushCloneTheme.textDim
                    font.pixelSize: PushCloneTheme.fontSizeSmall
                    font.family: PushCloneTheme.fontFamily
                    font.bold: true
                }
            }

            StateBadge { label: "M"; active: isMuted; activeColor: PushCloneTheme.warning }
            StateBadge { label: "S"; active: isSolo; activeColor: PushCloneTheme.primary }
            StateBadge { label: "A"; active: isArmed; activeColor: PushCloneTheme.error }
        }

        // Pickup indicator
        Text {
            visible: needsPickup
            text: qsTr("Pickup")
            color: PushCloneTheme.warning
            font.pixelSize: PushCloneTheme.fontSizeSmall
            horizontalAlignment: Text.AlignHCenter
            width: parent.width
        }

        // Fader visualization
        Rectangle {
            width: parent.width
            height: 130
            radius: PushCloneTheme.radius
            color: PushCloneTheme.background
            border.color: PushCloneTheme.border

            Rectangle {
                id: faderSlot
                width: 6
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                color: PushCloneTheme.surfaceHover
                radius: 3
            }

            // Actual value handle
            Rectangle {
                width: 24
                height: 8
                radius: 4
                color: PushCloneTheme.primary
                anchors.horizontalCenter: faderSlot.horizontalCenter
                y: faderSlot.height * (1 - faderValue) + faderSlot.y - height / 2
            }

            // Physical fader indicator
            Rectangle {
                visible: faderPhysicalValue >= 0
                width: 20
                height: 4
                radius: 2
                color: PushCloneTheme.colorWithAlpha(PushCloneTheme.textDim, 0.5)
                anchors.horizontalCenter: faderSlot.horizontalCenter
                y: faderSlot.height * (1 - faderPhysicalValue) + faderSlot.y - height / 2
            }
        }

        // Secondary value bar
        Column {
            spacing: 2

            Text {
                text: secondaryLabel + ": " + secondaryValue
                color: PushCloneTheme.textDim
                font.pixelSize: PushCloneTheme.fontSizeSmall
                font.family: PushCloneTheme.fontFamily
            }

            Rectangle {
                width: parent.width
                height: 6
                radius: 3
                color: PushCloneTheme.surfaceHover

                Rectangle {
                    width: parent.width * secondaryRatio
                    height: parent.height
                    radius: 3
                    color: PushCloneTheme.primary
                }
            }
        }

        // Clip label
        Text {
            text: clipName
            color: PushCloneTheme.textDim
            font.pixelSize: PushCloneTheme.fontSizeSmall
            font.family: PushCloneTheme.fontFamily
            elide: Text.ElideRight
            width: parent.width
            visible: clipName.length > 0
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.stripTapped()
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
    }
}
