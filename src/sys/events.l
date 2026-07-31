// --- namespace shortcuts ---
#define ctrl'*  = std'patterns'*.

// --- Controller ---

#class Controller
{
    #field theEvents.

    #role EOF
    {
        #method $getEvents = ctrl'Control fail.

        #method $setController : aGroup
        [
            #shift.

            self $setEvents : aGroup.
        ]
    }

    #method new
    [
        #shift EOF.

        theEvents := #cast(nil).
    ]

    #method $getEvents = theEvents.

    #method $setEvents : aGroup
    [
        theEvents := aGroup.
    ]

    #annex theEvents.
}

// --- GroupAdapter ---

#symbol GroupAdapter =
{
    ctrl'$getGroup = self $getController $getEvents.

    ctrl'$setGroup : aGroup = self $getController $setEvents:aGroup.
}.

// --- Handlers ---

#symbol Handlers : anObject =
{
    += aHandler
    [
        #group(GroupAdapter, ctrl'DynamicCast, anObject) += aHandler.
    ]        
}.

// --- ControllerType ---

#symbol ControllerType =
{
    of : anObject = anObject $getController.
}.