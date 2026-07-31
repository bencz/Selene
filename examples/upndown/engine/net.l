// --- namespace shortcuts ---
#define events'* = sys'events'*.
#define ctrl'*   = std'patterns'*.

// --- TCPIPController ---

#symbol SynchronizeValidator =
{
    validate : anObject
    [
        anObject $ifSynchronized.
    ]
}.

#class TCPIPController
{
    #method $ifNotsynchronized : Players
    [
        #if(ctrl'ValidateAgent::Players proceed:SynchronizeValidator)?
            [ self fail. ].         
    ]

    #method $wait4Synchro : aPlayers
    [
        #loop self $ifNotsynchronized:aPlayers
        [
            'program continue.
        ].
    ]
}

#static Controller = TCPIPController.

// --- MasterExtension ---

#class MasterExtension
{
    #method $synchronize : Players
    [
        Controller $wait4Synchro:Players.
    ]
}

// === ExtensionFactory ===

#symbol MasterExtensionFactory =
{
    create : aMaster
    [
        #var anExtension := MasterExtension.

        events'Handlers::aMaster += anExtension.

        ^ anExtension.
    ]
}.
