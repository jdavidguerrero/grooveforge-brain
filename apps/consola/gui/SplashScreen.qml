import QtQuick 2.15
import PushClone 1.0

Rectangle {
    id: root
    width: 800
    height: 480
    color: "#0a0a0a"

    // Signal emitted when finished
    signal finished()

    // ═══════════════════════════════════════════════
    // LOGO (Image)
    // ═══════════════════════════════════════════════
    Image {
        id: logo
        source: "qrc:/assets/logo.png"

        // Adjust size (adjust according to your logo)
        width: 250
        height: 350
        fillMode: Image.PreserveAspectFit

        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top
            topMargin: 80
        }

        // Fade in animation
        NumberAnimation on opacity {
            from: 0.0
            to: 1.0
            duration: 1000
            easing.type: Easing.InOutQuad
        }

        // Scale effect
        NumberAnimation on scale {
            from: 0.8
            to: 1.0
            duration: 1000
            easing.type: Easing.OutBack
        }
    }

    // ═══════════════════════════════════════════════
    // TEXTO DEBAJO DEL LOGO (Opcional)
    // ═══════════════════════════════════════════════

    // ═══════════════════════════════════════════════
    // LOADING BAR CONTAINER
    // ═══════════════════════════════════════════════
    Rectangle {
        id: loadingContainer
        width: 400
        height: 8
        color: "#1a1a1a"
        radius: 4
        border.color: "#2a2a2a"
        border.width: 1

        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: 100
        }

        // Barra de progreso (fill)
        Rectangle {
            id: loadingFill
            height: parent.height
            color: "#00ffff"
            radius: parent.radius

            // Gradient for professional effect
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00ffff" }
                GradientStop { position: 1.0; color: "#0088ff" }
            }

            // Fill animation
            NumberAnimation on width {
                id: loadingAnimation
                from: 0
                to: loadingContainer.width
                duration: 2000
                easing.type: Easing.InOutQuad

                onRunningChanged: {
                    if (!running) {
                        // When finished, emit signal
                        console.log("Loading complete!")
                        root.finished()
                    }
                }
            }

            // Efecto de brillo (animado)
            Rectangle {
                width: 60
                height: parent.height
                x: parent.width - width
                color: "transparent"

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 0.5; color: "#ffffff" }
                    GradientStop { position: 1.0; color: "transparent" }
                }

                opacity: 0.3

                // Moving shine animation
                SequentialAnimation on x {
                    running: loadingAnimation.running
                    loops: Animation.Infinite

                    NumberAnimation {
                        from: -60
                        to: loadingContainer.width
                        duration: 1000
                    }
                }
            }
        }
    }

    // ═══════════════════════════════════════════════
    // LOADING PERCENTAGE
    // ═══════════════════════════════════════════════
    Text {
        id: loadingPercent
        text: Math.round((loadingFill.width / loadingContainer.width) * 100) + "%"
        color: "#00ffff"
        font.pixelSize: 16
        font.bold: true
        font.family: PushCloneTheme.fontFamilyMono

        anchors {
            horizontalCenter: parent.horizontalCenter
            top: loadingContainer.bottom
            topMargin: 15
        }
    }

    // ═══════════════════════════════════════════════
    // LOADING TEXT (Pulsing)
    // ═══════════════════════════════════════════════
    Text {
        id: loadingText
        text: "Loading..."
        color: "#555555"
        font.pixelSize: 14

        anchors {
            horizontalCenter: parent.horizontalCenter
            top: loadingPercent.bottom
            topMargin: 10
        }

        // Pulse animation
        SequentialAnimation on opacity {
            loops: Animation.Infinite
            running: loadingAnimation.running

            NumberAnimation { to: 0.3; duration: 600 }
            NumberAnimation { to: 1.0; duration: 600 }
        }

        // Dots animation
        property int dotCount: 0

        Timer {
            interval: 500
            running: loadingAnimation.running
            repeat: true
            onTriggered: {
                loadingText.dotCount = (loadingText.dotCount + 1) % 4
                var dots = ""
                for (var i = 0; i < loadingText.dotCount; i++) {
                    dots += "."
                }
                loadingText.text = "Loading" + dots
            }
        }
    }

    // ═══════════════════════════════════════════════
    // VERSION INFO (corner)
    // ═══════════════════════════════════════════════
    Text {
        text: "v1.0.0"
        color: "#333333"
        font.pixelSize: 10

        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 15
        }
    }
}
