// --- namespace shortcuts ---
#define basic'*  = std'basic'*.
#define system'* = win32'system'*.

#define api'*    = win32'api'*.
#define events'* = sys'events'*.

// --- SDI ---

#class $SDIApp
{
    #field theMainWindow.
    #field theLoopAction.

    #role Stopped
    {
        #method continue
        [
            self fail.
        ]
    }

    #method $doMainLoop
    [
        #var aHWND := api'HWNDType of:theMainWindow.
        #loop theLoopAction proceed:aHWND.
    ]

    #method new
    [
        theLoopAction := api'SDILoopAction::api'MSG.

        events'Handlers::'system += 
        {
            system'$onStart [ self $onStart. ] 

            system'$onStop [ self $onStop. ]
        }.
    ]

	#method continue
	[
        theLoopAction proceed:0.
	]

    #method proceed
    [
        theMainWindow open.

        self $doMainLoop.
    ]

    #method exit : aCode
    [
        #if aCode ifNotNil
        [
            theMainWindow $exit:(basic'IntegerType of:aCode).
        ]
        | [
            theMainWindow $exit:0.
        ].
    ]

    #method $OnStart 
    [
        theMainWindow := 'MainWindow.
    ]

    #method $onStop
    [
        #shift Stopped.
    ]
}

#static SDIApp = $SDIApp.
 