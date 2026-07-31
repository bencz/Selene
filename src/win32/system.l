// --- namespace shortcuts ---
#define basic'*  = std'basic'*.
#define prop'*   = std'properties'*.
#define events'* = sys'events'*.
    
// --- GUISystem ---

#class $GUI
{
    #field theReferencePool.
    #field theLastReference.

    #field theEvents.

    // class methods

    #method[def] $getClassName = "win32'system'$GUI".

    #method new
    [
        theEvents := events'Controller.

        theReferencePool := basic'ArrayType create:{ Length = 20h. }.
        theLastReference := basic'Integer << 0.
    ]

    #method run : aProgram
    [
        theEvents $onStart:aProgram.

        #if aProgram proceed:self
            | [ theEvents $onError. ].

        theEvents $onStop:aProgram.
    ]


    #method $allocate : anObject
    [
        #external win32'55(theReferencePool, anObject, theLastReference).
    ]

    #method += anObject
    [
        #if self $allocate:anObject
        | [
            theLastReference << 0.

            #if self $allocate:anObject
            | [
                #var aPool := basic'ArrayType create:{ Length = theReferencePool Length + 10h. }.
                #var anIndexer := prop'Indexer::aPool.
            
                prop'IndexEnumerator::theReferencePool run: 
                    anItem => [  prop'Content::(aPool @ prop'Index::anItem) set:prop'ContentValue::anItem. ].
                    //anItem => [ anIndexer << anItem->vprop'Index. anIndexer set:anItem->vprop'Content::get. ].

                theReferencePool := aPool.

                self $allocate:anObject.
            ].
        ].

        ^ theLastReference clone.
    ]

    #method -= anObject
    [
        #external win32'56(theReferencePool, anObject).
    ]

    #method events'$getController = theEvents.
}

#static GUI = $GUI.
