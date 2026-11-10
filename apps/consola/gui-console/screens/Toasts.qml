import QtQuick
import "../theme"
import "../components"
Item {
    LiveSession { anchors.fill: parent }
    Repeater { model: console.toasts
        Toast { required property int index; required icon; required message; required value; required tone
            x: Theme.toastX; y: index === 0 ? Theme.toastOlderY : Theme.toastY
            opacity: index === 0 ? Theme.mutedToastOpacity : 1
        }
    }
}
