import QtQuick
import "../theme"
Item {
    id: root
    property bool hybrid: false
    Repeater { model: root.hybrid ? console.standalone.hybTabs : console.standalone.brainTabs
        ActionButton { required property int index; required property var entry; x: Theme.s2+index*Layout.brain.tabPitch; width: Layout.brain.tabW; height: Layout.brain.tabH
            text: entry.n; color: entry.bg; border.color: entry.bd; tint: entry.fg
            action: entry.n === 'ENGINE' ? 'nav:BrainEngine' : entry.n === 'HYBRID' ? 'nav:BrainHybrid' : ''
        }
    }
}
