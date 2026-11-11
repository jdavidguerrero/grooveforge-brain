pragma Singleton
import QtQuick
QtObject {
    id: theme
    readonly property color bg: "#0A0A0A"
    readonly property color surface: "#141414"
    readonly property color surface2: "#1C1C1C"
    readonly property color line: "#242424"
    readonly property color line2: "#2E2E2E"
    readonly property color keyTop: "#1F1F1F"
    readonly property color white: "#F2F2F2"
    readonly property color grayN: "#9A9AA6"
    readonly property color teal: "#1D9E75"
    readonly property color tealPlus: "#5DCAA5"
    readonly property color tealDim: "#0E5040"
    readonly property color gray: "#9FE1CB"
    readonly property color purple: "#534AB7"
    readonly property color purpleBright: "#7A6FE0"
    readonly property color purpleDim: "#2B2758"
    readonly property color mon: "#5A5A6E"
    readonly property color hyb: "#1E90C0"
    readonly property color hybHi: "#6CC4EA"
    readonly property color hybDim: "#0E3F55"
    readonly property color red: "#E84A4A"
    readonly property color amber: "#E8B84A"
    readonly property color liveLead: "#E37FB0"
    readonly property color liveBass: "#6E9BE8"
    readonly property color liveDrums: "#E8875A"
    readonly property color orange: "#FF6A35"
    readonly property color gpBgBase: "#09090B"
    readonly property color gpBgCard: "#141418"
    readonly property color gpBorder: "#27272A"
    readonly property color gpTeal: "#00D4AA"
    readonly property color gpPurple: "#8B5CF6"
    readonly property color gpText: "#FAFAFA"
    readonly property color gpTextSec: "#A1A1AA"
    readonly property var roleColors: ({"KICK":"#FF4560","DRUMS":"#FF9A2E","PERC":"#F5E04A","TOP":"#E8F0FF","BASS":"#3D8CFF","CHORDS":"#FF6FC8","LEAD":"#B6F23D","PAD":"#F2C9A0","FX":"#8E9AAF"})
    readonly property var roleDims: ({"KICK":"#4F1B22","DRUMS":"#4F3214","PERC":"#4C461C","TOP":"#484A4F","BASS":"#182E4F","CHORDS":"#4F263F","LEAD":"#3A4B18","PAD":"#4B3F34","FX":"#2F3238"})
    readonly property int rootWidth: 1280
    readonly property int rootHeight: 720
    readonly property int topHeight: 56
    readonly property int contentY: 56
    readonly property int contentHeight: 568
    readonly property int keysY: 624
    readonly property int keysHeight: 96
    readonly property int columnWidth: 152
    readonly property int columnPitch: 160
    readonly property int columns: 8
    readonly property int keyHeight: 80
    readonly property int keyLedY: 6
    readonly property int keyLedHeight: 4
    readonly property int keyInset: 12
    readonly property int keyLabelY: 16
    readonly property int keyValueY: 46
    readonly property int keyPositionY: 73
    readonly property int s1: 4
    readonly property int s2: 8
    readonly property int s3: 12
    readonly property int s4: 16
    readonly property int s5: 24
    readonly property int s6: 48
    readonly property int rCell: 6
    readonly property int rCard: 8
    readonly property int rWindow: 10
    readonly property int rOs: 12
    readonly property int hairline: 1
    readonly property int edge: 2
    readonly property int dHero: 160
    readonly property int dXl: 64
    readonly property int dL: 40
    readonly property int dM: 26
    readonly property int m28: 28
    readonly property int m20: 20
    readonly property int m18: 18
    readonly property int m14: 14
    readonly property int m12: 12
    readonly property int m11: 11
    readonly property int keyLabel: 13
    readonly property int tagSize: 9
    readonly property int tiny: 10
    readonly property int lineHero: 160
    readonly property int lineXl: 64
    readonly property int lineL: 40
    readonly property int lineM: 28
    readonly property int line28: 32
    readonly property int line20: 24
    readonly property int line18: 22
    readonly property int line14: 18
    readonly property int line12: 16
    readonly property int line11: 14
    readonly property int meterMs: 80
    readonly property int enterMs: 150
    readonly property int exitMs: 300
    readonly property int holdMs: 4000
    readonly property int frameMs: 16
    readonly property int infoHeight: 104
    readonly property int ringSize: 88
    readonly property int ringStroke: 10
    readonly property int ringCount: 52
    readonly property int sectionSize: 44
    readonly property int trackSize: 22
    readonly property int summaryX: 8
    readonly property int summaryY: 8
    readonly property int summaryWidth: 1264
    readonly property int roomY: 140
    readonly property int nextY: 280
    readonly property int optionAY: 404
    readonly property int optionBY: 496
    readonly property int roomHeight: 112
    readonly property int nextHeight: 96
    readonly property int optionHeight: 64
    readonly property int rowLabelHeight: 16
    readonly property int rowLabelGap: 4
    readonly property int cellPad: 10
    readonly property int stepHeight: 6
    readonly property int levelHeight: 4
    readonly property int phraseX: 124
    readonly property int roomTitleX: 384
    readonly property int nextTitleX: 656
    readonly property int transitionSummaryX: 1000
    readonly property int ringX: 12
    readonly property real ringSegment: 45
    readonly property real ringStart: -88
    readonly property real ringEnd: -47
    readonly property int handMin: 4
    readonly property int handMax: 30
    readonly property int rulerTicks: 14
    readonly property real noteCmStep: 2.75
    readonly property int layerHeader: 96
    readonly property int layerCardsY: 112
    readonly property int layerCardsHeight: 448
    readonly property int optionWidth: 312
    readonly property int rollHeight: 190
    readonly property int rollRows: 7
    readonly property int rollSteps: 16
    readonly property int optionHeaderHeight: 40
    readonly property int optionNameY: 268
    readonly property int optionEnergyY: 322
    readonly property int optionHintY: 404
    readonly property int modalX: 160
    readonly property int modalY: 96
    readonly property int modalWidth: 960
    readonly property int modalHeight: 488
    readonly property int modalHeader: 72
    readonly property int modalFooter: 52
    readonly property int modalTileHeight: 124
    readonly property int modalTileY: 92
    readonly property int modalOrderY: 240
    readonly property int modalChipY: 272
    readonly property int modalChipHeight: 72
    readonly property int modalProgressY: 356
    readonly property int toastWidth: 560
    readonly property int toastHeight: 64
    readonly property int toastX: 360
    readonly property int toastY: 488
    readonly property int toastOlderY: 416
    readonly property int toastIcon: 40
    readonly property int bootGlyphY: 250
    readonly property int bootGlyphSize: 72
    readonly property int bootWordY: 342
    readonly property int bootWordSize: 48
    readonly property int bootSubY: 414
    readonly property int bootProgressY: 475
    readonly property int bootProgressWidth: 320
    readonly property int bootLogX: 48
    readonly property int bootLogY: 600
    readonly property int bootVersionY: 668
    readonly property int modeX: 48
    readonly property int modeY: 40
    readonly property int modeWidth: 580
    readonly property int modeHeight: 456
    readonly property int modeGap: 24
    readonly property int modePad: 32
    readonly property int modeTitleY: 72
    readonly property int modeTitleSize: 44
    readonly property int modeDescY: 136
    readonly property int modeStatusY: 304
    readonly property int modeHintY: 520
    readonly property int settingsNavWidth: 240
    readonly property int settingsBodyX: 288
    readonly property int settingsBodyWidth: 968
    readonly property int settingsRow: 56
    readonly property int settingsLedHeight: 116
    readonly property int settingsForceY: 160
    readonly property int settingsForceHeight: 220
    readonly property int settingsIrY: 400
    readonly property int settingsIrHeight: 128
    readonly property int mixerGroupHeight: 64
    readonly property int mixerStripsY: 80
    readonly property int mixerStripHeight: 480
    readonly property int mixerGroupWidth: 230
    readonly property int mixerMasterWidth: 312
    readonly property int mixerMasterX: 960
    readonly property int mixerHeader: 30
    readonly property int mixerEqY: 40
    readonly property int mixerFilterY: 116
    readonly property int mixerSendsY: 196
    readonly property int mixerCueY: 248
    readonly property int mixerFaderY: 288
    readonly property int mixerFaderHeight: 140
    readonly property int handsPanelWidth: 604
    readonly property int handsPanelHeight: 552
    readonly property int handsPanelX: 24
    readonly property int handsPanelY: 8
    readonly property int handsReadoutY: 292
    readonly property int handsReadoutSize: 96
    readonly property int handsValueY: 400
    readonly property int handsFootY: 496
    readonly property int handsRulerX: 460
    readonly property int handsLadderX: 380
    readonly property int handsLadderWidth: 200
    readonly property int glowLedHeight: 20
    readonly property int bootGlowSize: 152
    readonly property int handHaloWidth: 60
    readonly property int handHaloHeight: 220
    readonly property int osSliderHeight: 8
    readonly property int osThumb: 24
    readonly property int fsrHeight: 72
    readonly property int fsrTileWidth: 214
    readonly property real scrimOpacity: 0.72
    readonly property real mutedToastOpacity: 0.6
    readonly property real labelTracking: 0.08
    readonly property color rollBg: "#0C0C0C"
    readonly property color rollAlt: "#0F0F0F"
    readonly property color keyBottom: "#161616"
    readonly property color off: "#3A3A44"
    readonly property color aiText: "#A9A0F2"
    property FontAsset displayLoader: FontAsset { source: localFonts.display || "" }
    property FontAsset monoLoader: FontAsset { source: localFonts.mono || "" }
    property FontAsset monoMediumLoader: FontAsset { source: localFonts.monoMedium || "" }
    property FontAsset monoSemiBoldLoader: FontAsset { source: localFonts.monoSemiBold || "" }
    property FontAsset monoBoldLoader: FontAsset { source: localFonts.monoBold || "" }
    property FontAsset archivoLoader: FontAsset { source: localFonts.archivo || "" }
    property FontAsset osLoader: FontAsset { source: localFonts.os || "" }
    property FontAsset osMonoLoader: FontAsset { source: localFonts.osMono || "" }
    readonly property string displayFont: displayLoader.status === FontLoader.Ready ? displayLoader.name : fallbackFonts.sans
    readonly property string monoFont: monoLoader.status === FontLoader.Ready ? monoLoader.name : fallbackFonts.mono
    readonly property string osFont: osLoader.status === FontLoader.Ready ? osLoader.name : fallbackFonts.sans
    readonly property string osMonoFont: osMonoLoader.status === FontLoader.Ready ? osMonoLoader.name : fallbackFonts.mono
    readonly property string archivoFont: archivoLoader.status === FontLoader.Ready ? archivoLoader.name : displayFont
    function ledGlow(name) { return Qt.resolvedUrl("../assets/led-"+(roleColors[name] ? name : name === "PURPLE" || name === "WHITE" ? name : "TEAL")+".png") }
    function roleColor(name) { return name[0] === "#" ? name : roleColors[name] || (name === "PURPLE" ? purpleBright : tealPlus) }
    function roleDim(name) { return name[0] === "#" ? Qt.darker(name, 3) : roleDims[name] || line2 }
    // 26 mm pitch / 0.12109 mm per px; outer encoder axes lie off screen.
    // Wider edge caps are a readable compromise, not physical 1:1 alignment.
    readonly property real softOuterWidth: 26 / 0.12109
    readonly property real softInnerWidth: (rootWidth - 2 * softOuterWidth) / 6
    function softKeyWidth(i) { return i === 0 || i === 7 ? softOuterWidth - s2 : softInnerWidth - s2 }
    function softKeyX(i) { return s1 + (i === 0 ? 0 : softOuterWidth + (i-1)*softInnerWidth) }
    function columnX(i) { return s2 + i * columnPitch }
}
