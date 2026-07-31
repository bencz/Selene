// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.
#define list'*  = std'collections'*.

// --- compiler hints ---
#define[const] ListExtension = list'ListExtension.

// === Extending text patterns ===

// --- PrintValue ---

#class PrintValue
{
    #field theValue.

    #method new : anObject
    [
        #var theWriter := basic'StringWriter.

        prop'TextWriter::{ prop'$getTextWriter = theWriter. } << anObject.

        theValue := basic'LiteralType of:theWriter.
    ]

    #annex(theValue).
}

// === Helper classes ===

// --- WordEnumerator ---

#class WordEnumerator
{
    #field theEnumerator.
    #field theWord.

    #role Space
    {
        #method $read : aChar
        [
            #if(32 != aChar)
                ? [ self $newToken. self $read:aChar. ]
                | ! [ theEnumerator next. ].
        ]
    }

    #role Token
    {
        #method $read : aChar
        [
            #if(32 == aChar)
                ? [ self $space fail. ]
                | ! [ theWord += aChar. ].

            theEnumerator next.
        ]
    }

    // class methods

    #method[def] $getClassName = "ext'text'WordEnumerator".

    // public methods

    #method $readWord
    [
        theWord new.

        #loop self $read:(theEnumerator get).
    ]

    #method $newToken
    [
        theWord new.

        #shift Token.
    ]

    #method $space
    [
        #shift Space.
    ]

    #method new : anObject
    [
        theEnumerator := prop'Enumerator::anObject.
        theWord := basic'String.
    ]

    #method start
    [
        #shift Space.

        theEnumerator start.

        self $readWord.
    ]

    #method get
    [
        ^ basic'LiteralType of:theWord.
    ]

    #method next
    [
        theEnumerator next.

        self $readWord.

        basic'True if:(theWord Length > 0).
    ]
}

// --- Words ---

#symbol Words : aText = #annex(ListExtension)
{
    prop'$getEnumerator = WordEnumerator::aText.
}.

// --- WordParsing ---

#class WordParsing
{
    #field theWords.

    // class methods

    #method[def] $getClassName = "ext'text'WordParsing".

    // public methods

    #method new : aText
    [
        theWords := Words::aText.
    ]

    #method run : anAction
    [
        ^ prop'Enumerator::theWords run:anAction.
    ]
}
