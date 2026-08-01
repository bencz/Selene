// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.
#define ctrl'*  = std'patterns'*.

#define game'*    = cardgame'game'*.
#define engine'*  = cardgame'engine'*.
#define cards'*   = cardgame'engine'cards'*.
#define upndown'* = cardgame'game'upndown'*.
#define players'* = cardgame'players'*.

// === Classes ===

// --- RuleBook ---

#class RuleBook
{
    #field thePlayerNumber.
    #field theDistributor.
    #field theLeader.

    #method $getRuleBook = $self.

    #method new : aPlayernumber
    [
        #if aPlayernumber ifNotNil
        [
            thePlayerNumber := basic'IntNumber::aPlayerNumber.

            theDistributor := 1/*ext'utilities'RandomValue::thePlayerNumber*/.
            theLeader := basic'Integer << (theDistributor + 1).
            #if (theLeader >= thePlayerNumber)?
            [
                theLeader << 0.
            ].
        ].
    ]

    #method $getDistributor = engine'GamePlayers@theDistributor.

    #method $getLeader = engine'GamePlayers@theLeader.

//    #method $PlayerNumber = thePlayerNumber.

    #method $getRoundEnumerator = upndown'RoundEnumerator::self.

    #method $getAdapter = upndown'RoundAdapter::self.
}

// --- Stages ---

#symbol StageUp
   = { == aStage = aStage $ifUp back:basic'True | back:basic'False. $ifUp [] }.

// --- Stage ---

#class Stage
{
    #role Up { #method $ifUp [] #method $getName = "Up". #method $getID = 1. }

    #method new : aStage
    [
        #if(StageUp == aStage)?
            [ #shift Up. ].
    ]

    #method prop'$writeTo : aWriter
    [
        self $getID $invoke:aWriter.
    ]

    #method prop'$printOn : aWriter
    [
        self $getName $invoke:aWriter.
    ]

    #method prop'$readFrom : aReader
    [
        #var anID := basic'Integer.
        anID $invoke:aReader.

        #if(anID==1)?
            [ #shift Up. ].
    ]
}

// --- Round ---

#class Round
{
    #field theCardsNumber.
    #field theStage.
    #field theDistributor.
    #field theLeader.
    #field theRulebook.

    #method $getRound = $self.

    #method $getLeader = theLeader.

    #method new : aRoundInfo
    [
        theCardsNumber := aRoundInfo $getCardsNumber.
        theStage := Stage::(aRoundInfo $getRoundType).
        theDistributor := aRoundInfo $getDistributor.
        theLeader := aRoundInfo $getLeader.
        theRulebook := aRoundInfo $getRulebook.
    ]

    #method proceed : aMaster
    [
        #var aPack := cards'Pack open.

        ctrl'DecrimentLoop::theCardsNumber run: =>
        [
            aMaster $distributeCard:aPack.
        ].

        /*#if(self)$ifTrumpless
            | [*/ aMaster $selectTrump:(aPack peek). //].

        aMaster $makeOrder:
        {
            @ aPlayer
            [
                #if(aPlayer == theDistributor)?
                    [ ^ game'NoDrawOrderValidator { $getTotalCards = theCardsNumber. }. ]
                    | [ ^ game'OrderValidator. ].
            ]
        }.

        ctrl'DecrimentLoop::theCardsNumber run: =>
        [
            #var aTable := aMaster $playRound:upndown'TableValidator::self.
                
//            aRound $calculateScores:aTable.
//
//            aMaster $setScores:aTable.
        ].
    ]

    #method $getCardsnumber = theCardsNumber.

    #method $getDistributor = theDistributor.

    #method prop'$writeTo : aWriter
    [
        theCardsNumber $invoke:aWriter.
        theStage $invoke:aWriter.
        prop'ContentKey::theDistributor $invoke:aWriter.  
        prop'ContentKey::theLeader $invoke:aWriter.
    ]

    // #method $getTableValidator = upndown'TableValidator::self.

    #method prop'$printOn : aWriter
    [
        theStage $invoke:aWriter.

        "(" $invoke:aWriter.
        theCardsNumber $invoke:aWriter.
        ")" $invoke:aWriter.
    ]
}

// --- RoundAdapter ---

#class RoundAdapter
{
    #field theRules.

    #method new : aRulebook
    [
        theRules := aRulebook $getRulebook.
    ]

    #method prop'$readFrom : aReader
    [
        #var aCardNumber := basic'Integer.
        #var aStage := Stage.
        #var aDistributor := basic'Integer.
        #var aLeader := basic'Integer.

        aCardNumber $invoke:aReader.
        aStage $invoke:aReader.
        aDistributor $invoke:aReader.
        aLeader $invoke:aReader.

        #var aRound := Round::
        { 
            $getCardsNumber=aCardNumber.
            $getRoundType=aStage. 
            $getDistributor={ prop'$getKey = aDistributor. }. 
            $getRulebook = theRules.
            $getLeader = { prop'$getKey = aLeader. }.
        }.

        ^ aRound.
    ]
}

// --- RoundEnumerator ---

#class RoundEnumerator
{
    #field theRules.
    #field theRound.

	#role Up        
    { 
        #method $newRound
        [
            theRound := Round::
            { 
                $getCardsNumber=1. 
                $getRoundType=StageUp. 
                $getDistributor=theRules $getDistributor.
                $getLeader=theRules $getLeader.
                $getRulebook = theRules.
            }.
        ]
    }

    #method new : aRules
    [
        theRules := aRules $getRuleBook.
    ]

    #method start
    [
        #shift Up.

        self $newRound.
    ]

    #method get = theRound.
}

// --- SuitComparator ---

#class SuitComparator
{
    #method == aCard
    [
        #if(aCard==0)?
            [ ^ basic'False. ].

        ^ ((aCard != cards'Joker)or:(self $getTrump == 0))and:(cards'Suit::aCard == self $getSuit).
    ]
}

// --- TableValidator ---

#class TableValidator
{
    #field theRound.

    #method new : aRound
    [
        theRound := aRound $getRound.
    ]

    #method validate : aTable
    [
        #var aFirstCard := players'Card::(aTable@(theRound $getLeader)).
        #var aHand := players'Hand::(aTable@game'Current).
        #var aTrump := engine'Trump::aTable.
        #var aCard := players'Card::(aTable@game'Current).

        // The leading player could put any card
        #if(aFirstCard == aCard)?
            [ ^ basic'True. ].

        // If the leading player put the seniour card
        #if(cards'Big_Joker == aFirstCard)?
        [
            // If the player has no trump cards
            #if(ctrl'SearchAgent::aHand != SuitComparator { $getTrump = aTrump. $getSuit = cards'Suit::aTrump. } )?
            [
                ^ basic'True.
            ].
            // Else should put the biggest trump
            #var aMaxCard := basic'Integer << 0.
            ^ (aCard == ctrl'Agent proceed:ctrl'GetMaxAction::#annex(aMaxCard)
            { 
                < aCard
                [ 
                    ^ (cards'Suit::aCard == cards'Suit::aTrump)and:(aCard > aMaxCard).
                ]
            }).
        ]
        | [
            #if (cards'Suit::aFirstCard == cards'Suit::aCard) 
                or:(cards'MinSuitCard::aFirstCard == cards'Suit::aCard)
            ? [ ^ basic'True. ].

            #if (cards'Suit::aCard == cards'Suit::aTrump)or:(aCard == cards'Big_Card)
            ? [
                #if(ctrl'SearchAgent::aHand != SuitComparator { $getTrump = aTrump. $getSuit = cards'Suit::aCard. } )
                    or:(aCard == cards'Big_Card)
                ? [ ^ basic'True. ].
                    
            ].
            #if(ctrl'SearchAgent::aHand != SuitComparator { $getTrump = aTrump. $getSuit = cards'Suit::aFirstCard. } )
                or:(ctrl'SearchAgent::aHand == SuitComparator { $getTrump = aTrump. $getSuit = cards'Suit::aTrump. } )
            ? [ ^ basic'True. ].            
        ].

        ^ basic'False.
    ]

    #method prop'$writeTo : aWriter
    [
        game'TableValidatorID $invoke:aWriter.
        theRound $invoke:aWriter.
    ]    
}

