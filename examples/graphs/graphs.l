#define basic'*  = std'basic'*.
#define ctrl'*   = std'patterns'*.
#define math'*   = std'basic'math'*.

#define forms'*    = gui'forms'*.
#define controls'* = gui'controls'*.
#define graph'*    = gui'graphics'*.
#define gui'prop'* = gui'controls'properties'*.

#define events'*   = sys'events'*.

#symbol ScaleX   = 15.
#symbol ScaleY   = 15.
#symbol starting = 0.0r.
#symbol ending   = math'Pi * 6.
#symbol H        = 0.005r.
#symbol A        = 5.0r.

#symbol Function : t = ctrl'Control run: =>
[
    #var c := 2.0r / 3.0r.

    #var anX := A*((c * math'cos::t)+(math'cos::(c*t))).
    #var anY := A*((c * math'sin::t)-(math'sin::(c*t))).

    ^ { X = anX. Y = anY. }.
].

#class Window (forms'SDIDialog)
{
    #field theImagebox.
    #field theZeroX.
    #field theZeroY.

    #method $value2Coord : aValue
    [
        #var x := theZeroX + math'Rounded::(aValue X * ScaleX).
        #var y := theZeroY - math'Rounded::(aValue Y * ScaleY).

        ^ graph'Point { X = theZeroX + math'Rounded::(aValue X * ScaleX). Y = theZeroY - math'Rounded::(aValue Y * ScaleY). }.
    ]

    #method $drawAxis : aCanvas
    [
        #var aSize := graph'SizeType of:gui'prop'Size::theImagebox.
        theZeroX :=  aSize Width / 2.
        theZeroY := aSize Height / 2.

//        #var aRegion := { ggraph'$Left = 0. ggraph'$Top = 0. ggraph'$Width = aSize::X. ggraph'$Height = aSize::Y. }->ggraph'Rectangle.

        #var aPloter := graph'Plotter::aCanvas.
        #var aPrinter := graph'Drawing::aCanvas. //aCanvas-> { ggraph'$Region = aRegion. }.

        // X axis
        aPloter << graph'Point { X = 0. Y = theZeroY. }.
        aPloter proceed:graph'Point { X = aSize Width. Y = theZeroY. }.
        
        aPrinter << graph'Point { X = aSize Width - 20. Y = theZeroY + 2. }.
        aPrinter proceed:"X".

        // Y axis
        aPloter << graph'Point { X = theZeroX. Y = 0. }.
        aPloter proceed:graph'Point { X = theZeroX. Y = aSize Height. }.

        aPrinter << graph'Point { X = theZeroX + 2. Y = 2. }.
        aPrinter proceed:"Y".

        ctrl'RangeEnumerator { for = basic'Integer. from = -10. till = 10. step : anItem [ anItem += 2. ] } 
            run: aCurrent =>
            [
                #if (aCurrent != 0)?
                [
                    #var anStr := basic'LiteralType of:basic'Variant::aCurrent.
                    
                    aPrinter << graph'Point { X = theZeroX + (aCurrent * ScaleX) - 4. Y = theZeroY + 2. }.
                    aPrinter proceed:anStr.
    
                    aPrinter << graph'Point { X = theZeroX + 8. Y = theZeroY - (aCurrent * ScaleY) - 2. }.
                    aPrinter proceed:anStr.
                ]
                | [  
                    aPrinter << graph'Point { X = theZeroX + 2. Y = theZeroY + 2. }.
                    aPrinter proceed:"0".
                ].
            ].
    ]

    #method $drawGraph : aCanvas
    [
        #var aPloter := graph'Plotter::aCanvas.

        #var t := starting.
        #var t2 := ending.
        #var aZero := graph'Point { X = theZeroX. Y = 0 - theZeroY. }.

        aPloter << (self $value2Coord:Function::t).
        #loop(t <= t2)?
        [
            aPloter proceed:(self $value2Coord:Function::t).

            t := t + H.
        ].
    ]

    #method $onRender : aCanvas
    [
        aCanvas set:graph'Pen{ graph'$getColor = 0FFFFFFh. }.
        aCanvas set:graph'Brush{ graph'$getColor = 0. }.

        self $drawAxis:aCanvas $drawGraph:aCanvas.
    ]

    #method forms'$initialize
    [
        theImagebox := controls'Imagebox::self.

        events'Handlers::theImagebox +=
            { controls'$onRender : aHandle = self $onRender:aHandle. }.

        gui'prop'Caption::self << "Graph sample".
        gui'prop'Size::self << graph'Size { Width = 400. Height = 420. }.
        gui'prop'Location::self <<  graph'Point { X = 50. Y = 50. }.

        gui'prop'Size::theImagebox << graph'Size { Width = 400. Height = 400. }.
        gui'prop'Location::theImagebox << graph'Point { X = 0. Y = 0. }.
        gui'prop'Visible::theImagebox << basic'True.

        super $invoke.
    ]
}