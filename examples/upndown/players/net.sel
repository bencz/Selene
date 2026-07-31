// --- namespace shortcuts ---
#define basic'*   = std'basic'*.
#define prop'*    = std'properties'*.
#define coll'*    = std'collections'*.
#define events'*  = sys'events'*.

#define net'*     = cardgame'net'*.
#define players'* = cardgame'players'*.
#define game'*    = cardgame'game'*.
#define engine'*  = cardgame'engine'*.
#define cards'*   = cardgame'engine'cards'*.

// --- HostExtension ---

#class HostExtension
{
    #method engine'$ifSynchronized
    [
    ]
}

// --- PlayerExtension ---

#class PlayerExtension
{
    #field theSocket.

    #method $onNew : aProfile
    [
        theSocket := aProfile $getSocket.
    ]

    #method net'$reactCommand : aCommand
    [
        #if aCommand 
            == net'PrepareGameID ?
            [             
                #var Players := coll'List.
    
                aCommand >> 
                {
                    prop'$readFrom : aReader
                    [
                        #var aCount := basic'Integer.
                        aCount $invoke:aReader.
        
                        #loop(aCount > 0)?
                        [
                            #var aPlayer := players'Base.
    
                            aPlayer $invoke:aReader.
            
                            Players += aPlayer.
    
                            aCount -= 1.
                        ].
                    ]
                }.
    
                self $doPrepare:Players.
            ]
            | == net'NewRoundID ?
            [
                #var aRound := aCommand >> game'RoundAdapter::'RuleBook.
    
                self $doNewRound:aRound.    
            ]
            | == net'NewLeadingRoundID ?
            [ 
                #var aRound := aCommand >> game'RoundAdapter::'RuleBook.
    
                self $doNewRound:aRound.
            ]
            | ==net'NewCardID ?
            [ 
                #var aCard := aCommand >> cards'CardAdapter.
    
                self $doDistributeCard:aCard.
            ]
            | == net'NewTrumpID ?
            [ 
                #var aCard := aCommand >> cards'CardAdapter.
    
                self $doSelectTrump:aCard.
            ]
            | == net'OrderRequestID ?
            [ 
                #var aValidator := aCommand >> game'ValidatorAdapter.
    
                players'Client $makeOrder:#annex(aValidator)
                { 
                    << anOrder
                    [
                        #if(aValidator validate:anOrder)?
                        [
                            prop'Writer::theSocket << net'OrderResponseCommand::anOrder.
                        ]
                        | [ self fail. ].
                    ]
                }.
            ]
            | == net'OrderedID ?
            [             
                #var anOrdered := basic'Integer.
                #var anID := basic'Integer.
    
                aCommand >> {
                    prop'$readFrom : aReader
                    [
                        anID $invoke:aReader.
                        anOrdered $invoke:aReader.
                    ]
                }.
    
                players'Client $doOrdered:#annex(anOrdered){ prop'$getKey = anID. }.
            ]
            | == net'BeforeMoveID ?
            [
                #var anID := basic'Integer.
    
                aCommand >> anID.
    
                players'Client $doBeforeMove:{ prop'$getKey = anID. }.
            ]
            | == net'GetCardID ?
            [
                #var aValidator := aCommand >> game'ValidatorAdapter.
    
                players'Client $putCard:#annex(aValidator)
                { 
                    << aCard
                    [
                        #var aTable := self $getTable.

                        #if(aValidator validate:#annex(aTable) { $getCard = aCard. } )?
                        [
                            prop'Writer::theSocket << net'GetCardResponseCommand::aCard.
                        ]
                        | [ self fail. ].
                    ]
                }.
            ]
            | == net'CardOnTableID ?
            [
                #var aCard := basic'Integer.
                #var anID := basic'Integer.
    
                aCommand >> {
                    prop'$readFrom : aReader
                    [
                        anID $invoke:aReader.
                        aCard $invoke:aReader.
                    ]
                }.
    
                players'Client $doShowCard:#annex(aCard){ prop'$getKey = anID. }.
            ].
    ]

    #method $onPrepare : Players
    [
        prop'Writer::theSocket << net'SynchroCommand.
    ]

    #method $onNewLeadingRound : aRound
    [
        prop'Writer::theSocket << net'SynchroCommand.
    ]

    #method $onNewRound : aRound
    [
        prop'Writer::theSocket << net'SynchroCommand.
    ]

    #method $distributeCard : aCard
    [
        prop'Writer::theSocket << net'SynchroCommand.
    ]

    #method $selectTrump : aCard
    [
        prop'Writer::theSocket << net'SynchroCommand.
    ]

    #method $onOrdered : anOrdered
    [
        prop'Writer::theSocket << net'SynchroCommand.
    ]

    #method $onBeforeMove : aPlayerID
    [
        prop'Writer::theSocket << net'SynchroCommand.
    ]

    #method $onBeforeYourMove : aPlayerID
    [
        prop'Writer::theSocket << net'SynchroCommand.
    ]

    #method $onShowCard : aCard
    [
        prop'Writer::theSocket << net'SynchroCommand.
    ]

    #method $putCard : aCard
    [
        
    ]
}

// --- RemotePlayerExtension ---

#class RemotePlayerExtension
{
    #field theSocket.
    #field theResponse.

    #role Responded
    {
        #method $getResponse
        [
            #shift.

            #var aResponse := theResponse.

            theResponse := basic'nil.
    
            ^ aResponse.
        ]
    }

    #role Unsynchronized
    {
        #method engine'$ifSynchronized
        [
            self fail.
        ]

        #method $ifUnsynchronized []

        #method $reset
        [
        ]

        #method $synchronize
        [
            #shift.
        ]
    }

    #method $setResponse : aResponse
    [
        theResponse := aResponse.

        #shift Responded.
    ]

    #method $waitForResponse
    [
        #loop $self $ifUnsynchronized
        [
            'program continue.
        ].

        ^ self $getResponse.
    ]

    #method engine'$ifSynchronized
    [
    ]

    #method $reset
    [
        #shift Unsynchronized.
    ]

    #method $synchronize
    [
    ]

    #method $onNew : aProfile
    [
        theSocket := aProfile $getSocket.
    ]

    #method $onPrepare : Players
    [
        self $reset.

        #var aCommand := net'PrepareGameCommand::
        {
            prop'$writeTo : aWriter
            [
                basic'IntegerType of:(Players Count) $invoke:aWriter.

                #var anEnum := prop'Enumerator::Players.
                #loop anEnum next 
                [
                    anEnum get $invoke:aWriter.
                ].
            ]
        }.

        prop'Writer::theSocket << aCommand.
    ]

    #method players'$onNewLeadingRound : aRound
    [
        self $reset.

        #var aCommand := net'NewLeadingRoundCommand::aRound.

        prop'Writer::theSocket << aCommand.
    ]

    #method players'$onNewRound : aRound
    [
        $self $reset.

        #var aCommand := net'NewRoundCommand::aRound.

        prop'Writer::theSocket << aCommand.
    ]

    #method $distributeCard : aCard
    [
        $self $reset.

        #var aCommand := net'NewCardCommand::aCard.

        prop'Writer::theSocket << aCommand.
    ]

    #method $selectTrump : aCard
    [
        $self $reset.

        #var aCommand := net'NewTrumpCommand::aCard.

        prop'Writer::theSocket << aCommand.
    ]

    #method $makeOrder : anOrder
    [
        $self $reset.

        #var aCommand := net'OrderRequestCommand::anOrder.
        prop'Writer::theSocket << aCommand.

        anOrder << $self $waitForResponse.
    ]

    #method $onOrdered : anOrdered
    [
        $self $reset.

        #var aData := 
        {
            prop'$writeTo : aWriter
            [
                prop'ContentKey::anOrdered $invoke:aWriter.
                basic'IntegerType of:anOrdered $invoke:aWriter.
            ]
        }.

        #var aCommand := net'OrderedCommand::aData.
        prop'Writer::theSocket << aCommand.
    ]

    #method players'$onBeforeMove : aPlayer
    [
        $self $reset.

        #var aCommand := net'BeforeMoveCommand::prop'ContentKey::aPlayer.
        prop'Writer::theSocket << aCommand.
    ]

    #method players'$onBeforeYourMove : aPlayer
    [
        $self $reset.

        #var aCommand := net'BeforeMoveCommand::prop'ContentKey::aPlayer.
        prop'Writer::theSocket << aCommand.
    ]

    #method players'$onShowCard : aCard
    [
        #var aCommand := net'CardOnTableCommand::aCard.
        prop'Writer::theSocket << aCommand.
    ]

    #method $putCard : aCardValidator
    [
        $self $reset.

        #var aCommand := net'GetCardCommand::aCardValidator.
        prop'Writer::theSocket << aCommand.

        aCardValidator << $self $waitForResponse.
    ]

    #method net'$reactCommand : aCommand
    [
        #if aCommand
            == net'SynchroID ?
            [ self $synchronize. ]
            |  == net'OrderResponseID ?
            [ 
                #var anOrder := aCommand >> basic'Integer.
    
                self $setResponse:anOrder.
            ]
            | == net'GetCardResponseID ?
            [ 
                #var aCard := aCommand >> basic'Integer.
    
                self $setResponse:aCard.
            ].
    ]
}

// === ExtensionFactory ===

#symbol HostExtensionFactory =
{
    create : aPlayer
    [
        ^ HostExtension.
    ]
}.

#symbol ClientExtensionFactory =
{
    create : aPlayer
    [
        #var anExtension := PlayerExtension.

        events'Handlers::aPlayer += anExtension.

        ^ anExtension.
    ]
}.

#symbol RemotePlayerExtensionFactory =
{
    create : aPlayer
    [
        #var anExtension := RemotePlayerExtension.

        events'Handlers::aPlayer += anExtension.

        ^ anExtension.
    ]
}.