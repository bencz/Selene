// --- namespace shortcuts ---
#define prop'*          = std'properties'*.
#define events'*        = sys'events'*.

#define engine'*        = cardgame'engine'*.
#define game'*          = cardgame'game'*.

// --- FormExtension ---

#class FormExtension
{
    #field theForm.

    #method $onNew : aProfile
    [
        theForm := aProfile $getForm.
    ]

    #method $onPrepare : Players
    [
        theForm $invoke:Players.
    ]

    #method $onNewLeadingRound : aRound
    [
        theForm $invoke:aRound.
    ]

    #method $onNewRound : aRound
    [
        theForm $invoke:aRound.
    ]

    #method $distributeCard : aCard
    [
        theForm $invoke:aCard.
    ]

    #method $selectTrump : aCard
    [
        theForm $invoke:aCard.
    ]

    #method $makeOrder : anOrder
    [
        theForm $invoke:anOrder.
    ]

    #method $onOrdered : anOrdered
    [
        theForm $invoke:anOrdered.
    ]

    #method $onBeforeMove : anID
    [
        theForm $invoke:anID.
    ]

    #method $onBeforeYourMove
    [
        theForm $invoke.
    ]

    #method $onShowCard : aCard
    [
        theForm $invoke:aCard.
    ]

    #method $putCard : aTable
    [
        theForm $invoke:aTable.
    ]

    #method $getCard
    [
        ^ theForm $invoke.
    ]

    #method $getTable =
    {
        engine'$getTrump = theForm $getTrump.

        @ aPlayer
        [ 
            #if(game'Current == aPlayer)?
            [
                ^ { $getHand = self $getHand. $getCard = self $getCard. }.
            ]
            | [ ^ { $getCard = theForm $getPlayerCard:aPlayer. }. ].
        ]
    }.
}

// === ExtensionFactory ===

#symbol ClientExtensionFactory =
{
    create : aPlayer
    [
        #var anExtension := FormExtension.

        events'Handlers::aPlayer += anExtension.

        ^ anExtension.
    ]
}.

#symbol HostExtensionFactory =
{
    create : aPlayer
    [
        #var anExtension := FormExtension.

        events'Handlers::aPlayer += anExtension.

        ^ anExtension.
    ]
}.
