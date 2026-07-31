#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.

// --- TextBuilder ---

#class TextBuilder
{
    #field theText.

    #role Empty
    {
        #method += aValue
        [
            theText += aValue.

            #shift.
        ]
    }

    #role Stopped
    {
        #method $ifNotStopped [ self fail. ]
    }

    #method new
    [
        theText := basic'String.

        #shift Empty.
    ]

    #method $ifNotStopped []

    #method << aValue
    [
        self += aValue.
    ]

    #method += aValue
    [
        #var aLiteral := basic'LiteralType of:aValue.

        #if(aLiteral Length > 0)?
        [
            theText += " " += aLiteral.
        ]
        | [ #shift Stopped. ].
    ]

    #annex (theText).
}

// --- Program ---

#symbol Program =>
[
    'program'output << "Enter the text(to stop press enter two times):%n".

    #var aText := TextBuilder.
    #loop 'program'Input >> aText $ifNotStopped.

    #var aSearchText := 'program'console << "Enter the phrase to be found:" >> basic'String.
    #var aReplaceText := 'program'console << "Enter the phrase to replace with:" >> basic'String.
    
    #var aCriteria := prop'ValueCriteria { Value = aSearchText. }.

    #var anIndexer := prop'Indexer::aText.
    #loop anIndexer seek:aCriteria delete:{ Count = aSearchText Length. } insert:aReplaceText.

    'program'output << "The resulting text:%n" << aText.

    'program'input get. // wait for any key
].
