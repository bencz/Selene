// --- namespace shortcuts ---
#define basic'*   = std'basic'*.
#define prop'*   = std'properties'*.

#define game'*    = cardgame'game'*.

// === Common game patterns ===

// --- Order Validators ---

#symbol NoDrawOrderValidatorID = 1.
#symbol OrderValidatorID = 2.
#symbol TableValidatorID = 3.

#class NoDrawOrderValidator
{
    #method validate : anOrder
    [
        ^ (anOrder != (self $getTotalCards - self $getTotalOrdered)).
    ]

    #method prop'$writeTo : aWriter
    [
        NoDrawOrderValidatorID $invoke:aWriter.

        self $getTotalOrdered $invoke:aWriter.
        self $getTotalCards $invoke:aWriter.
    ]
}

#class OrderValidator
{
    #method validate : anOrder
    [
        ^ basic'True.
    ]

    #method prop'$writeTo : aWriter
    [
        OrderValidatorID $invoke:aWriter.
    ]
}

// --- Validator Adapter ---

#symbol ValidatorAdapter =
{
    prop'$readFrom : aReader
    [
        #var anID := basic'Integer.
        anID $invoke:aReader.

        #if anID
            ==NoDrawOrderValidatorID ?
            [
                #var aTotalOrdered := basic'Integer.
                #var aTotalCards := basic'Integer.
                aTotalOrdered $invoke:aReader.
                aTotalCards $invoke:aReader.
    
                ^ NoDrawOrderValidator { $getTotalOrdered = aTotalOrdered. $getTotalCards = aTotalCards. }.
            ]
            | == OrderValidatorID ?
            [
                ^ OrderValidator.
            ]
            | == TableValidatorID ?
            [
                #var aRound := game'RoundAdapter::'RuleBook $invoke:aReader.

                ^ cardgame'game'upndown'TableValidator::aRound.
            ].
    ]
}.

//#symbol TableValidatorAdapter =
//{
//    prop'$readFrom : aReader
//    [
//        #var anID := basic'Integer.
//        anID $invoke:aReader.
//
//        #if anID
//            ==NoDrawOrderValidatorID ?
//            [
//                #var aTotalOrdered := basic'Integer.
//                #var aTotalCards := basic'Integer.
//                aTotalOrdered $invoke:aReader.
//                aTotalCards $invoke:aReader.
//    
//                ^ NoDrawOrderValidator { $getTotalOrdered = aTotalOrdered. $getTotalCards = aTotalCards. }.
//            ]
//            | == OrderValidatorID ?
//            [
//                ^ OrderValidator.
//            ].
//    ]
//}.

// === Common game rule properties ===

// --- RoundAdapter ---

#symbol RoundAdapter : aBook
    = aBook $getAdapter.

// --- Rounds ---

#symbol Rounds : aBook =
{
    prop'$getEnumerator = aBook $getRoundEnumerator.

    Count = aBook $getRoundNumber.
}.

// --- RuleBookType ---

#symbol RuleBookType =
{
    of : anObject = anObject $getRuleBook.
}.

// --- CardsNumber ---

#symbol CardsNumber : aRound
    = aRound $getCardsNumber.

// --- Distributor ---

#symbol Distributor : aRound
    = aRound $getDistributor.

//// --- RoundType ---
//
//#symbol RoundType : aRound
//    = aRound::$RoundType.
//
//// --- TableValidator ---
//
//#symbol TableValidator : aRound
//    = aRound::$TableValidator.

#symbol Current = 
{ 
    $isCurrent [] 
    == anIndex
    [
        ^ anIndex $isCurrent 
            back:basic'True
            | back:basic'False. 
    ]
}.