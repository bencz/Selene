// --- namespace shortcuts ---
#define basic'*    = std'basic'*.
#define gui'prop'* = gui'controls'properties'*.
#define controls'* = gui'controls'*.
#define events'*   = sys'events'*.

// --- Custom ---

#class Custom (controls'Container)
{
    #method new : aParent
    [
        super new:aParent.

        self $initialize.
    ]

    #method $initialize
    [
        controls'InitAction { for = self. } proceed:self.
    ]
}

// --- SDI ---

#class SDI (Custom)
{
    #method open
    [        
        theHandle open.

        theEvents $onOpen.
    ]

    #method controls'$createHandle
    [
        ^ 'gui'handlerType create:#group('gui'styles'SDIWindow, 'gui'styles'Owner::self).
    ]


    #method $initialize
    [
        events'Handlers::self +=
        { 
            controls'$onClose = self $onClose.     
        }.

        super $initialize.
    ]

    #method $onClose
    [
       'program exit.
    ]
}

// --- Dialog ---

#class Dialog (Custom)
{
    #field theResult.

    #role Modal
    {
        #method controls'$exitWithResult : aValue
        [
            theResult := aValue.

            self close.

            #shift.
        ]
    }

    #method new : aParent
    [
        theParent := aParent.
        theResult := nil.

        super new.
    ]

    #method controls'$createHandle 
    [
        ^ 'gui'handlerType create:#group('gui'styles'Dialog, 'gui'styles'Parent::theParent, 'gui'styles'Owner::self).
    ]

    #method open
    [
        theHandle open.

        theEvents $onOpen.

        #shift Modal.
        
        gui'prop'Enabled::theParent << basic'false.

        'gui'DialogLoopAction run:theHandle.

        gui'prop'Enabled::theParent << basic'true.

        ^ theResult.
    ]
}

// --- SDIDialog ---

#class SDIDialog (SDI)
{
    #method controls'$createHandle
    [
        ^ 'gui'handlerType create:#group('gui'styles'SDIDialog, 'gui'styles'Owner::self).
    ]
}
