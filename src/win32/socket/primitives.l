// --- namespace shortcuts ---
#define basic'*  = std'basic'*.
#define prop'*   = std'properties'*.
#define memory'* = std'basic'memory'*.

#define events'* = sys'events'*.

#define system'* = win32'system'*.
#define api'*    = win32'api'*.
#define consts'* = win32'api'constants'*.
#define prims'*  = win32'socket'primitives'*.

// --- SocketInfo--

#class $SocketInfo
{
    #role Released
    {
        #method close []
    }

    #method new
    [
        events'Handlers::'system
            += { system'$onStop = self close. }.
    ]

    #method close
    [
        #external winsock'2(self).

        #shift Released.
    ]
}

#static SocketInfo = #external winsock'3($SocketInfo, 101h).

// --- SOCKET ---

#class SOCKET (win32'api'DWORD)
{
    #method $getSocket = $self.

    #method close
    [
        #external winsock'4($self).
    ]

    #method prop'$getReader = prims'SOCKETReader::self.

    #method prop'$getWriter = prims'SOCKETWriter::self.
}

// --- ListenerSOCKET ---

#class ListenerSOCKET(SOCKET)
{
    #method open
    [
        #external winsock'5($self).
    ]
}

// --- SOCKETReader ---

#class SOCKETReader
{
    #field theHandle.

    #method new : aHandle
    [
        theHandle := aHandle $getSocket.
    ]
/*
    #method $$readAsInt32 : aValue
    [
        #external winsock'11(theHandle, aValue, 0, 4).
    ]

    #method $$readAsLiteral : aLiteral
    [
        #external winsock'11(theHandle, aLiteral, 4, aLiteral->basic'Length * 2).

        #inline standard'70(theHandle, aLiteral).
    ]
*/
    #method basic'$readAsBuffer : aBuffer
    [
        #external winsock'6(theHandle, memory'ByteArrayType of:aBuffer).
    ]
}

// --- SOCKETWriter ---

#class SOCKETWriter
{
    #field theHandle.

    #method new : aHandle
    [
        theHandle := aHandle $getSocket.
    ]

    #method basic'$writeAsInt32 : aValue
    [
        #external winsock'7(theHandle, basic'IntegerType of:aValue).
    ]

    #method basic'$writeAsLiteral : aValue
    [
        #external winsock'8(theHandle, basic'LiteralType of:aValue).
    ]

    #method basic'$writeAsBuffer : aBuffer
    [
        #external winsock'9(theHandle, memory'ByteArrayType of:aBuffer).
    ]
}

// === Factories ===

#symbol ListenerSocketType =
{
    create : aPort = #external winsock'10(prims'ListenerSOCKET, basic'IntegerType of:aPort).
}.

// --- RemoteSocketType ---

#symbol RemoteSocketType =
{
    of : aListener = #external winsock'11(prims'SOCKET, aListener $getSocket).
}.

// --- RemoteSocketType ---

#symbol ClientSocketType =
{
    of : anObject = anObject $getSocket.

    create : aProfile
    [
        #var aSocket := #external winsock'12(prims'SOCKET).

        ^ #annex(aSocket) 
        {
            open
            [
                #var aPort := basic'IntegerType of:(aProfile $getPort).
                #var anIPAddress := api'LPSTR::(aProfile $getIPAddress).
        
                #external winsock'13(aSocket $getSocket, anIPAddress, aPort).                
            ]
        }.
    ]
}.

// --- SocketType ---

#symbol SocketType =
{
    of : anObject = anObject $getSocket.
}.

