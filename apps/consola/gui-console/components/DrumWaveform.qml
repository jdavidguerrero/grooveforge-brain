import QtQuick
import "../theme"
Canvas {
    id: root
    property var bars: []
    property color tint: Theme.tealPlus
    onPaint: {
        let c=getContext('2d');c.reset();c.fillStyle=root.tint;
        let count=root.bars.length,pitch=width/Math.max(1,count);
        for(let i=0;i<count;i++) {
            let h=Math.max(1,height*Math.min(100,root.bars[i])/100);
            c.fillRect(i*pitch,(height-h)/2,Math.max(.5,pitch*.6),h);
        }
    }
    onBarsChanged:requestPaint()
    onTintChanged:requestPaint()
    onWidthChanged:requestPaint()
    onHeightChanged:requestPaint()
}
