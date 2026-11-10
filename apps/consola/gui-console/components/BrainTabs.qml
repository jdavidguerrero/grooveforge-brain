import QtQuick
import "../theme"
Item {
    id: root
    property bool hybrid: false
    Repeater { model: root.hybrid ? console.standalone.hybTabs : console.standalone.brainTabs
        ActionButton { required property int index; required property var entry; x: Theme.s2+index*Layout.brain.tabPitch; width: Layout.brain.tabW; height: Layout.brain.tabH
            text: entry.n; color: entry.bg; border.color: entry.bd; tint: entry.fg
            action: entry.n === 'ENGINE' ? 'nav:SynthParts' : entry.n === 'HYBRID' ? 'nav:SynthHybrid' : entry.n === 'ENV · LFO' ? 'nav:SynthEnv' : entry.n === 'PRESETS' ? 'nav:SynthPresets' : ['OSC','FILTER','ENV','LFO','FX'].indexOf(entry.n) >= 0 ? 'nav:Synth'+entry.n.charAt(0)+entry.n.slice(1).toLowerCase() : ''
        }
    }
}
