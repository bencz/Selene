// --- namespace shortcuts ---
#define basic'* = std'basic'*.
//#define memory'*  = std'basic'memory'*.
#define prop'*  = std'properties'*.
#define coll'*  = std'collections'*.
#define ctrl'*  = std'patterns'*.

#define cards'* = cardgame'engine'cards'*.

// --- compiler hints ---
#define[const]ArrayExtension   = basic'ArrayExtension.
#define[const]IndexerExtension = prop'IndexerExtension.
#define[const]ListExtension    = coll'ListExtension.

// --- Card Constants ---

#symbol Diamonds_Six=   1.
#symbol Diamonds_Seven= 2.
#symbol Diamonds_Eight= 3.
#symbol Diamonds_Nine=  4.
#symbol Diamonds_Ten=   5.
#symbol Diamonds_Knave= 6.
#symbol Diamonds_Queen= 7.
#symbol Diamonds_King=  8.
#symbol Diamonds_Ace=   9.

#symbol Clubs_Six=   10.
#symbol Clubs_Seven= 11.
#symbol Clubs_Eight= 12.
#symbol Clubs_Nine=  13.
#symbol Clubs_Ten=   14.
#symbol Clubs_Knave= 15.
#symbol Clubs_Queen= 16.
#symbol Clubs_King=  17.
#symbol Clubs_Ace=   18.

#symbol Hearts_Six=   19.
#symbol Hearts_Seven= 20.
#symbol Hearts_Eight= 21.
#symbol Hearts_Nine=  22.
#symbol Hearts_Ten=   23.
#symbol Hearts_Knave= 24.
#symbol Hearts_Queen= 25.
#symbol Hearts_King=  26.
#symbol Hearts_Ace=   27.

#symbol Spades_Six=   28.
#symbol Spades_Seven= 29.
#symbol Spades_Eight= 30.
#symbol Spades_Nine=  31.
#symbol Spades_Ten=   32.
#symbol Spades_Knave= 33.
#symbol Spades_Queen= 34.
#symbol Spades_King=  35.
#symbol Spades_Ace=   36.

#symbol Joker= Clubs_Six.

#symbol Big_Joker=    37.
#symbol Big_Card=     42.

#symbol Diamonds_Min= 38.
#symbol Hearts_Min=   39.
#symbol Clubs_Min=    40.
#symbol Spades_Min=   41.

// --- Pack ---

#class Pack
{
    #field theCards.
    #field theCurrent.

    #role Open
    {
        #method get
        [
            theCurrent next.
    
            ^ theCurrent get.
        ]

        #method peek
        [
            ^ theCards @ (theCurrent Position + 1) | back:(theCurrent get).
        ]
    }

    #method new
    [
        theCards := basic'ArrayType create:{ Length = 36. }.
    ]

    #method open
    [
        #var aHand := cards'Hand.

        #var aCounter := basic'Integer << 0.
        #loop (aCounter < 36)?
        [
            #var aCard := 1 + ext'utilities'RandomValue::36.
            #if(aHand@aCard)!
            [
                prop'Content::(theCards@aCounter) set:aCard.
                prop'Content::(aHand@aCard) set:basic'True.

                aCounter += 1.
            ].
        ].
    ]

    #method get
    [
        theCurrent := prop'Indexer::theCards.

        #shift Open.

        ^ theCurrent get.
    ]

    #method prop'$getIndexer = theCards $invoke. // !! should be read-only

    #annex (ArrayExtension, IndexerExtension).
}

// --- Hand ---

#class Hand
{
    #field theDiamonds.
    #field theClubs.
    #field theHearts.
    #field theSpades.

    #method new
    [
        theDiamonds := basic'Integer << 0.
        theHearts := basic'Integer << 0.
        theClubs := basic'Integer << 0.
        theSpades := basic'Integer << 0.
    ]

    #method << aHand
    [
        theDiamonds << aHand $getDiamonds.
        theHearts << aHand $getHearts.
        theClubs << aHand $getClubs.
        theSpades << aHand $getSpades.
    ]
    
    #method $getDiamonds = theDiamonds.

    #method $getHearts = theHearts.

    #method $getClubs = theClubs.

    #method $getSpades = theSpades.

    #method prop'$getIndexer = cards'HandIndexer::self.

    #method Count
    [
        #var aCounter := basic'Integer::0.

        prop'Enumerator::self run: aCard =>
        [
            #if aCard?
                [ aCounter += 1. ].
        ].

        ^ aCounter.
    ]

    #method clear
    [
        theDiamonds << 0.
        theHearts << 0.
        theClubs << 0.
        theSpades << 0.
    ]

    #annex (ListExtension, IndexerExtension).
}

// --- HandIndexer ---

#class HandIndexer
{
    #field theHand.
    #field theCard.

    #method prop'$getIndex = theCard.

    #method new : aHand
    [
        theHand := aHand.
        theCard := basic'Integer << 0.
    ]

    #method prop'$goto : aCard
    [
        basic'True if:(aCard < 37) if:(aCard > 0).

        theCard << aCard.
    ]

    #method prop'$start
    [
        theCard << 0.
    ]

    #method prop'$next
    [
        basic'True if:(theCard < 36).

        theCard += 1.
    ]

    #method prop'$getContent
    [
        #if(theCard < Clubs_Six)?
        [
            #var aCode := 1 shift:(Diamonds_Six - theCard).

            ^ theHand $getDiamonds allMask:aCode back:basic'True | back:basic'False.
        ].

        #if(theCard < Hearts_Six)?
        [
            #var aCode := 1 shift:(Clubs_Six - theCard).

            ^ theHand $getClubs allMask:aCode back:basic'True | back:basic'False.
        ].

        #if(theCard < Spades_Six)?
        [
            #var aCode := 1 shift:(Hearts_Six - theCard).

            ^ theHand $getHearts allMask:aCode back:basic'True | back:basic'False.
        ].

        #if(theCard < Big_Joker)?
        [
            #var aCode := 1 shift:(Spades_Six - theCard).

            ^ theHand $getSpades allMask:aCode back:basic'True | back:basic'False.
        ].
    ]

    #method prop'$setContent : aBool
    [
        #var aCode := nil.
        #var aBitset := nil.

        #if(theCard < Clubs_Six)?
        [
            aCode := 1 shift:(Diamonds_Six - theCard).
            aBitset := theHand $getDiamonds.
        ]
        | [
            #if(theCard < Hearts_Six)?
            [
                aCode := 1 shift:(Clubs_Six - theCard).
                aBitset := theHand $getClubs.

            ]
            | [
                #if(theCard < Spades_Six)?
                [
                    aCode := 1 shift:(Hearts_Six - theCard).
                    aBitset := theHand $getHearts.
                ]
                | [
                    #if(theCard < Big_Joker)?
                    [
                        aCode := 1 shift:(Spades_Six - theCard).
                        aBitset := theHand $getSpades.
                    ].
                ].
            ].
        ].

        #if(aBool)
            ? [  aBitset << aBitset or:aCode. ]
            | ! [ aBitset << aBitset and:(aCode Not). ].
    ]

    #method basic'$getBool = prop'ContentValue::self.

    #method ? [ basic'True if:self. ]

    #method ! [ basic'False if:self. ]

    #method << aBool
    [
        prop'Content::self set:aBool.
    ]
}

// --- CardAdapter ---

#symbol CardAdapter =
{
    prop'$readFrom : aReader
    [
        #var anIndex := basic'Integer.
        #var aCard := basic'Integer.

        anIndex $invoke:aReader.
        aCard $invoke:aReader.

        ^ prop'KeyValue::{ prop'$getKey = anIndex. prop'$getContent = aCard. }.
    ]
}.

// --- Suit ---

#class Suit
{
    #role Diamonds
    {
        #method $getMinCard = Diamonds_Six.
    }

    #role Hearts
    {
        #method $getMinCard = Hearts_Six.
    }

    #role Clubs
    {
        #method $getMinCard = Clubs_Six.
    }

    #role Spades
    {
        #method $getMinCard = Spades_Six.
    }

    #method new : aCard
    [
        #if((aCard >= Diamonds_Six)and:(aCard <= Diamonds_Ace)or:(aCard == Diamonds_Min))?
            [ #shift Diamonds. ^ self. ].

        #if((aCard >= Hearts_Six)and:(aCard <= Hearts_Ace)or:(aCard == Hearts_Min))?
            [ #shift Hearts. ^ self. ].

        #if((aCard >= Clubs_Six)and:(aCard <= Clubs_Ace)or:(aCard == Clubs_Min))?
            [ #shift Clubs. ^ self. ].

        #if((aCard >= Spades_Six)and:(aCard <= Spades_Ace)or:(aCard == Spades_Min))?
            [ #shift Spades. ^ self. ].
    ]
}

#symbol MinSuitCard : aCard 
    = Suit::aCard $getMinCard.