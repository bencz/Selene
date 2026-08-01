// --- namespace shortcuts ---
#define basic'*  = std'basic'*.
#define graph'*  = gui'graphics'*.
#define gui'*    = gui'controls'*.

#define api'*    = win32'api'*.
#define consts'* = win32'api'constants'*.

// --- Canvas ---

#class Canvas
{
    #field theHDC.
    #field theHPEN.
    #field theHBRUSH.

    #method new : aHandle
    [
        theHDC := api'HDC::aHandle.
    ]

    #method $selectPen : anHPEN
    [
        theHPEN := anHPEN $getHPEN.
    ]

    #method $selectBrush : aHBRUSH
    [
        theHBRUSH := aHBRUSH $getHBRUSH.
    ]

    #method $selectColor : aColor
    [
        theHDC $setTextColor:aColor.
    ]

    #method $selectBkColor : aColor
    [
        theHDC $setBkColor:aColor.
    ]

    #method set : anObject
    [
        anObject $select:self.
    ]

    #method graph'$draw : aFrame
    [
        #var aSize := aFrame $getSize.
        #var aDest := aFrame $getDestination.
        #var aSour := aFrame $getSource.
        #var anHDC := aFrame $getHDC:theHDC.
        #var anOperation := aFrame $getOperation.

        #external win32'59(theHDC, aDest X, aDest Y, aSize Width, aSize Height, anHDC, 
                 aSour X, aSour Y, anOperation).

        anHDC close.
    ]

    #method graph'$moveTo : aPoint
    [
        self $moveTo:aPoint.
    ]

    #method graph'$lineTo : aPoint
    [
        self $lineTo:aPoint.
    ]

    #method $moveTo : aPoint
    [
        #external win32'60(theHDC, aPoint X, aPoint Y).
    ]

    #method $lineTo : aPoint
    [
        #external win32'61(theHDC, aPoint X, aPoint Y, theHPEN).
    ]

    #method graph'$textOut : aTextXY
    [
        #var aPoint := aTextXY $getDestination | back:{ X = 0. Y = 0. }.
        #var aText := aTextXY $getText.

        #external win32'62(theHDC, aPoint X, aPoint Y, api'LPSTR::aText, aText Length).
    ]

    #method ggraph'$fillRectangle : aRectangle
    [
        #var aLeftTop := aRectangle from.
        #var aRightBottom := aRectangle till.

        #external win32'63(theHDC, aLeftTop X, aLeftTop Y, aRightBottom X, aRightBottom Y, theHBRUSH).
    ]
}

// --- Bitmap ----

#class Bitmap(api'HBITMAP)
{
    #method gui'$getSize = self $getSize.
}

// --- PenType ---

#symbol PenType =
{
    create : aProfile
    [
        ^ api'PenFactory create:aProfile.
    ]
}.

// --- BrushType ---

#symbol BrushType =
{
    create : aProfile
    [
        ^ api'BrushFactory create:aProfile.
    ]
}.

// --- ImageType ---

#symbol ImageType =
{
    create : aProfile
    [
        ^ api'BitmapType create:#annex(aProfile)
        {
            $getBitmap = Bitmap.
        }.
    ]
}.
