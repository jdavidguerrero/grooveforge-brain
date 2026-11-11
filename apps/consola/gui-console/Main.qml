import QtQuick
import QtQuick.Window as QtWindow
import "theme"
import "components"
import "screens"
QtWindow.Window {
    id: window
    objectName: "consoleWindow"
    width: eglfsMode ? QtWindow.Screen.width : (qmlRotation % 180 ? Theme.rootHeight : Theme.rootWidth)
    height: eglfsMode ? QtWindow.Screen.height : (qmlRotation % 180 ? Theme.rootWidth : Theme.rootHeight)
    onActiveChanged: if(fakeSurface && !active) surface.panic()
    visible: true
    title: "GrooveForge Console"
    color: console.screen === 2 || console.screen === 4 ? Theme.gpBgBase : Theme.bg
    flags: frameless ? Qt.FramelessWindowHint : Qt.Window
    Item {
        id: scene
        objectName: "rotatedScene"
        width: Theme.rootWidth; height: Theme.rootHeight
        anchors.centerIn: parent
        rotation: qmlRotation
        scale: Math.min(window.width / (qmlRotation % 180 ? height : width), window.height / (qmlRotation % 180 ? width : height))
        TopBar { objectName: "topBar"; x: 0; y: 0; width: parent.width; height: Theme.topHeight; visible: console.screen !== 1 }
        Item { objectName: "content"; visible: console.screen !== 1; y: Theme.contentY; width: parent.width; height: Theme.contentHeight
            Loader { id: screenLoader; property int loadStatus: status; objectName: "screenLoader"; anchors.fill: parent; source: "screens/"+console.screenSource; visible: console.screen !== 1 && !perfMode }
        }
        Loader { x: 0; y: Theme.contentY; width: Theme.rootWidth; height: Theme.contentHeight; active: perfMode; source: "tools/PerfScene.qml" }
        Loader { x: 480; y: 450; width: 1280; height: 568; scale: .25; transformOrigin: Item.TopLeft
            active: perfMode && cycleScreens; source: "screens/"+console.screenSource
        }
        SoftKeys { objectName: "softKeys"; y: Theme.keysY; width: parent.width; height: Theme.keysHeight; visible: console.screen !== 1 }
        Loader { anchors.fill: parent; active: console.screen === 1 && !perfMode; source: "screens/Boot.qml" }
        Item {
            anchors.fill: parent; focus: true
            property var fakePadKeys: [Qt.Key_Z,Qt.Key_X,Qt.Key_C,Qt.Key_V,Qt.Key_B,Qt.Key_N,Qt.Key_M,Qt.Key_Comma,Qt.Key_A,Qt.Key_S,Qt.Key_D,Qt.Key_F,Qt.Key_G,Qt.Key_H,Qt.Key_J,Qt.Key_K,Qt.Key_Q,Qt.Key_W,Qt.Key_E,Qt.Key_R,Qt.Key_T,Qt.Key_Y,Qt.Key_U,Qt.Key_I,Qt.Key_1,Qt.Key_2,Qt.Key_3,Qt.Key_4,Qt.Key_5,Qt.Key_6,Qt.Key_7,Qt.Key_8]
            function fakePadIndex(key) { let i=fakePadKeys.indexOf(key); return 24-Math.floor(i/8)*8+i%8 }
            Keys.onReleased: function(event) { if(fakeSurface && fakePadKeys.indexOf(event.key)>=0) { if(!event.isAutoRepeat) surface.fakePad(fakePadIndex(event.key),false); event.accepted=true; return } if(event.key === Qt.Key_Shift) synth.setFine(false) }
            Keys.onPressed: function(event) {
                if(fakeSurface && fakePadKeys.indexOf(event.key)>=0) { if(!event.isAutoRepeat) surface.fakePad(fakePadIndex(event.key),true) }
                else if(fakeSurface && event.key === Qt.Key_BracketLeft) surface.transpose(-1)
                else if(fakeSurface && event.key === Qt.Key_BracketRight) surface.transpose(1)
                else if((event.modifiers & Qt.ShiftModifier) && (console.screenName === "SequencerPianoRoll" || console.screenName === "SequencerSteps") && [Qt.Key_Left,Qt.Key_Right,Qt.Key_Up,Qt.Key_Down].indexOf(event.key) >= 0) console.panRoll(event.key === Qt.Key_Left ? -1 : event.key === Qt.Key_Right ? 1 : 0, event.key === Qt.Key_Up ? 1 : event.key === Qt.Key_Down ? -1 : 0)
                else if(event.key >=  Qt.Key_1 && event.key <= Qt.Key_9) console.setScreen(event.key-Qt.Key_0)
                else if(event.key === Qt.Key_Tab) console.cycleScreen(event.modifiers & Qt.ShiftModifier ? -1 : 1)
                else if(event.key === Qt.Key_Backtab) console.cycleScreen(-1)
                else if(event.key === Qt.Key_F5) console.triggerAction('mode:3')
                else if(event.key === Qt.Key_Space && console.drumMode) { if(drum.clock.running)drum.stop();else drum.play() }
                else if(event.key === Qt.Key_F3) console.triggerAction('mode:2')
                else if(event.key === Qt.Key_F4) console.navigate('SynthSettingsAudio')
                else if(event.key === Qt.Key_Shift) synth.setFine(true)
                else if(event.key === Qt.Key_PageUp && console.screenName.indexOf('Synth') === 0) synth.nextPage(-1)
                else if(event.key === Qt.Key_PageDown && console.screenName.indexOf('Synth') === 0) synth.nextPage(1)
                else if(event.key === Qt.Key_F2) console.setScreen(2)
                else if(event.key === Qt.Key_Left && (event.modifiers & Qt.AltModifier)) touch.move(-1)
                else if(event.key === Qt.Key_Right && (event.modifiers & Qt.AltModifier)) touch.move(1)
                else if(event.key === Qt.Key_Return && (event.modifiers & Qt.AltModifier)) touch.activate()
                else if(event.key === Qt.Key_Left) console.selectEncoder(console.selectedEncoder-1)
                else if(event.key === Qt.Key_Right) console.selectEncoder(console.selectedEncoder+1)
                else if(event.key === Qt.Key_Up) { if(fakeSurface) surface.fakeEncoder(console.selectedEncoder,1); else console.turnEncoder(console.selectedEncoder,1) }
                else if(event.key === Qt.Key_Down) { if(fakeSurface) surface.fakeEncoder(console.selectedEncoder,-1); else console.turnEncoder(console.selectedEncoder,-1) }
                else if(event.key === Qt.Key_Return || event.key === Qt.Key_Enter) console.pressEncoder(console.selectedEncoder)
                else if(event.key === Qt.Key_O) console.setLiveState("offline")
                else if(event.key === Qt.Key_E) console.setLiveState("empty")
                else if(event.key === Qt.Key_L) console.setLiveState("loading")
                else if(event.key === Qt.Key_N) console.setLiveState("normal")
                else if(event.key === Qt.Key_B) console.triggerAction("encoding")
                else if(event.key === Qt.Key_Escape) console.triggerAction(console.screenName.indexOf("Drum") === 0 ? "nav:DrumRack" : console.screenName.indexOf("Synth") === 0 ? "nav:SynthParts" : "back")
                else return
                event.accepted = true
            }
        }
        Rectangle { x: 300; y: 564; width: 960; height: 48; z: 200; visible: touch.detail.length>0; color: Theme.surface2; border.color: Theme.tealPlus
            Label { anchors.fill: parent; anchors.margins: 12; text: touch.detail; font.pixelSize: 12 }
            TouchTarget { hardwareAction: "detail"; callback: function() { touch.dismiss() } }
        }
        DrumTempo {x:420;y:72;width:440;height:236;z:220;visible:console.drumMode && console.screenName.indexOf('Drum')===0 && drum.tempoVisible}
        Rectangle {
            objectName: "loadingFeedback"
            property string message: console.drumMode ? drum.loadingText : console.synthMode ? synth.loadingText : ""
            x: 400; y: 64; width: 480; height: 48; z: 230
            visible: message.length > 0; color: Theme.surface2; border.color: Theme.amber
            Label { anchors.centerIn: parent; text: parent.message; font.pixelSize: 18; color: Theme.amber }
        }
        // Visible acknowledgement is synchronized with the same frame as the models.
        Rectangle { objectName: "surfaceProbe"; visible: perfMode; x: 0; y: 718; width: 1280; height: 2
            color: Qt.rgba((surfaceBatch.serial % 256)/255, (Math.floor(surfaceBatch.serial/256) % 256)/255, .6, 1)
            property int batchSerial: surfaceBatch.serial
        }
    }
}
