// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define ctrl'*   = std'patterns'*.

// === Common base classes ===

// --- EnumIndexerProxy ---

#class EnumIndexerProxy
{
    #field theEnumerator.
    #field theIndex.
    #field theCollection.

    #role BOF
    {
        #method $getContent 
        [
            self fail.
        ]
    
        #method $setContent : anObject
        [
            self fail.
        ]    
    
        #method $next        
        [
            self fail.
        ]
    
        #method $previous
        [
            self fail.
        ]

        #method $goto : anIndex
        [
            #if basic'True if:(0 == anIndex) if:(theCollection Count == 0)
            [
                theEnumerator end.

                #shift.
            ]
            | [
                theEnumerator start.
    
                #shift.
    
                self $invoke:anIndex.
            ].
        ]
    }

    #role EOF
    {
        #method $getContent 
        [
            self fail.
        ]
    
        #method $setContent : anObject
        [
            self fail.
        ]    

        #method $next        
        [
            self fail.
        ]
    
        #method $previous
        [
            self fail.
        ]

        #method $goto : aValue
        [
            #if(aValue < theCollection Count)?
            [
                #shift.

                self $goto:aValue.
            ]
            | [ self fail. ].
        ]
    }

    // private methods

    #method $getIndex = theIndex.

    #method $getContent = theEnumerator get.

    #method $setContent : anObject
    [
        theEnumerator set:anObject.
    ]

    #method $goto : aValue
    [
        #if(aValue==theCollection Count)?
        [
            theIndex << aValue.

            #shift EOF.
            theEnumerator end.

            ^ self.
        ].

        #if(theIndex > aValue)?
        [
            theEnumerator start.
            theIndex << 0.
        ].

        #loop theIndex < aValue ?
        [
            theEnumerator next.
            theIndex += 1.
        ].
    ]

    #method $next
    [
        theIndex += 1.

        #if(theIndex==theCollection Count)?
        [
            #shift EOF.

            theEnumerator end.

            self fail.
        ]
        | ![ theEnumerator next. ].
    ]

    #method $previous
    [
        self $goto:(theIndex - 1).
    ]

    // public methods

    #method new : anEnumerable
    [
        theCollection := anEnumerable.
        theEnumerator := anEnumerable $getEnumerator.
        theIndex := basic'Integer << 0.

        #shift BOF.
    ]

    #annex (theEnumerator get).
}

// --- PositionCriteria --- 

#class PositionCriteria
{
    #method $seekin : anIndexer
    [
        anIndexer $goto:(self Position).
    ]
}

// --- ValueCritreia ---

#class ValueCriteria
{
    #method $seekin : anIndexer
    [
        anIndexer $seek:(self Value).
    ]
}

// --- ArrayIterator ---

#symbol ArrayIterator =
{
    ctrl'$getIterator = self $getIndexer.

    ctrl'$getCurrent : anIndexer = anIndexer.

    ctrl'$step : anIndexer
    [
        anIndexer $next.
    ]
}.

// --- ArrayBIterator ---

#symbol ArrayBIterator =
{
    ctrl'$getIterator = self $getIndexer.

    ctrl'$getCurrent : anIndexer = anIndexer.

    ctrl'$step : anIndexer
    [
        anIndexer $previous.
    ]
}.

// === Common properties ===

// --- Enumerator ---

#class Enumerator
{
    #field theProxy.

    #role BOF
    {
        #method next
        [
            theProxy start.

            #shift.
        ]

        #method start
        [
            self next.
        ]
    }

    // public
    #method new : aCollection
    [
        theProxy := aCollection $getEnumerator.

        #shift BOF.
    ]

    #method run : anAction
    [
        #loop self next
        [
            #if(ctrl'Break == anAction proceed:(theProxy get))?
                [ ^ anAction. ].
        ].

        ^ anAction.
    ]

    #annex theProxy.
}

// --- TextWriter ---

#class TextWriter
{
    #field theAdapter.

    // class methods
    #method[def] $getClassName = "std'properties'TextWriter".

    #method new : aStream
    [
        theAdapter := aStream $getTextWriter.
    ]

    #method << aPrintable
    [
        aPrintable $printOn:self.
    ]

    #annex theAdapter.
}

// --- TextReader ---

#class TextReader
{
    #field theAdapter.

    // class methods
    #method[def] $getClassName = "std'properties'TextReader".

    #method new : aStream
    [
        theAdapter := aStream $getTextReader.
    ]

    #method >> aReadable
    [
        ^ aReadable $input:self.
    ]

    #annex theAdapter.
}

// --- Reader ---

#class Reader
{
    #field theAdapter.

    // class methods
    #method[def] $getClassName = "std'properties'Reader".

    #method new : aStream
    [
        theAdapter := aStream $getReader.
    ]

    #method >> anObject
    [
        ^ anObject $readFrom:self.
    ]

    #annex theAdapter.
}

// --- Writer ---

#class Writer
{
    #field theAdapter.

    // class methods
    #method[def] $getClassName = "std'properties'Writer".

    #method new : aStream
    [
        theAdapter := aStream $getWriter.
    ]

    #method << anObject
    [
        anObject $writeTo:self.
    ]

    #annex theAdapter.
}

// --- Indexer ---

#class Indexer
{
    #field theProxy.

    // class
    #method[def] $getClassName = "std'properties'Indexer".

    // public
    #method new : anArray
    [
        theProxy := anArray $getIndexer.
    ]

    #method Position = theProxy $getIndex.

    #method get = theProxy.

    #method set : anObject
    [
        theProxy $setContent:anObject.
    ]

    #method seek : aCriteria
    [
        aCriteria $seekIn:theProxy.
    ]

    #method insert : anObject
    [
        theProxy $insert:anObject.
    ]

    #method delete : aNumber
    [
        theProxy $delete:(aNumber Count).
    ]

    #method next
    [
        theProxy $next.
    ]

    #method previous
    [
        theProxy $previous.
    ]

    #method start []
}

// --- Content ---

#class Content
{
    #field theProxy.

    #method new : aProxy
    [
        theProxy := aProxy.
    ]

    #method get = theProxy $getContent.

    #method set : aValue 
    [ 
        theProxy $setContent:aValue. 
    ]

    #method exchange : aProxy
    [
        #var aTemp := aProxy $getContent.

        aProxy $setContent:(theProxy $getContent).

        theProxy $setContent:aTemp.
    ] 
}

// --- Index ---

#symbol Index : anIndexer
    = anIndexer $getIndex clone.

// --- ContentValue ---

#symbol ContentValue : anObject
    = anObject $getContent.

// --- ContentKey ---

#symbol ContentKey : anObject
    = anObject $getKey.

// --- KeyValue ---

#class KeyValue
{
    #field theKey.
    #field theValue.

    #method new : aKeyValue
    [
        theKey := aKeyValue $getKey | back:'nil.
        theValue := aKeyValue $getContent.
    ]

    #method $getKey = theKey.
    
    #method $getContent = theValue.

    #annex (theValue).
}

// --- ContentProxy ---

#class ContentProxy
{
    #field theContainer.

    #method new : aContainer
    [
        theContainer := aContainer.
    ]

    #method $getContent = theContainer get.

    #method $setContent : anObject
    [
        theContainer set:anObject.
    ]

    #annex (theContainer get).
}

// --- Range ---

#class Range
{
    #field theRange.

    #method new : anObject
    [
        theRange := anObject $getRange.
    ]

    #method isValid : aValue
    [
        ^ (theRange from <= aValue) and:(theRange till >= aValue).
    ]
    
    #annex (theRange).
}

// --- IndexEnumerator ---

#class IndexEnumerator
{
    #field theProxy.
    #field theRange.

    #role BOF
    {
        #method start
        [
            self next.
        ]

        #method next
        [
            theProxy $goto:(theRange From).

            #shift.
        ]
    }

    #method new : anArray
    [
        theProxy := anArray $getIndexer.
        theRange := anArray $getRange.

        #shift BOF.
    ]

    #method clone = IndexEnumerator::{ $getIndexer = theProxy clone. }.

    #method start
    [
        #shift BOF.

        self start.
    ]
    
    #method next
    [
        theProxy $next.
    ]

    #method seek : aValue
    [
        theProxy $seek:(aValue Value)
            | $goto:(aValue Position).
    ]

    #method insert : aValue
    [
        theProxy $insert:aValue.
    ]

    #method delete : aNumber
    [
        theProxy $delete:(aNumber Count).
    ]

    #method run : anAction
    [
        #loop self next
        [
            #if(ctrl'Break == anAction proceed:theProxy)?
                [ ^ anAction. ].
        ].

        ^ anAction.
    ]
}

// === Common extensions ===

// --- EEnumerator ---

#symbol EEnumerator =
{
    ctrl'$getCurrent : anEnumerator = anEnumerator get.

    ctrl'$getIterator = self $getEnumerator start.

    ctrl'$step : anEnumerator [ anEnumerator next. ]
}.

// --- EContent ---

#symbol EContent = 
{
    get = self $getContent.

    set : aValue 
    [ 
        self $setContent:aValue. 
    ]

    exchange : aProxy
    [
        #var aTemp := aProxy $getContent.

        aProxy $setContent:(self $getContent).

        self $setContent:aTemp.
    ] 
}.

// --- IndexerExtension ---

#symbol IndexerExtension =
{
    @ anIndex
    [
        ^ self $getIndexer $goto:anIndex.
    ]
}.

#symbol ERange =
{
    in : aValue
    [
        ^ (self from <= aValue) and:(self till >= aValue).
    ]
}.
