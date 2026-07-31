// --- namespace shortcuts ---
#define basic'*  = std'basic'*.
#define prop'*   = std'properties'*.
#define ctrl'*   = std'patterns'*.
#define coll'*   = std'collections'*.
#define events'* = sys'events'*.

////#define players'* = cardgame'game'players'*.
#define game'*    = cardgame'game'*.
#define engine'*  = cardgame'engine'*.
#define cards'*   = cardgame'engine'cards'*.
#define config'* = cardgame'configs'*.

// === Classes ===

// --- Base ---

#class Base 
{
    #field theID.
    #field theName.
    #field theExtension.
    #field theEvents.

    #method new : aPlayerInfo
    [
        theEvents := events'Controller.

        #if aPlayerInfo ifNotNil
        [
            theExtension := config'ExtensionFactoryType of:aPlayerInfo create:self.

            theID := basic'IntNumberType of:(aPlayerInfo $getID).
            theName := basic'LiteralType of:(aPlayerInfo $getName).

            theEvents $onNew:aPlayerInfo.
        ].
    ]

    #method prop'$getKey = theID.

    #method $getID = theID.

    #method $getName = theName.

    #method == aPlayer
    [
        ^ theID == prop'ContentKey::aPlayer.
    ]

    #method != aPlayer
    [
        ^ theID != prop'ContentKey::aPlayer.
    ]

    #method prop'$writeTo : aWriter
    [
        theName $invoke:aWriter.
        theID $invoke:aWriter.
    ]

    #method prop'$readFrom : aReader
    [
        theName := basic'String.
        theID := basic'Integer.

        theName $invoke:aReader.
        theID $invoke:aReader.
    ]

    #method prop'$printOn : aWriter
    [
        theName $invoke:aWriter.
    ]

    #method $getExtension = theExtension.

    #method events'$getController = theEvents.

    #method engine'$onPrepare : Players
    [
        self $doPrepare:Players.
    ]

    #method engine'$onNewRound : aRound
    [
        self $doNewRound:aRound.
    ]    

    #method engine'$distributeCard : aCard
    [
        self $doDistributeCard:aCard.
    ]

    #method engine'$selectTrump : aCard
    [
        self $doSelectTrump:aCard.
    ]

    #method engine'$onOrdered : anOrdered
    [
        self $doOrdered:anOrdered.
    ]

    #method engine'$onBeforeMove : aPlayer
    [
        self $doBeforeMove:aPlayer.
    ]

    #method engine'$onShowCard : aCard
    [
        self $doShowCard:aCard.
    ]

    #method engine'$makeOrder : anOrder
    [
        self $makeOrder:anOrder.
    ]

    #method engine'$putCard : aCard
    [
        self $putCard:aCard.
    ]

    #method $doPrepare : Players
    [
        theEvents $onPrepare:Players.
    ]

    #method $doNewRound : aRound
    [
        #if(self==game'Distributor::aRound)?
            [ theEvents $onNewLeadingRound:aRound. ]
            | [ theEvents $onNewRound:aRound. ].        
    ]

    #method $doDistributeCard : aCard
    [
        theEvents $distributeCard:aCard.
    ]

    #method $doSelectTrump : aCard
    [
        theEvents $selectTrump:aCard.
    ]

    #method $doOrdered : anOrdered
    [
        theEvents $onOrdered:anOrdered.
    ]

    #method $doBeforeMove : aPlayer
    [
        #if(self==aPlayer)?
            [ theEvents $onBeforeYourMove:aPlayer. ]
            | [ theEvents $onBeforeMove:aPlayer. ].
    ]

    #method $doShowCard : aCard
    [
        theEvents $onShowCard:aCard.
    ]

    #annex theExtension.
}

// --- Human ---

#class Human (Base)
{
    #field theOponents.
    #field theHand.
    #field theTrump.

    #method $getHand = theHand.

    #method new : aPlayerInfo
    [
        theHand := cards'Hand.
        theTrump := basic'Integer.

        super new:aPlayerInfo.
    ]

    #method engine'$onPrepare : Players
    [
        theOponents := coll'List.

        prop'Enumerator::Players run: aPlayer =>
        [
			#if(aPlayer != self)?
                [ theOponents += aPlayer. ]. 
        ].

        super $invoke:Players.
    ]

    #method engine'$onNewRound : aRound
    [
        super $invoke:aRound.

        theHand clear.
    ]

    #method engine'$distributeCard : aCard
    [
        super $invoke:aCard.

        theHand@aCard << basic'True.
    ]

    #method engine'$selectTrump : aCard
    [
        super $invoke:aCard.

        theTrump << aCard.
    ]
}

//// --- Name ---
//
//#symbol Name : aPlayer
//    = aPlayer::$Name.

// --- Client ---

#static Client = ctrl'Variable.

// --- Hand ---

#symbol Hand : aPlayer
    = aPlayer $getHand.

// --- Card ---

#symbol Card : aPlayer
    = aPlayer $getCard.
