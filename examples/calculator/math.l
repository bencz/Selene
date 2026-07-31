// --- namespace shortcuts ---
#define basic'*   = std'basic'*.
#define parser'* = calc'parser'*.

// --- compiler hints ---
#define[const] RealExtension    = basic'RealExtension.
#define[const] ParsingOrder0    = parser'ParsingOrder0.
#define[const] ParsingOrder1    = parser'ParsingOrder1.
#define[const] ParsingOrder2    = parser'ParsingOrder2.

// --- Number ---

#class Number
{
    #field theNumber.
    
    #method basic'$toReal = theNumber.

    #method new : aValue
    [
        theNumber := basic'RealType of:basic'VariantValue::aValue.
    ]

    #annex(#group(RealExtension, ParsingOrder0)).
}

// --- Sum ---

#class Sum
{
    #method basic'$toReal
    [
        ^ basic'RealType of:(self X) + basic'RealType of:(self Y).
    ]

    #annex(#group(RealExtension, ParsingOrder2)).
}


// --- Difference ---

#class Difference
{
    #method basic'$toReal 
    [
        ^ basic'RealType of:(self X) - basic'RealType of:(self Y).
    ]

    #annex(#group(RealExtension, ParsingOrder2)).
}

// --- Product ---

#class Product
{
    #method basic'$toReal 
    [
        ^ basic'RealType of:(self X) * basic'RealType of:(self Y).
    ]

    #annex(#group(RealExtension, ParsingOrder1)).
}

// --- Fraction ---

#class Fraction
{
    #method basic'$toReal 
    [
        ^ basic'RealType of:(self X) / basic'RealType of:(self Y).
    ]

    #annex(#group(RealExtension, ParsingOrder1)i).
}
