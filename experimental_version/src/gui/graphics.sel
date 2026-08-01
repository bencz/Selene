// --- namespace shortcuts ---
#define basic'*   = std'basic'*.
#define prop'*  = std'properties'*.
#define list'*    = std'collections'*.

#define graph'*   = gui'graphics'*.

// --- compiler hints ---
#define[const] IndexerExtension = prop'IndexerExtension.
#define[const] ListExtension    = list'ListExtension.

// === Common types ===

// --- Point ---

#class Point
{
    #method $getCoord = $self.

	#method - aPoint
	[
        #var aCoord := aPoint $getCoord.

		^ graph'PointVar << { $getCoord = $self. X = self X - aCoord X | back:0. Y = self Y - aCoord Y | back:0. }.
	]

	#method + aPoint
	[
        #var aCoord := aPoint $getCoord.

		^ graph'PointVar << { $getCoord = $self. X = self X + aCoord X | back:0. Y = self Y + aCoord Y | back:0. }.
	]
}

// --- Size ---

#class Size
{
    #method $getSize = $self.
}

// --- PointVar ---

#class PointVar (Point)
{
    #field theX.
    #field theY.

    #method X = basic'IntNumber::theX.

    #method Y = basic'IntNumber::theY.

    #method new : aPoint
    [
        theX := basic'Integer.
        theY := basic'Integer.

        #if aPoint ifNotNil
        [
            #var aCoord := aPoint $getCoord.

            theX << aCoord X.
            theY << aCoord Y.
        ].
    ]

    #method << aPoint
    [
        #var aCoord := aPoint $getCoord.

        theX << aCoord X.
        theY << aCoord Y.
    ]

    #method += aPoint
    [
        #var aCoord := aPoint $getCoord.

        theX += aCoord X.
        theY += aCoord Y.
    ]

    #method -= aPoint
    [
        #var aCoord := aPoint $getCoord.

        theX -= aCoord X.
        theY -= aCoord Y.
    ]
}

// --- Rectangle ---

#class Rectangle
{
    #field theLeft.
    #field theTop.
    #field theWidth.
    #field theHeight.

    #method X = theLeft.

    #method Y = theTop.

    #method Width = theWidth.

    #method Height = theHeight.

    #method new : aRectangle
    [
        theLeft := basic'Integer << (aRectangle X | back:0).
        theTop := basic'Integer << (aRectangle Y | back:0).
        theWidth := basic'Integer << (aRectangle Width | back:0).
        theHeight := basic'Integer << (aRectangle Height | back:0).
    ]

    #method isValid : aPoint
    [
        #var aCoord := aPoint $getCoord.

        #var anX := aCoord X.
        #var anY := aCoord Y.

        ^ (theLeft <= anX)and:(theLeft + theWidth > anX)and:(theTop <= anY)and:(theTop + theHeight > anY).
    ]

    #method From = Point{ X = basic'IntNumber::theLeft. Y = basic'IntNumber::theTop. }.

    #method Till = Point{ X = theLeft + theWidth - 1. Y = theTop + theHeight - 1. }.
}

// --- SizeVar ---

#class SizeVar (Size)
{
    #field theWidth.
    #field theHeight.

    #method Width = basic'IntNumber::theWidth.

    #method Height = basic'IntNumber::theHeight.

    #method new : aSize
    [
        theWidth := basic'Integer.
        theHeight := basic'Integer.

        #if aSize ifNotNil
        [
            #var aCoord := aSize $getSize.

            theWidth << aCoord Width.
            theHeight << aCoord Height.
        ].
    ]

    #method << aSize
    [
        #var aCoord := aSize $getSize.

        theWidth << aCoord Width.
        theHeight << aCoord Height.
    ]
}

// --- PointType ---

#symbol PointType =
{
    of : anObject = anObject $getCoord.
}.

// --- SizeType ---

#symbol SizeType =
{
    of : anObject = anObject $getSize.
}.

// === Common graphic types ===

// --- Image ---

#class Image
{
    #field theImage.

    #method new : anImage
    [
        theImage := anImage.
    ]

    #annex theImage.
}

// --- Pen ---

#class Pen
{
    #field thePen.

    #method new
    [
        #var aWidth := self $getWidth | back:1.
        #var aColor := self $getColor | back:0.

        thePen := 'gui'graphics'penType create:#group('gui'styles'Width::aWidth, 'gui'styles'Color::aColor).
    ]

    #annex thePen.
}

// --- Brush ---

#class Brush
{
    #field theBrush.

    #method new
    [
        #var aColor := self $getColor.

        theBrush := 'gui'graphics'brushType create:'gui'styles'Color::aColor.
    ]

    #annex theBrush.
}

// === Controls ===

// --- ImageList ---

#static ImageListGroup = #group(ListExtension, IndexerExtension).

#class ImageList
{
    #field theParent.
    #field theList.

    #method new : aParent
    [
        theParent := aParent.
        theList := list'List.
    ]

    #method += aFileName
    [
        theList += Image::('gui'imageType create:'gui'styles'FileName::aFileName).
    ]

    #method prop'$getIndexer = theList $invoke.

    #annex ImageListGroup.
}

// === Operartion patterns ===

// --- Drawing ---

#class Drawing
{
    #field theCanvas.
    #field theCoord.

    #method $getCoord = theCoord.

    #method new : aCanvas
    [
        theCanvas := aCanvas.
        theCoord := PointVar.
    ]

    #method << aCoord
    [
        theCoord << aCoord.
    ]

    #method += aCoord
    [
        theCoord += aCoord.
    ]

    #method -= aCoord
    [
        theCoord -= aCoord.
    ]

    #method proceed : aFigure
    [
        aFigure identify:self.
    ]

    #method $ifImage : anImage
    [
        #var aDestination := 'gui'styles'Destination::theCoord.
        #var aCopy := 'gui'styles'ImageCopy::anImage.

        theCanvas $draw:#annex((aCopy, aDestination)) {}.
    ]

    #method $ifRectangle : aRectange
    [
        theCanvas $fillRectangle:aRectange.
    ]

    #method basic'$ifLiteral : aLiteral
    [
        #var aDestination := 'gui'styles'Destination::theCoord.
        #var aText := 'gui'styles'Text::aLiteral.

        theCanvas $textOut:#annex((aText, aDestination)) {}.
    ]
}

// --- Plotter ---

#class Plotter
{
    #field theCanvas.

    #method new : aCanvas
    [
        theCanvas := aCanvas.
    ]

    #method << aCoord
    [
        theCanvas $moveTo:(aCoord $getCoord).
    ]

    #method proceed : aCoord
    [
        theCanvas $lineTo:(aCoord $getCoord).
    ]
}
