// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.
#define list'*  = std'collections'*.
#define ctrl'*  = std'patterns'*.

// --- compiler hints ---
#define[const] ListExtension    = list'ListExtension.

// === collection items ===

// --- Item ---

#class Item
{
    #field theObject.
    #field theNext.

    #method prop'$getContent = theObject.

    #method prop'$setContent : anObject
    [
        theObject := anObject.
    ]

    #method $getObject = theObject.

    #method $getNext = theNext.

    #method $setNext : aNext
    [
        theNext := aNext.
    ]

    #method new : anObject
    [
        theObject := anObject.
        theNext := nil.
    ]

    #annex theObject.
}

// === collection classes ===

// --- List ---

#static ListGroup = #group(ListExtension, prop'IndexerExtension).

#class List
{
    #field theTop.
    #field theTale.

    #role Empty
    {
        #method $getTop = nil.

        #method $getTale = nil.

        #method += anObject
        [
            theTop := Item::anObject.
            theTale := theTop.

            #shift.
        ]
    }

    // class methods

    #method[def] $getClassName = "std'collections'List".

    // private methods

    #method $getList = $self.

    #method $getTop = theTop.

    #method $getTale = theTale.

    #method $setTop : anItem
    [
        theTop := anItem.

        #if anItem ifNotNil []
        | [
            #shift Empty.
        ].
    ]

    #method prop'$getEnumerator = list'Enumerator::self.

    // public methods

    #method new
    [
        #shift Empty.
    ]

    #method += anObject
    [
        theTale $setNext:Item::anObject.
        theTale := theTale $getNext.
    ]

    #annex ListGroup.
}

// --- Circle ---

#class Circle(List)
{
    #role Empty
    {
        #method $getTale = self fail.

        #method += anObject
        [
            theTop := Item::anObject.
            theTale := theTop.

            theTale $setNext:theTop.

            #shift.
        ]
    }

    // class methods

    #method[def] $getClassName = "std'collections'Circle".

    // private methods

    #method $getCircle = $self.

    #method $getTale = theTale.

    #method $shiftRight
    [
        theTale := theTop.
        theTop := theTop $getNext.
    ]

    #method shift : anIndex
    [
        basic'True if:(anIndex >= 0).

        ctrl'DecrimentLoop::anIndex run: =>
        [
            self $shiftRight.
        ].
    ]

    #method += anObject
    [
        theTale $setNext:Item::anObject.
        theTale := theTale $getNext.

        theTale $setNext:theTop.
    ]    

    #method prop'$getEnumerator = list'CircleEnumerator::self.
}

// === Collection adapters ===

// --- Indexer ---

#class Indexer (prop'EnumIndexerProxy)
{
    #method prop'$insert : aValue
    [
        theEnumerator insert:aValue.
    ]

    #method prop'$delete : aNumber
    [
        theEnumerator delete:{ Count = aNumber. }.
    ]
}

// --- Enumerator ---

#class Enumerator
{
    #field theList.
    #field theCurrent.
    #field thePrevious.

    #role EOF
    {
        #method get = self fail.

        #method set = self fail.

        #method next
        [
            self fail.
        ]

        #method insert : anObject
        [
            theList += anObject.

            theCurrent := theList $getTale.

            #shift.
        ]

        #method delete
        [
            self fail.
        ]
    }

    #role BOF
    {
        #method insert : anObject
        [
            theCurrent := Item::anObject $setNext:(theList $getTop).

            theList $setTop:theCurrent.
        ]

        #method delete : aNumber
        [
            ctrl'DecrimentLoop::(aNumber Count) run: =>
            [
                theList $setTop:(self $deleteCurrent).
            ].
        ]

        #method next
        [   
            #shift.

            self next.
        ]
    }

    #method $deleteCurrent
    [
        theCurrent := theCurrent $getNext.

        ^ theCurrent.
    ]

    #method new : aCollection
    [
        theList := aCollection $getList.
    ]

    #method start
    [
        thePrevious := nil.
        
        theCurrent := theList $getTop ifNotNil.

        #shift BOF.
    ]

    #method next
    [
        thePrevious := theCurrent.
        theCurrent := theCurrent $getNext ifNotNil.
    ]

    #method end
    [
        thePrevious := theList $getTale.

        #shift EOF.
    ]

    #method set : anObject
    [
        theCurrent new:anObject.
    ]

    #method get = theCurrent $getObject.

    #method insert : anObject
    [
        #var anItem := Item::anObject $setNext:theCurrent.

        thePrevious $setNext:anItem.
        theCurrent := anItem.
    ]

    #method delete : aNumber
    [
        ctrl'DecrimentLoop::(aNumber Count) run: =>
        [
            thePrevious $setNext:(self $deleteCurrent).
        ].
    ]
}

// --- CircleEnumerator ---

#class CircleEnumerator (Enumerator)
{
    #method start
    [
        thePrevious := theList $getTale.
        theCurrent := theList $getTop ifNotNil.

        #shift BOF.
    ]

    #method next
    [
        theCurrent := theCurrent $getNext.

        #if theCurrent ifSame:(theList $getTop)
            [ self fail. ].
    ]
}

// === Extensions ===

// --- ListExtension ---

#symbol ListExtension =
{
    Count = ctrl'Counter::self.

    prop'$getIndexer = list'Indexer::self.

    prop'$printOn : aWriter
    [
        'collection'ListPrinter::self $invoke:aWriter.
    ]
}.

// --- ReadOnlyEnumerator ---

#symbol ReadOnlyEnumerator : anEnumerator = #annex(anEnumerator)
{
    set = $self fail.

    delete = $self fail.

    insert = $self fail.
}.

// --- ReadOnly ---

#symbol ReadOnly : aList = #annex(aList)
{
    += aParam = $self fail.

    prop'$getEnumerator = ReadOnlyEnumerator::(aList $invoke).
}.
