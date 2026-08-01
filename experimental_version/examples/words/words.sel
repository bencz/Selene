#define basic'* = std'basic'*.
#define list'*  = std'collections'*.
#define ctrl'*  = std'patterns'*.

#define text'*  = ext'text'*.

// --- WordList ---

#class WordList
{
    #field theList.
    #field theTotal.

    #method new
    [
        theList := list'List.
        theTotal := basic'Integer << 0.
    ]

    #method += aWord
    [
        theTotal += 1.

        #if(ctrl'SearchAgent::theList == aWord)!
            [ theList += basic'LiteralType of:aWord. ].
    ]

    #method $getTotal = theTotal.

    #method $getUnique = theList Count.

    #annex (theList).
}

// --- Program ---

#symbol Program =>
[
    'program'output << "Enter the text(to stop press enter two times):%n".

    #var aText := basic'String.
    #var aFlag := basic'Boolean << basic'True.

    #var aReader := #annex(aText)
    {
        += aValue
        [
            #var aLiteral := basic'LiteralType of:aValue.

            #if (aLiteral Length != 0)?
                [ aText += aValue. ]
                | [ aFlag << basic'False. ].
        ]
    }.

    #loop aFlag?
    [    
        'program'Input >> aReader.

        aText += " ".
    ].

    #var aList := WordList.
    text'WordParsing::aText run: aWord =>(aList += aWord).

    'program'output << "There are " << aList $getUnique << " unquie words out of " << aList $getTotal.
    'program'output << "%nthe list of unique words:%n".

    'program'output << aList.

    'program'input get. // wait for any key
].
