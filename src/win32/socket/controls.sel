// --- namespace shortcuts ---
#define basic'*     = std'basic'*.
#define prop'*      = std'properties'*.
#define list'*      = std'collections'*.
#define ctrl'*      = std'patterns'*.
#define events'*    = sys'events'*.

#define api'*       = win32'api'*.
#define styles'*    = win32'api'styles'*.
#define prims'*     = win32'socket'primitives'*.
#define consts'*    = win32'api'constants'*.
#define controls'*  = win32'api'controls'*.
#define factories'* = win32'api'factories'*.
#define sockets'*   = win32'socket'controls'*.

#define gui'*       = gui'controls'*.

// --- compiler hints ---
#define[const] IndexerExtension = prop'IndexerExtension.
#define[const] ListExtension    = list'ListExtension.

// --- Constants ---

#symbol WSM_NOTIFY = consts'WM_USER + 1.

#symbol FD_READ     = 01h.
#symbol FD_WRITE    = 02h.
#symbol FD_ACCEPT   = 08h.
#symbol FD_CONNECT  = 10h.
#symbol FD_CLOSE    = 20h.

// --- CustomSocketStyles ---

#symbol CustomSocketStyles =
{
    api'$getWindowHandle = controls'CustomControl.

    api'$getState : aControl = 'system += aControl.

    api'$getStyle = consts'WS_OVERLAPPED.

    api'$getExStyle = 0.

    api'$getClass = factories'NonvisibleWindowClass.
}.

// --- CustomSocket ---

#class CustomSocket
{
    #field theParent.
    #field theHandle.
    #field theEvents.

    #field thePort.
    #field theSocket.

    #method $getPort = thePort.

    #method new : aParent
    [
        thePort := basic'Integer.
        theEvents := events'Controller.

        prims'SocketInfo ifNotNil.

        theParent := aParent.
        #var anOwner := styles'Owner::self.
        theHandle := api'HWNDType create:#group(CustomSocketStyles, styles'Parent::aParent, anOwner).
//
//        theHandle->sys'events'Handlers +=
//        { 
//           api'$onUserEvent : aMessage = self $onSocketEvent:aMessage. 
//           api'$onClose = self::$onClose. 
//        }.
    ]

    #method close []

    #method prop'$getReader = theSocket $invoke.

    #method prop'$getWriter = theSocket $invoke.

    #method events'$getController = theEvents.

    #annex(theHandle).
}

// --- ServerSocket ---

#class ServerSocket(CustomSocket)
{
    #field theRemoteSockets.

    #role Listening
    {
        #method open [ self fail. ]
    
	    #method api'$onUserEvent : aMessage
	    [
	        #var aLParam := api'LParam::aMessage.
            #var aWParam := api'WParam::aMessage.

            #if aLParam 
                == FD_ACCEPT ? [ self $accept. ]
                | == FD_READ ? 
                [ 
                    theEvents $onRead:
                        (ctrl'SearchAgent::theRemoteSockets proceed:{ == aControl = aWParam == prims'SocketType of:aControl. }).
                ].
    	    ]

        #method close
        [
            prop'Enumerator::theRemoteSockets run:
                aSocket => (aSocket close).

            theRemoteSockets new.

            theSocket close.

            #shift.
        ]

        #method api'$onClose
        [
            self close.

            ^ super invoke.
        ]
    }

    #method new : aParent
    [
        super new:aParent.

        theRemoteSockets := list'List.
    ]

    #method open
    [
        theSocket := prims'ListenerSocketType create:thePort open.

        #shift Listening.
 
        #external winsock'1(theSocket, api'HWNDType of:theHandle, 
            WSM_NOTIFY, 
            FD_READ or:FD_WRITE or:FD_CONNECT or:FD_CLOSE or:FD_ACCEPT).
    ]

    #method $accept
    [
        #var aRemoteSocket := sockets'RemoteSocket::(prims'RemoteSocketType of:theSocket).

        theRemoteSockets += aRemoteSocket.

        theEvents $onServerAccept:aRemoteSocket.
    ]

    #method api'$onUserEvent : aMessage
    [
//	    #var aLParam := aMessage::getLParam.
    ]

    #method prop'$getEnumerator = theRemoteSockets $invoke.

    #annex (ListExtension, IndexerExtension).
}

// --- ClientSocket ---

#class ClientSocket(CustomSocket)
{
    #field theIPAddress.

    #role Connecting
    {
        #method open [ self fail. ]

        #method close
        [
            theSocket close.

            #shift.
        ]    

	    #method api'$onUserEvent : aMessage
	    [
	        #var aLParam := api'LParam::aMessage.
            #if(aLParam == FD_CONNECT)? [ self $onConnect. ].

            #if(basic'HiWord::aLParam != 0)?
            [
                #if(basic'LoWord::aLParam==FD_CONNECT)? [ theEvents $onConnectRefuse. ].
            ].
	    ]

        #method $onClose
        [
            self close.
        ]    
    }

    #role Connected
    {
 	    #method $onDisconnect
	    [
	        #shift.
	
            theEvents $onSocketDisconnect:self.
	    ]

	    #method api'$onUserEvent : aMessage
	    [
	        #var aLParam := api'LParam::aMessage.
            #if(aLParam == FD_CLOSE)? [ self $onDisconnect. ].
            #if(aLParam == FD_READ)? [ theEvents $onRead:self. ].
	    ]

        #method $onClose
        [
            self close.
        ]

        #method close
        [
            theSocket close.

            #shift.
        ]    
    }

    #method $getIPAddress = theIPAddress.

    #method new : aParent
    [
        super new:aParent.

        theIPAddress := basic'String.
    ]

    #method $onConnect
    [
        #shift Connected.

        theEvents $onSocketConnect:self.
    ]

    #method $markAsConnected [ #shift Connecting. ]

    #method open
    [
        #var aSocketProxy :=  prims'ClientSocketType create:{ prims'$getPort = thePort. prims'$getIPAddress = theIPAddress. }.
        theSocket := prims'ClientSocketType of:aSocketProxy.

        #external winsock'1(theSocket, api'HWNDType of:theHandle,
            WSM_NOTIFY, 
            FD_READ or:FD_WRITE or:FD_CONNECT Or:FD_CLOSE).

        self $markAsConnected.

        aSocketProxy open | ifNotNil.
    ]
}

// --- RemoteSocket ---

#class RemoteSocket
{
    #field theSocket.
    #field theObject.

    #method new : aSocket
    [
        theSocket := aSocket.
    ]

    #method close
    [
        theSocket close.
    ]

    #method gui'$getTag = theObject.

    #method gui'$setTag : anObject
    [
        theObject := anObject.
    ]

    #annex (theSocket).
}

// === Properties ===

// --- Port ---

#symbol Port : aControl
    = aControl $getPort.

// --- IPAddress ---

#symbol IPAddress : aControl
    = aControl $getIPAddress.
