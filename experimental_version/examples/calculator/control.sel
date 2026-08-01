// --- namespace shortcuts ---

#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.
#define ctrl'*  = std'patterns'*.

#define parser'* = calc'parser'*.

// --- compiler hints ---
#define[const] NumericExtension = parser'NumericExtension.
#define[const] OperatorExtension = parser'OperatorExtension.
#define[const] BracketExtension = parser'BracketExtension.

// --- Sum ---

#class Sum
{
    #method == aLiteral
    [
        ^ ("+" == aLiteral).
    ]

    #method != aLiteral
    [
        ^ (self == aLiteral) not.
    ]

    #method create : aLiteral
    [
        ^ calc'math'Sum.
    ]
}

// --- Difference ---

#class Difference
{
    #method == aLiteral
    [
        ^ ("-" == aLiteral).
    ]

    #method != aLiteral
    [
        ^ (self == aLiteral) not.
    ]

    #method create : aLiteral
    [
        ^ calc'math'Difference.
    ]
}

// --- Product ---

#class Product
{
    #method == aLiteral
    [
        ^ ("*" == aLiteral).
    ]

    #method != aLiteral
    [
        ^ (self == aLiteral) not.
    ]

    #method create : aLiteral
    [
        ^ calc'math'Product.
    ]
}

// --- Fraction ---

#class Fraction
{
    #method == aLiteral
    [
        ^ ("/" == aLiteral).
    ]

    #method != aLiteral
    [
        ^ (self == aLiteral) not.
    ]

    #method create : aLiteral
    [
        ^ calc'math'Fraction.
    ]
}

// --- Operations ---

#symbol Operations = ( Sum, Difference, Product, Fraction ).

// --- TokenExtensions ---

#symbol TokenExtensions =
(
    NumericExtension,
    OperatorExtension,
    BracketExtension
).

// --- Controller --

#class Controller 
{
    #field theLine. 

    #role Finish
    {
        #method ctrl'$ifBreak [ ]
    }

    #method prop'$input : aReader
    [
        theLine clear $invoke:aReader.

        #if("-q" == theLine)
            ? [ #shift Finish. theLine clear. ]
            | ! [
                theLine << calc'Parser'ExpressionFactory::theLine.
            ].
    ]

    #method prop'$printOn : aWriter
    [
        theLine $invoke:aWriter.
    ]

    #method new
    [
        theLine := basic'String.
    ]
}

// === Program ===

#symbol Program = 
{
    run : anAction [
        'program'output << "ELENA Calculator. Evaluate the expression. To quit type -q".

        anAction proceed:Controller.
    ]
}.

#static Input = { >> aVariable [ win32'io'StdOutput << ">". win32'io'StdInput >> aVariable. ] }.

#static Output = { << aValue [ win32'io'StdOutput << aValue << "%n". ] }.
