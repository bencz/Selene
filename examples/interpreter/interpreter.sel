// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.
#define io'*    = ext'io'*.

// --- Tape ---

#class TapeIterator
{
    #field thePointer.
    #field theArray.

    #method new
    [
        thePointer := basic'Integer << 0.
        theArray := basic'ArrayType create:{ Length = 1024. create = basic'Char << 0. }.
    ]

    #method $next
    [
        thePointer += 1.
    ]

    #method $previous
    [
        #if (thePointer > 0)?
        [
            thePointer -= 1.    
        ].
    ]

    #annex (theArray@thePointer).
}

// --- CharEnumerator ---

#symbol CharEnumerator : aFileName =
{
    run : anAction
    [
        prop'Enumerator::io'TextFile::aFileName run: 
            aLine => ( prop'Enumerator::aLine run:anAction ).
    ]
}.

// --- Interpreter ---

#class Interpreter
{
    #field theTapeIterator.

    #field theLoopTape.

    #role Looping
    {
        #method $resetLoop
        [
            theLoopTape new.
        ]

        #method proceed : aChar
        [
            #if (basic'LiteralType of:aChar)
            == "]" ? 
            [
                #loop(0 < theTapeIterator)?
                    [ prop'Enumerator::theLoopTape run:Interpreter::theTapeIterator. ].

                #shift.
            ]
            | [
                theLoopTape += aChar.
            ].
        ]
    }

    #method proceed : aChar
    [
        #if (basic'LiteralType of:aChar)
            == ">"   ? [ theTapeIterator $next. ]
            | == "<" ? [ theTapeIterator $previous. ]
            | == "+" ? [ theTapeIterator += 1. ]
            | == "-" ? [ theTapeIterator -= 1. ]
            | == "." ? [ 'program'Output << theTapeIterator. ]
            | == "," ? [ 'program'Input get. ]
            | == "[" ? [ #shift Looping. self $resetLoop. ].
    ]

    #method new : aTapeIterator
    [
        theLoopTape := basic'String.
        theTapeIterator := aTapeIterator.
     ]
}

// --- Program ---

#symbol Program = Arguments =>
[
    #if (Arguments Count == 1)?
        [  'program'output << "Please provide the path to the file to interpret". $self fail. ].

    CharEnumerator::(basic'LiteralType of:(Arguments@1)) run:Interpreter::TapeIterator.

    'program'input get. // wait for any key
].