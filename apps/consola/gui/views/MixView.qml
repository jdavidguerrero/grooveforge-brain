import QtQuick 2.15
import QtQuick.Layouts 1.15
import PushClone 1.0

// ═══════════════════════════════════════════════════════════
// MIX VIEW - Using ListView for reactive model updates
// ═══════════════════════════════════════════════════════════

Rectangle {
    id: root
    anchors.fill: parent
    clip: true
    color: PushCloneTheme.background

    // ═══════════════════════════════════════════════════════
    // REAL MODEL CONNECTION
    // ═══════════════════════════════════════════════════════
    property var mixerModel: serialController.mixerModel

    // Sync with model properties
    property int trackBank: mixerModel ? mixerModel.trackBank : 0
    property int selectedTrackIndex: mixerModel ? mixerModel.selectedTrackIndex : 0
    property int totalTracks: mixerModel ? mixerModel.totalTracks : 0

    readonly property int tracksPerBank: 4

    // ═══════════════════════════════════════════════════════
    // HELPERS
    // ═══════════════════════════════════════════════════════
    function setTrackBank(bank) {
        if (mixerModel) {
            mixerModel.trackBank = bank;
            // Notify Teensy about bank change so faders can reset pickup states
            serialController.sendMixerBankChange(bank);
        }
    }

    function setSelectedTrack(index) {
        if (mixerModel) {
            mixerModel.selectedTrackIndex = index;
            // Send to Teensy → Ableton so selection is bidirectional
            serialController.sendTrackSelect(index);
        }
    }

    function nextTrackBank() {
        var maxBanks = Math.ceil(totalTracks / tracksPerBank);
        setTrackBank((trackBank + 1) % Math.max(1, maxBanks));
    }

    function prevTrackBank() {
        var maxBanks = Math.ceil(totalTracks / tracksPerBank);
        setTrackBank((trackBank - 1 + Math.max(1, maxBanks)) % Math.max(1, maxBanks));
    }

    // ═══════════════════════════════════════════════════════
    // LAYOUT
    // ═══════════════════════════════════════════════════════
    Column {
        anchors.fill: parent
        anchors.margins: PushCloneTheme.spacing
        spacing: 8

        // Tracks Grid - Using ListView with reactive model binding
        ListView {
            id: tracksListView
            width: parent.width
            height: parent.height - footer.height - 16
            orientation: ListView.Horizontal
            spacing: 4
            interactive: false
            clip: true

            model: root.mixerModel

            // Only show tracks in current bank (4 tracks)
            delegate: Item {
                id: trackDelegate
                property int globalIndex: model.index
                property int localIndex: globalIndex - (root.trackBank * root.tracksPerBank)
                property bool inCurrentBank: localIndex >= 0 && localIndex < root.tracksPerBank
                property bool hasTrack: model.active

                visible: inCurrentBank
                width: inCurrentBank ? (tracksListView.width / root.tracksPerBank) : 0
                height: tracksListView.height

                Rectangle {
                    anchors.fill: parent
                    radius: PushCloneTheme.radius
                    color: hasTrack && globalIndex === root.selectedTrackIndex ? PushCloneTheme.surfaceActive : PushCloneTheme.surface
                    border.width: hasTrack && globalIndex === root.selectedTrackIndex ? 2 : 1
                    border.color: hasTrack && globalIndex === root.selectedTrackIndex ? PushCloneTheme.primary : PushCloneTheme.border

                    Column {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 4

                        // Track Header
                        Rectangle {
                            width: parent.width
                            height: 40
                            radius: 4
                            color: hasTrack ? model.color : "#303030"

                            Text {
                                anchors.centerIn: parent
                                text: hasTrack ? model.name : "---"
                                font.pixelSize: 12
                                font.bold: true
                                font.family: PushCloneTheme.fontFamily
                                color: "#000000"
                                elide: Text.ElideRight
                                width: parent.width - 8
                                horizontalAlignment: Text.AlignHCenter
                            }

                            MouseArea {
                                anchors.fill: parent
                                enabled: hasTrack
                                onClicked: root.setSelectedTrack(globalIndex)
                            }
                        }

                        // Volume
                        Column {
                            width: parent.width
                            spacing: 4

                            Text {
                                text: hasTrack ? model.volumeLabel : "--"
                                font.pixelSize: 11
                                font.family: PushCloneTheme.fontFamilyMono
                                font.bold: true
                                color: PushCloneTheme.text
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Rectangle {
                                width: parent.width
                                height: 12
                                radius: 6
                                color: PushCloneTheme.background

                                Rectangle {
                                    width: hasTrack ? (parent.width * model.volume) : 0
                                    height: parent.height
                                    radius: 6
                                    color: PushCloneTheme.primary
                                }
                            }

                            Text {
                                text: "VOLUME"
                                font.pixelSize: 8
                                color: PushCloneTheme.textDim
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }

                        // Pan
                        Column {
                            width: parent.width
                            spacing: 4

                            Text {
                                text: hasTrack ? model.panLabel : "--"
                                font.pixelSize: 11
                                font.family: PushCloneTheme.fontFamilyMono
                                font.bold: true
                                color: PushCloneTheme.text
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Rectangle {
                                width: parent.width
                                height: 12
                                radius: 6
                                color: PushCloneTheme.background

                                Rectangle {
                                    width: hasTrack ? (parent.width * model.pan) : parent.width * 0.5
                                    height: parent.height
                                    radius: 6
                                    color: PushCloneTheme.accent
                                }
                            }

                            Text {
                                text: "PAN"
                                font.pixelSize: 8
                                color: PushCloneTheme.textDim
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }

                        // Send A
                        Column {
                            width: parent.width
                            spacing: 4

                            Text {
                                text: hasTrack ? Math.round(model.sendA * 100) + "%" : "--"
                                font.pixelSize: 11
                                font.family: PushCloneTheme.fontFamilyMono
                                color: PushCloneTheme.text
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Rectangle {
                                width: parent.width
                                height: 12
                                radius: 6
                                color: PushCloneTheme.background

                                Rectangle {
                                    width: hasTrack ? (parent.width * model.sendA) : 0
                                    height: parent.height
                                    radius: 6
                                    color: "#00ff88"
                                }
                            }

                            Text {
                                text: "SEND A"
                                font.pixelSize: 8
                                color: PushCloneTheme.textDim
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }

                        // Send B
                        Column {
                            width: parent.width
                            spacing: 4

                            Text {
                                text: hasTrack ? Math.round(model.sendB * 100) + "%" : "--"
                                font.pixelSize: 11
                                font.family: PushCloneTheme.fontFamilyMono
                                color: PushCloneTheme.text
                                anchors.horizontalCenter: parent.horizontalCenter
                            }

                            Rectangle {
                                width: parent.width
                                height: 12
                                radius: 6
                                color: PushCloneTheme.background

                                Rectangle {
                                    width: hasTrack ? (parent.width * model.sendB) : 0
                                    height: parent.height
                                    radius: 6
                                    color: "#ff8800"
                                }
                            }

                            Text {
                                text: "SEND B"
                                font.pixelSize: 8
                                color: PushCloneTheme.textDim
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }

                        Item {
                            height: 4
                        }  // Spacer

                        // M/S/A Buttons
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 4

                            // Mute
                            Rectangle {
                                width: 30
                                height: 24
                                radius: 4
                                color: hasTrack && model.muted ? "#ff4444" : PushCloneTheme.background
                                border.color: PushCloneTheme.border

                                Text {
                                    anchors.centerIn: parent
                                    text: "M"
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: hasTrack && model.muted ? "#ffffff" : PushCloneTheme.textDim
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: hasTrack
                                    onClicked: console.log("Mute toggle track", globalIndex)
                                }
                            }

                            // Solo
                            Rectangle {
                                width: 30
                                height: 24
                                radius: 4
                                color: hasTrack && model.solo ? "#ffaa00" : PushCloneTheme.background
                                border.color: PushCloneTheme.border

                                Text {
                                    anchors.centerIn: parent
                                    text: "S"
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: hasTrack && model.solo ? "#000000" : PushCloneTheme.textDim
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: hasTrack
                                    onClicked: console.log("Solo toggle track", globalIndex)
                                }
                            }

                            // Arm
                            Rectangle {
                                width: 30
                                height: 24
                                radius: 4
                                color: hasTrack && model.armed ? "#ff0000" : PushCloneTheme.background
                                border.color: PushCloneTheme.border

                                Text {
                                    anchors.centerIn: parent
                                    text: "A"
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: hasTrack && model.armed ? "#ffffff" : PushCloneTheme.textDim
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: hasTrack
                                    onClicked: console.log("Arm toggle track", globalIndex)
                                }
                            }
                        }
                    }
                }
            }
        }

        // Footer (Navigation)
        Rectangle {
            id: footer
            width: parent.width
            height: 40
            radius: PushCloneTheme.radius
            color: PushCloneTheme.surface
            border.color: PushCloneTheme.border

            Row {
                anchors.centerIn: parent
                spacing: 16

                // Previous Bank
                Rectangle {
                    width: 32
                    height: 28
                    radius: 4
                    color: PushCloneTheme.background
                    border.color: PushCloneTheme.border

                    Text {
                        anchors.centerIn: parent
                        text: "◀"
                        font.pixelSize: 14
                        color: PushCloneTheme.text
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.prevTrackBank()
                    }
                }

                Text {
                    text: "Bank " + (root.trackBank + 1) + " / " + Math.ceil(root.totalTracks / root.tracksPerBank)
                    font.pixelSize: 12
                    font.family: PushCloneTheme.fontFamilyMono
                    color: PushCloneTheme.text
                    anchors.verticalCenter: parent.verticalCenter
                }

                // Next Bank
                Rectangle {
                    width: 32
                    height: 28
                    radius: 4
                    color: PushCloneTheme.background
                    border.color: PushCloneTheme.border

                    Text {
                        anchors.centerIn: parent
                        text: "▶"
                        font.pixelSize: 14
                        color: PushCloneTheme.text
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.nextTrackBank()
                    }
                }
            }
        }
    }
}
