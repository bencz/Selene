// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.
#define ctrl'*  = std'patterns'*.

// --- BSort ---

#symbol BSort =
{
    $sortDown : aRange
    [
        #var anArray := aRange for.
        #var aRoot   := aRange from.
        #var aBottom := aRange till.

        #loop aRoot * 2 < aBottom ?
        [
            #var aMaxChild := nil.
            #var aChild := (aRoot * 2) + 1.

            #if basic'False if:(aChild == aBottom) if:(anArray@aChild > (anArray@(aChild + 1)))
                [ aMaxChild := aChild + 1. ] // false part
                | [ aMaxChild := aChild. ].  // true part

            #var aRootItem := anArray@aRoot.
            #var aMaxChildItem := anArray@aMaxChild.

            #if(aRootItem < aMaxChildItem)?
                [                    
                    prop'Content::aRootItem exchange:aMaxChildItem.

                    aRoot := aMaxChild.
                ]
                | [ ^ basic'false. ].
        ].
    ]
 
    $sort : anArray
    [
        #var aLastIndex := anArray Length - 1.

        #group(ctrl'EIteration, anArray @ (anArray Length / 2 - 1), prop'ArrayBIterator) run:
            anItem => [
                #var aCurrent := prop'Index::anItem.
    
                BSort $sortDown:
                {
                    for = anArray. 
                    from = aCurrent. 
                    till = aLastIndex.
                }.
            ].
    
        #group(ctrl'EIteration, anArray @ aLastIndex, prop'ArrayBIterator) run:
            anItem => [
                #var aCurrent := prop'Index::anItem.
    
                prop'Content::anItem exchange:(anArray@0).
    
                BSort $sortDown:
                {
                    for = anArray. 
                    from = 0. 
                    till = aCurrent - 1.
                }.
            ].
    ]
}.

// === Program ===

#symbol Program =>
[
    // 1. Create the array
    #var aSize := ext'io'console << "Enter the array size:" >> basic'Integer.

    #var anArray := basic'ArrayType create:{ Length = aSize. }.
    anArray := #annex(anArray)
    { 
        $sort = BSort $sort:anArray. 
    }.

    // 2. Initialize the array
    #group(ctrl'EIteration, prop'ArrayIterator, anArray@0) run:
        anItem => (prop'Content::anItem set:(ext'io'console << "Enter the array item:" >> basic'Integer)).

    // 3. Sort the array
    anArray $sort.
	
    // 4. Print the array
    'program'Output << { prop'$printOn : aWriter [ ext'io'ListPrinter::anArray $invoke:aWriter. ] }.

    'program'input get. // wait for any key
].
