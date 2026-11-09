import QtQuick
import "../theme"
Item {
    id: root
    Repeater { model: console.standalone.kbd
        Item { required property int index; required property var entry
            x: entry.pad; y: index*(Layout.search.keyH+Layout.search.gap); width: root.width-2*x; height: Layout.search.keyH
            property real units: entry.keys.reduce((sum,k)=>sum+k.f,0)
            Repeater { model: entry.keys
                Panel { required property int index; required property var modelData
                    x: {let units=0;for(let i=0;i<index;i++)units+=entry.keys[i].f;return units*(parent.width-(entry.keys.length-1)*Layout.search.gap)/parent.units+index*Layout.search.gap}
                    width: modelData.f*(parent.width-(entry.keys.length-1)*Layout.search.gap)/parent.units; height: parent.height; color: modelData.bg
                    Label { anchors.centerIn: parent; text: modelData.k; color: modelData.fg; font.pixelSize: Theme.m18 }
                    MouseArea { anchors.fill: parent; onClicked: console.typeKey(modelData.k) }
                }
            }
        }
    }
}
