pragma Singleton
import QtQuick 2.15

// ═══════════════════════════════════════════════════════════
// PUSH CLONE THEME - Global color and style system
// ═══════════════════════════════════════════════════════════
// This file is a Singleton, accessible from any QML file:
// import "." as Local
// color: Local.Theme.background
// ═══════════════════════════════════════════════════════════

QtObject {
    id: theme

    // ═══════════════════════════════════════════════════════
    // BASE COLORS (Inspired by Ableton Push 3)
    // ═══════════════════════════════════════════════════════
    readonly property color background: "#0a0a0a"      // Deep black
    readonly property color surface: "#1a1a1a"         // Elevated surface
    readonly property color surfaceHover: "#252525"    // Hover state
    readonly property color surfaceActive: "#303030"   // Active state
    readonly property color border: "#2a2a2a"          // Subtle borders
    readonly property color borderBright: "#404040"    // Highlighted borders

    // ═══════════════════════════════════════════════════════
    // TEXT COLORS
    // ═══════════════════════════════════════════════════════
    readonly property color text: "#e0e0e0"            // Primary text
    readonly property color textDim: "#888888"         // Secondary text
    readonly property color textDisabled: "#555555"    // Disabled text

    // ═══════════════════════════════════════════════════════
    // ACCENT COLORS (Cyan - Push primary color)
    // ═══════════════════════════════════════════════════════
    readonly property color primary: "#00ffff"         // Primary cyan
    readonly property color primaryDim: "#008888"      // Dimmed cyan
    readonly property color primaryHover: "#33ffff"    // Cyan hover

    // ═══════════════════════════════════════════════════════
    // FUNCTIONAL COLORS
    // ═══════════════════════════════════════════════════════
    readonly property color success: "#00ff00"         // Green (playing)
    readonly property color warning: "#ffff00"         // Yellow (queued)
    readonly property color error: "#ff0000"           // Red (recording/error)
    readonly property color info: "#0088ff"            // Blue (info)

    // ═══════════════════════════════════════════════════════
    // CLIP COLORS (Based on Live's color palette)
    // ═══════════════════════════════════════════════════════
    readonly property var clipColors: [
        "#ff4c4c",  // 0: Red
        "#ffa54c",  // 1: Orange
        "#ffff4c",  // 2: Yellow
        "#a5ff4c",  // 3: Lime
        "#4cff4c",  // 4: Green
        "#4cffa5",  // 5: Mint
        "#4cffff",  // 6: Cyan
        "#4ca5ff",  // 7: Light Blue
        "#4c4cff",  // 8: Blue
        "#a54cff",  // 9: Purple
        "#ff4cff",  // 10: Magenta
        "#ff4ca5",  // 11: Pink
        "#7f7f7f",  // 12: Gray
        "#ffffff",  // 13: White
        "#505050",  // 14: Dark Gray
        "#0a0a0a"   // 15: Black/Empty
    ]

    // ═══════════════════════════════════════════════════════
    // CLIP STATES
    // ═══════════════════════════════════════════════════════
    readonly property color clipEmpty: "#282828"       // No clip - RGB(40, 40, 40)
    readonly property color clipStopped: "#3a3a3a"     // Stopped clip
    // Playing/Queued/Recording use base color modulation

    // ═══════════════════════════════════════════════════════
    // TYPOGRAPHY
    // ═══════════════════════════════════════════════════════
    readonly property string fontFamily: "Helvetica"
    readonly property string fontFamilyMono: "Monaco"  // macOS system monospace font

    readonly property int fontSizeSmall: 10
    readonly property int fontSizeNormal: 12
    readonly property int fontSizeMedium: 14
    readonly property int fontSizeLarge: 16
    readonly property int fontSizeXLarge: 20
    readonly property int fontSizeTitle: 24

    // ═══════════════════════════════════════════════════════
    // SPACING AND DIMENSIONS
    // ═══════════════════════════════════════════════════════
    readonly property int spacing: 8               // Standard spacing
    readonly property int spacingSmall: 4
    readonly property int spacingLarge: 16

    readonly property int radius: 4                // Standard border radius
    readonly property int radiusLarge: 8

    readonly property int touchTargetMin: 44       // Minimum touch target size (iOS/Android standard)

    // Specific dimensions
    readonly property int headerHeight: 60
    readonly property int footerHeight: 60
    readonly property int tabBarHeight: 50

    // ═══════════════════════════════════════════════════════
    // ANIMATIONS (Durations in ms) - Optimized for RPi 5
    // ═══════════════════════════════════════════════════════
    readonly property int animationFast: 100       // Faster response
    readonly property int animationNormal: 200     // Smooth transitions
    readonly property int animationSlow: 350       // Noticeable but not sluggish

    // Easing curve for smooth animations
    readonly property int easingType: Easing.OutCubic

    // ═══════════════════════════════════════════════════════
    // HELPERS: Utility functions
    // ═══════════════════════════════════════════════════════

    // Get clip color by index
    function getClipColor(colorIndex) {
        if (colorIndex < 0 || colorIndex >= clipColors.length) {
            return clipEmpty
        }
        return clipColors[colorIndex]
    }

    // Modulate color based on clip state
    function getClipStateColor(baseColor, state) {
        // state: 0=empty, 1=playing, 2=queued, 3=recording, 4=stopped
        switch(state) {
            case 0: return clipEmpty
            case 1: return Qt.lighter(baseColor, 1.5)  // Playing: brighter
            case 2: return Qt.rgba(1.0, 1.0, 0.3, 1.0) // Queued: yellow
            case 3: return Qt.rgba(1.0, 0.3, 0.3, 1.0) // Recording: red
            case 4: return Qt.darker(baseColor, 1.3)   // Stopped: darker
            default: return baseColor
        }
    }

    // Adjust color opacity
    function colorWithAlpha(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }
}
