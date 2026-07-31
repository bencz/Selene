// --- namespace shortcuts ---
#define basic'*  = std'basic'*.
//#define memory'*  = std'basic'memory'*.
#define prop'*   = std'properties'*.
#define ctrl'*   = std'patterns'*.
#define coll'*   = std'collections'*.
#define events'* = sys'events'*.

#define game'*    = cardgame'game'*.
#define players'* = cardgame'players'*.
#define cards'*   = cardgame'engine'cards'*.
#define config'*  = cardgame'configs'*.
#define engine'*  = cardgame'engine'*.

// --- compiler hints ---
#define[const]ArrayExtension   = basic'ArrayExtension.
#define[const]ListExtension    = coll'ListExtension.
#define[const]IndexerExtension = prop'IndexerExtension.

// === Classes ===

// --- TableIndexer ---

#class TableIndexer
{
    #field theTable.
    #field theCardVar.
    #field theHand.

    #method new : aTable
    [
        theTable := aTable.
    ]

    #method prop'$goto : aPlayer
    [
        #if (game'Current == aPlayer)
            ? [ self $invoke:(theTable $getCurrent). ]
            | [
                #var anIndex := engine'GamePlayers $getIndex:aPlayer.

                theCardVar := (theTable $getCards)@anIndex.
                theHand := (theTable $getHands)@anIndex.
            ].
    ]

    #method players'$getCard = theCardVar.

    #method players'$getHand = theHand.

    #method $put : aCard
    [
        #if(theHand@aCard)?
        [
            theHand@aCard << basic'False.
            theCardVar << aCard.
        ]
        | [ self fail. ].
    ]

    #method $pullBack
    [
        theHand@theCardVar << basic'False.
        theCardVar << 0.
    ]
}

// --- Table ---

#class Table
{
    #field theTrump.
    #field theHands.
    #field theCards.
    #field theCurrent.

    #method $getHands = theHands.

    #method $getCards = theCards.

    #method $getTrump = theTrump.

    #method $getCurrent = theCurrent.

    #method $setCurrent : aPlayer
    [
        theCurrent := aPlayer.
    ]

    #method new : aPlayernumber
    [
        theTrump := basic'Integer << 0.

        theHands := basic'ArrayType create:{ Length = aPlayernumber. Create = cards'Hand. }.
        theCards := basic'ArrayType create:{ Length = aPlayernumber. Create = basic'Integer << 0. }.
    ]

    #method prop'$getIndexer = TableIndexer::self.

    #method clear
    [
        theTrump << 0.

        prop'Enumerator::theHands run: aHand => (aHand clear).
    ]

    #annex (ArrayExtension, IndexerExtension).
}

// --- Score ---

#class Score
{
    #field theOrdered.
    #field theTaken.

    #method new
    [
        theOrdered := basic'Integer << 0.
        theTaken := basic'Integer << 0.
    ]

    #method $getOrdered = theOrdered.

    #method $getTaken = theTaken.
}

// --- ScoreDesk ---

#class ScoreDesk
{
    #field theScores.

    #method new : Players
    [
        theScores := coll'List.

        prop'Enumerator::Players run: aPlayer =>
        [
            #var aScore := Score.

            theScores += prop'KeyValue :: #annex(aPlayer) { prop'$getContent = aScore. }.
        ].
    ]

    #method @ aPlayer
    [
        ^ ctrl'SearchAgent::theScores proceed:ctrl'KeyComparator::aPlayer.
    ]
}

// --- GameMaster ---

#class GameMaster
{
    #field theRulebook.
    #field thePlayers.

    #field theExtension.
    #field theEvents.

    #field theTable.
    #field theScores.

    #role NotStarted
    {
        #method $addPlayer : aPlayerInfo
        [
            thePlayers += aPlayerInfo.
        ]

        #method start
        [
            #shift.

            self $prepare.

            self $playGame:game'Rounds::theRulebook.
        ]
    }

    #method $getPlayers = thePlayers.

    #method $prepare
    [
        theTable := Table::(thePlayers Count).
        theScores := ScoreDesk::thePlayers.

        #var aCounter := thePlayers Count.
        #loop (aCounter > 0)?
        [
            thePlayers shift:1.

            (thePlayers@0) $onPrepare:thePlayers.

            aCounter -= 1.
        ].

        self $synchronize:thePlayers.
    ]

////    #method $PlayerCount = thePlayers->vprop'Count.

    #method new : aGameInfo
    [
        theEvents := events'Controller.

        theExtension := config'ExtensionFactoryType of:aGameInfo create:self.
        theRulebook := game'RuleBookType of:aGameInfo.
        thePlayers := coll'Circle.

        #shift NotStarted.
    ]

    #method events'$getController = theEvents.

    #method $playGame : aRounds
    [
        prop'Enumerator::aRounds run: aRound =>
        [
            theTable clear.

            prop'Enumerator::thePlayers run: aPlayer => (aPlayer $onNewRound:aRound).

            self $synchronize:thePlayers.

            // set the distribution order - the distributor is the last
            #loop(thePlayers@0 != game'Distributor::aRound)?
            [
                thePlayers shift:1.
            ].
            thePlayers shift:1.

            aRound proceed:
            {
                game'$selectTrump : aCard = self $selectTrump:aCard.
                game'$distributeCard : aPack = self $distributeCard:aPack.
                game'$makeOrder : aValidators = self $makeOrder:aValidators.
                game'$playRound : aRound = self $playRound:aRound.
            }.
        ].
    ]

    #method $selectTrump : aCard
    [
        theTable $getTrump << aCard.

        prop'Enumerator::thePlayers run: aPlayer =>
        [
            aPlayer $selectTrump:aCard.            
        ].
        self $synchronize:thePlayers.
    ]

    #method $distributeCard : aPack
    [
        prop'IndexEnumerator::thePlayers run: aPlayer =>
        [
            #var aCard := aPack get.

            aPlayer $distributeCard:aCard.

            (theTable $getHands)@ prop'Index::aPlayer @ aCard << basic'True.

            (theScores@aPlayer) $getTaken << 0.
            (theScores@aPlayer) $getOrdered << 0.
        ].

        self $synchronize:thePlayers.
    ]

    #method $makeOrder : aValidators
    [
        #var aTotalOrdered := basic'Integer << 0.

        prop'Enumerator::thePlayers run: aPlayer =>
        [
            #var anOrdered := basic'Integer.
            #var aValidator := aValidators@aPlayer.

            aPlayer $makeOrder:#annex(aValidator)
            { 
                game'$getTotalOrdered = aTotalOrdered.

                << anOrder 
                [ 
                    #if($self validate:anOrder)?
                        [ anOrdered << anOrder. ]
                        | [ self fail. ].
                ]
            }.

            (theScores@aPlayer) $getOrdered << anOrdered.

            self $onOrdered:#annex(anOrdered) { prop'$getKey = aPlayer $invoke. }.

            aTotalOrdered += anOrdered.
        ].
    ]

    #method $onOrdered : anOrdered
    [
        prop'Enumerator::thePlayers run: aPlayer =>
        [
            aPlayer $onOrdered:anOrdered.

            self $synchronize:thePlayers.
        ].
    ]

    #method $beforeMove : aPlayer
    [
        prop'Enumerator::thePlayers run: aCurrentPlayer =>
        [
            aCurrentPlayer $onBeforeMove:aPlayer.

            self $synchronize:thePlayers.
        ].
    ]

    #method $playRound : aRound
    [
        prop'IndexEnumerator::thePlayers run: aPlayer =>
        [
            theTable $setCurrent:prop'ContentValue::aPlayer.

            self $beforeMove:aPlayer.

            aPlayer $putCard:#annex(aRound)
            {
                << aCard
                [
                    (theTable@game'Current) $put:aCard.

                    #if aRound validate:theTable
                    | [
                        (theTable@game'Current) $pullBack.
                        self fail.
                    ].

                    self $showCard:prop'KeyValue::#annex(aPlayer) { prop'$getContent = aCard. }.
                ]
            }.            
        ].
    ]

    #method $showCard : aCard
    [
        prop'Enumerator::thePlayers run: aCurrentPlayer =>
        [
            aCurrentPlayer $onShowCard:aCard.
        ].

        self $synchronize:thePlayers.
    ]

    #annex theExtension.
}

// === Properties ===

// --- Master ---

#static Master = ctrl'Variable.

// --- Players ---

#class Players
{
    #field theMaster.

    #method new : aMaster
    [
        theMaster := aMaster.
    ]

    #method += aPlayer
    [
        theMaster $addPlayer:aPlayer.
    ]

    #method $getIndex : aPlayer
    [
        #var anIndex := basic'Integer << -1.

        prop'IndexEnumerator::self run: aCurrent =>
        [
            #if(aPlayer == aCurrent)?
                [ anIndex << prop'Index::aCurrent. ^ ctrl'Break. ].
        ].

        ^ anIndex.
    ]

    #method prop'$getEnumerator = theMaster $getPlayers $invoke.

    #annex (ListExtension, IndexerExtension).
}

////// --- Scores ---
////
////#symbol Scores : aTable = 'nil.

// --- Trump ---

#symbol Trump : aTable = aTable $getTrump.

////// --- Round ---
////
////#symbol Round : aTable = 'nil.

// --- GamePlayers ---

#static GamePlayers = Players::Master.
