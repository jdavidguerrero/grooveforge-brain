import QtQuick
import QtQuick.Window as QtWindow
import "theme"
import "components"
import "screens"
QtWindow.Window {
    id: window
    objectName: "consoleWindow"
    width: Theme.rootWidth; height: Theme.rootHeight
    minimumWidth: Theme.rootWidth; maximumWidth: Theme.rootWidth
    minimumHeight: Theme.rootHeight; maximumHeight: Theme.rootHeight
    visible: true
    title: "GrooveForge Console · Phase 2"
    color: console.screen === 2 || console.screen === 4 ? Theme.gpBgBase : Theme.bg
    flags: frameless ? Qt.FramelessWindowHint : Qt.Window
    TopBar { objectName: "topBar"; x: 0; y: 0; width: parent.width; height: Theme.topHeight; visible: console.screen !== 1 }
    Item { objectName: "content"; visible: console.screen !== 1; y: Theme.contentY; width: parent.width; height: Theme.contentHeight
        Loader { id: screenLoader; property int loadStatus: status; objectName: "screenLoader"; anchors.fill: parent; source: "screens/"+console.screenSource; visible: console.screen !== 1 }
    }
    SoftKeys { objectName: "softKeys"; y: Theme.keysY; width: parent.width; height: Theme.keysHeight; visible: console.screen !== 1 }
    Loader { anchors.fill: parent; active: console.screen === 1; source: "screens/Boot.qml" }
    Item {
        anchors.fill: parent; focus: true
        Keys.onPressed: function(event) {
            if((event.modifiers & Qt.ShiftModifier) && (console.screenName === "SequencerPianoRoll" || console.screenName === "SequencerSteps") && [Qt.Key_Left,Qt.Key_Right,Qt.Key_Up,Qt.Key_Down].indexOf(event.key) >= 0) console.panRoll(event.key === Qt.Key_Left ? -1 : event.key === Qt.Key_Right ? 1 : 0, event.key === Qt.Key_Up ? 1 : event.key === Qt.Key_Down ? -1 : 0)
            else if(event.key >=  Qt.Key_1 && event.key <= Qt.Key_9) console.setScreen(event.key-Qt.Key_0)
            else if(event.key === Qt.Key_Tab) console.cycleScreen(event.modifiers & Qt.ShiftModifier ? -1 : 1)
            else if(event.key === Qt.Key_Backtab) console.cycleScreen(-1)
            else if(event.key === Qt.Key_F2) console.setScreen(2)
            else if(event.key === Qt.Key_Left) console.selectEncoder(console.selectedEncoder-1)
            else if(event.key === Qt.Key_Right) console.selectEncoder(console.selectedEncoder+1)
            else if(event.key === Qt.Key_Up) console.turnEncoder(console.selectedEncoder,1)
            else if(event.key === Qt.Key_Down) console.turnEncoder(console.selectedEncoder,-1)
            else if(event.key === Qt.Key_Return || event.key === Qt.Key_Enter) console.pressEncoder(console.selectedEncoder)
            else if(event.key === Qt.Key_O) console.setLiveState("offline")
            else if(event.key === Qt.Key_E) console.setLiveState("empty")
            else if(event.key === Qt.Key_L) console.setLiveState("loading")
            else if(event.key === Qt.Key_N) console.setLiveState("normal")
            else if(event.key === Qt.Key_B) console.triggerAction("encoding")
            else if(event.key === Qt.Key_Escape) console.triggerAction("back")
            else return
            event.accepted = true
        }
    }
}
