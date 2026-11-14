import QtQuick
QtObject {
    id: root
    property url source
    readonly property string name: host.item ? host.item.name : ""
    readonly property int status: host.item ? host.item.status : FontLoader.Null
    property Component definition: Component { FontLoader { source: root.source } }
    property Loader host: Loader { active: root.source.toString().length > 0; sourceComponent: root.definition }
}
