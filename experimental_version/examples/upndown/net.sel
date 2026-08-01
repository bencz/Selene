// --- namespace shortcuts ---
#define basic'*       = std'basic'*.
#define memory'*      = std'basic'memory'*.
#define prop'*        = std'properties'*.

#define config'*      = cardgame'configs'*.
#define players'net'* = cardgame'players'net'*.
#define engine'net'*  = cardgame'engine'net'*.

// --- Module ---

#symbol Module =
{
    proceed
    [
        config'HostPlayerExtensionFactories += players'net'HostExtensionFactory.

        config'ClientPlayerExtensionFactories += players'net'ClientExtensionFactory.

        config'RemotePlayerExtensionFactories += players'net'RemotePlayerExtensionFactory.

        config'MasterExtensionFactories += engine'net'MasterExtensionFactory.
    ]
}.

// --- Command codes ---

#symbol SynchroID         = 1.
#symbol AcceptID          = 2.
#symbol AcceptReplyID     = 3.
#symbol PlayerAcceptedID  = 4.
#symbol PrepareGameID     = 5.
#symbol NewRoundID        = 6.
#symbol NewLeadingRoundID = 7.
#symbol NewCardID         = 8.
#symbol NewTrumpID        = 9.
#symbol OrderRequestID    = 10.
#symbol OrderResponseID   = 11.
#symbol OrderedID         = 12.
#symbol BeforeMoveID      = 13.
#symbol GetCardID         = 14.
#symbol GetCardResponseID = 15.
#symbol CardOnTableID     = 16.

// --- Command ---

#class Command
{
    #field theID.
    #field theData.

    #role Empty
    {
        #method << aData
        [
            theData := memory'Buffer.

            #shift.

            self << aData.
        ]

        #method prop'$readFrom : aReader
        [
            theData := memory'Buffer.

            theData $invoke:aReader.
        ]

        #method prop'$writeTo : aWriter
        [
            theID $invoke:aWriter.        
            0 $invoke:aWriter.
        ]
    }

    #method new : anID
    [
        theID := basic'Integer << anID.

        #shift Empty.
    ]

    #method == anID 
    [
        ^ (theID == anID).
    ]

    #method << aData
    [
        #var aWriter := prop'Writer::theData.

        aWriter seek:{ Position = theData Length. }.

        aWriter << aData.
    ]

    #method >> aData
    [
        ^ prop'Reader::theData >> aData.
    ]

    #method prop'$writeTo : aWriter
    [
        theID $invoke:aWriter.        
        theData Length $invoke:aWriter.

        theData $invoke:aWriter.
    ]

    #method prop'$readFrom : aReader
    [
        theData $invoke:aReader.
    ]

    #method proceed : anObject
    [
        anObject $reactCommand:self.
    ]
}

// --- AcceptCommand ---

#symbol AcceptCommand : aHostPlayer
    = Command::AcceptID << aHostPlayer.

#symbol AcceptReplyCommand : aClientPlayer
    = Command::AcceptReplyID << aClientPlayer.

#symbol PlayerAcceptedCommand : aClientID
    = Command::PlayerAcceptedID << aClientID.

// --- PrepareGameCommand ---

#symbol PrepareGameCommand : Players
    = Command::PrepareGameID << Players.

// --- SynchroCommand ---

#symbol SynchroCommand
    = Command::SynchroID.

// --- NewRoundCommand ---

#symbol NewRoundCommand : RoundInfo
    = Command::NewRoundID << RoundInfo.

#symbol NewLeadingRoundCommand : RoundInfo
    = Command::NewLeadingRoundID << RoundInfo.

// --- NewCardCommand ---

#symbol NewCardCommand : aCard
    = Command::NewCardID << 0 << aCard.

// --- NewTrumpCommand ---

#symbol NewTrumpCommand : aCard
    = Command::NewTrumpID << 0 << aCard.

// --- OrderCommand ---

#symbol OrderRequestCommand : anOrder
    = Command::OrderRequestID << anOrder.

#symbol OrderResponseCommand : anOrder
    = Command::OrderResponseID << anOrder.

// --- OrderedCommand ---

#symbol OrderedCommand : anOrdered
    = Command::OrderedID << anOrdered.

// --- BeforeMoveCommand ---

#symbol BeforeMoveCommand : aPlayerID
    = Command::BeforeMoveID << aPlayerID.

// --- GetCardCommand ---

#symbol GetCardCommand : aCard
    = Command::GetCardID << aCard.

// --- GetCardResponseID ---

#symbol GetCardResponseCommand : aCard
    = Command::GetCardResponseID << aCard.

// --- CardOnTableCommand ---

#symbol CardOnTableCommand : aCard
    = Command::CardOnTableID << prop'ContentKey::aCard << aCard.

// --- CommandFactory ---

#class $CommandFactory
{
    #field theBuffer.
    #field theID.
    #field theSize.

    #role Empty
    {
        #method prop'$readFrom : aReader
        [
            theBuffer clear.

            theBuffer $invoke:aReader.

            #shift.

            ^ self $invoke:aReader.
        ]
    }

    #role Continue
    {
        #method prop'$readFrom : aReader
        [
            theBuffer $invoke:aReader.

            #shift.

            ^ self $invoke:aReader.
        ]
    }

    #method prop'$readFrom
    [
        #var aBufferLen := theBuffer Length.
        #if(aBufferLen < 8)?
        [
            #shift Continue.

             ^self fail.
        ].

        #var aReader := prop'Reader::theBuffer.
        aReader >> theID.
        aReader >> theSize.

        #if(0 == theSize)?
        [ 
            #shift Empty.

            ^ Command::theID. 
        ].

        #if(theSize > (aBufferLen - 8))?
        [
            #shift Continue.

             ^self fail. 
        ]
        | [ #shift Empty. ].

        ^ aReader >> Command::theID.
    ]

    #method new
    [
        theBuffer := memory'Buffer.

        theID := basic'Integer << 0.
        theSize := basic'Integer << 0.

        #shift Empty.
    ]
}

#static CommandFactory = $CommandFactory.
