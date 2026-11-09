import QtQuick 2.15
import PushClone 1.0
import "../components" as Components

// ═══════════════════════════════════════════════════════════
// SESSION VIEW - Main view with 8×4 clips grid
// ═══════════════════════════════════════════════════════════
// Layout:
// - Track headers (names + colors)
// - 8×4 ClipPads grid
// - Scene buttons (right side)
// ═══════════════════════════════════════════════════════════

Rectangle {
    id: root

    color: PushCloneTheme.background

    property color masterColor: PushCloneTheme.clipColors[12]

    // ═══════════════════════════════════════════════════════
    // MAIN LAYOUT
    // ═══════════════════════════════════════════════════════
    Column {
        anchors.fill: parent
        spacing: PushCloneTheme.spacing

        // ═══════════════════════════════════════════════════
        // TRACK HEADERS
        // ═══════════════════════════════════════════════════
        Row {
            width: parent.width
            height: 40
            spacing: PushCloneTheme.spacingSmall

            // First 8 tracks (regular width - with clips)
            Repeater {
                model: serialController.trackModel

                Rectangle {
                    property bool activeTrack: model.active

                    // Calculate width to match clip grid (8 tracks)
                    width: (parent.width - 60 - PushCloneTheme.spacing - (PushCloneTheme.spacingSmall * 8)) / 8
                    height: parent.height
                    color: activeTrack ? PushCloneTheme.surface : PushCloneTheme.surfaceHover
                    opacity: activeTrack ? 1.0 : 0.35
                    border.color: activeTrack ? model.color : PushCloneTheme.border
                    border.width: activeTrack ? 2 : 1
                    radius: PushCloneTheme.radius

                    Rectangle {
                        width: parent.width
                        height: 4
                        anchors.top: parent.top
                        color: activeTrack ? model.color : PushCloneTheme.border
                        radius: PushCloneTheme.radius
                    }

                    Text {
                        text: activeTrack ? model.name : ""
                        anchors.centerIn: parent
                        color: activeTrack ? PushCloneTheme.text : PushCloneTheme.textDim
                        font.pixelSize: PushCloneTheme.fontSizeSmall
                        font.family: PushCloneTheme.fontFamily
                        font.bold: activeTrack
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: activeTrack
                        onClicked: {
                            console.log("Track selected:", model.index, model.name)
                            // TODO: Send CMD_TRACK_SELECT
                        }
                    }
                }
            }

            // Master track (same width as scene buttons)
            Rectangle {
                width: 60
                height: parent.height
                color: PushCloneTheme.surface
                border.color: root.masterColor
                border.width: 2
                radius: PushCloneTheme.radius

                // Track color indicator
                Rectangle {
                    width: parent.width
                    height: 4
                    anchors.top: parent.top
                    color: root.masterColor
                    radius: PushCloneTheme.radius
                }

                Text {
                    text: "Master"
                    anchors.centerIn: parent
                    color: PushCloneTheme.text
                    font.pixelSize: PushCloneTheme.fontSizeSmall
                    font.family: PushCloneTheme.fontFamily
                    font.bold: true
                    elide: Text.ElideRight
                }

                // Touch to select track
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        console.log("Track selected: Master")
                        // TODO: Send CMD_TRACK_SELECT
                    }
                }
            }
        }

        // ═══════════════════════════════════════════════════
        // GRID 8×4 + SCENE BUTTONS
        // ═══════════════════════════════════════════════════
        Row {
            width: parent.width
            height: parent.height - 40 - PushCloneTheme.spacing
            spacing: PushCloneTheme.spacing

            // CLIP GRID (8 columns × 4 rows - 8 complete tracks)
            Grid {
                id: clipGrid
                width: parent.width - 60 - PushCloneTheme.spacing
                height: parent.height

                columns: 8
                rows: 4
                columnSpacing: PushCloneTheme.spacingSmall
                rowSpacing: PushCloneTheme.spacingSmall

                // Calculate each pad size
                property real padWidth: (width - (columnSpacing * (columns - 1))) / columns
                property real padHeight: (height - (rowSpacing * (rows - 1))) / rows

                // 32 ClipPads (8×4)
                Repeater {
                    model: serialController.clipModel
                    delegate: Components.ClipPad {
                        width: clipGrid.padWidth
                        height: clipGrid.padHeight

                        trackIndex: model.track
                        sceneIndex: model.scene
                        clipName: model.name
                        clipState: model.state
                        clipColor: model.color

                        onClipTriggered: {
                            console.log("Clip triggered: Track", trackIndex, "Scene", sceneIndex)
                            serialController.sendClipTrigger(trackIndex, sceneIndex)
                        }

                        onClipLongPressed: {
                            console.log("Clip long pressed: Track", trackIndex, "Scene", sceneIndex)
                            // TODO: Show context menu (delete, duplicate, rename)
                        }
                    }
                }
            }

            // SCENE BUTTONS (lateral derecho)
            Column {
                width: 60
                height: parent.height
                spacing: PushCloneTheme.spacingSmall

                Repeater {
                    model: serialController.sceneModel

                    Rectangle {
                        width: parent.width
                        height: (parent.height - (PushCloneTheme.spacingSmall * 3)) / 4
                        color: model.triggered ? PushCloneTheme.surfaceActive : PushCloneTheme.surface
                        border.color: model.color
                        border.width: 1
                        radius: PushCloneTheme.radius

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Text {
                                text: "▶"
                                color: model.triggered ? PushCloneTheme.primary : PushCloneTheme.textDim
                                font.pixelSize: PushCloneTheme.fontSizeLarge
                                font.bold: true
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Text {
                                text: model.name
                                color: PushCloneTheme.textDim
                                font.pixelSize: PushCloneTheme.fontSizeSmall
                                font.family: PushCloneTheme.fontFamily
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        MouseArea {
                            id: sceneMouseArea
                            anchors.fill: parent
                            onClicked: {
                                console.log("Scene fired:", model.index)
                                // TODO: Enviar CMD_SCENE_FIRE
                            }
                        }

                        scale: sceneMouseArea.pressed ? 0.95 : 1.0
                        Behavior on scale {
                            NumberAnimation {
                                duration: PushCloneTheme.animationFast
                                easing.type: Easing.OutQuad
                            }
                        }
                    }
                }
            }
        }
    }
}
