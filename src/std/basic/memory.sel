// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.
#define mem'*   = std'basic'memory'*.

// === Memory container ===

// --- ByteArray ---

#class ByteArray
{
    #field.

    // class methods

    #method[def] $getClassName = "std'basic'memory'ByteArray".

    #method[def] $typeCast : anObject = anObject $getByteArray.

    // private methods

    #method $getByteArray = $self.

    #method $getBOF = 0.

    #method prop'$getReader = mem'ByteArrayReader::self. 

    #method prop'$getWriter = mem'ByteArrayWriter::self. 

    // public methods

    #method Length = #inline standard'75(basic'IntNumber, $self).

    #method new
    [
        self fail.
    ]
}

// --- Buffer ---

#static BufferGroup = mem'BufferType $getGroup.

#class Buffer
{
    #field theDump.

    // class methods

    #method[def] $getClassName = "std'basic'memory'ByteArray".

    #method[def] $typeCast : anObject = anObject $getByteArray.

    // private methods

    #method $getBOF = 4.

    #method $getByteArray = theDump.

    #method $getDump = $self.        

    #method $getSize = theDump Length - 4.

    #method prop'$getReader = mem'BufferReader::self. 

    #method prop'$getWriter = mem'BufferWriter::self. 

    // public methods

    #method new : aSize
    [
        #if aSize ifNotNil
            [ aSize := aSize Length. ]
            | [ aSize := 64. ].

        theDump := mem'ByteArrayType $create:aSize.

        self clear.
    ]

    #method Length = #inline standard'3(basic'IntNumber, theDump).

    #method << aDump
    [
        #inline standard'76(theDump, aDump $getByteArray).
    ]

    #method clear
    [
        #inline standard'3(theDump $getByteArray, 0).
    ]

    #annex BufferGroup.
}

// --- ByteArrayReader ---

#class ByteArrayReader
{
    #field theDump.
    #field theOffset.

    #method $getOffset = theOffset.

    #method Position = basic'IntNumber::theOffset.

    #method new : aDump
    [
        theDump := aDump $getByteArray.
        theOffset := basic'Integer << aDump $getBOF.
    ]

    #method seek : aLocation
    [
        theOffset << aLocation Position.
    ]

	#method $readAsInt32 : aVariable
	[
		#var aValue := basic'IntNumber.

		#inline standard'61(aValue, theDump, theOffset).

		theOffset += 4.

		aVariable << aValue.
	]

	#method $readAsLiteral : aValue
	[
		#inline standard'77(aValue $getLiteral, theDump, theOffset).
		
		theOffset += (aValue Length * 2).
	]    

    #method $readAsBuffer : aBuffer
    [
		#inline standard'78(aBuffer $getByteArray, theDump, theOffset).

		theOffset += aBuffer Length.
    ]
}

// --- ByteArrayWriter ---

#class ByteArrayWriter
{
	#field theDump.
	#field theOffset.

    #method $getOffset = theOffset.

    #method Position = basic'IntNumber::theOffset.

	#method new : aDump
	[
		theDump := aDump $GetByteArray.
		theOffset := basic'Integer << aDump $getBOF.
	]

    #method seek : aLocation
    [
        theOffset << aLocation Position.
    ]

    #method $writeAsLiteral : aValue
    [
        #inline standard'79(theDump, theOffset, aValue $getLiteral).

		theOffset += (aValue Length * 2).
    ]

    #method basic'$writeAsInt32 : aValue
    [
        #inline standard'80(theDump, theOffset, aValue $getInteger).

		theOffset += 4.
    ]

    #method $writeAsBuffer : aBuffer
    [
		#inline standard'81(aBuffer $getByteArray, theDump, theOffset).
		
        theOffset += aBuffer Length.
    ]
}

// --- BufferReader ---

#class BufferReader
{
    #field theReader.
    #field theBuffer.

    #method Position = theReader Position.

	#method new : aBuffer
	[
        theBuffer := aBuffer.
        theReader := mem'ByteArrayReader::aBuffer.
	]

    #method seek : aLocation
    [
        #var anOffset := aLocation Position.

        basic'True if:(anOffset <= theBuffer Length).

        theReader seek:{ Position = anOffset + 4. }.
    ]

	#method $readAsInt32 : aVariable
	[
        theReader $invoke:aVariable.

        basic'True if:(theBuffer Length >= (theReader $getOffset - 4)).
	]

	#method $readAsLiteral : aValue
	[
		theReader $invoke:aValue.		

        basic'True if:(theBuffer Length >= (theReader $getOffset - 4)).
	]

	#method $readAsBuffer : aBuffer
	[
		#var aSize := theBuffer Length - (theReader $getOffset - 4).

        #if(aSize < aBuffer $getSize)?
        [
            #inline standard'3(aBuffer $getByteArray, aSize).
        ].

		theReader $invoke:aBuffer.		
	]
}

// --- BufferWriter ---

#class BufferWriter
{
    #field theBuffer.
    #field theWriter.

    #method Position = theWriter Position.

	#method new : aBuffer
	[
        theBuffer := aBuffer.
        theWriter := mem'ByteArrayWriter::aBuffer.
	]

    #method seek : aLocation
    [
        #var anOffset := aLocation Position.

        basic'True if:(anOffset <= theBuffer Length).

        theWriter seek:{ Position = anOffset + 4. }.
    ]

    #method $writeAsLiteral : aValue
    [
        theWriter $invoke:aValue.

        #inline standard'3(theBuffer $getByteArray, theWriter $getOffset - 4).
    ]

    #method $writeAsInt32 : aValue
    [
        theWriter $invoke:aValue.

        #inline standard'3(theBuffer $getByteArray, theWriter $getOffset - 4).
    ]

    #method $writeAsBuffer : aBuffer
    [
        theWriter $invoke:aBuffer.

        #inline standard'3(theBuffer $getByteArray, theWriter $getOffset - 4).
    ]    
}

// === Memory extensions ===

// --- BufferExtension ---

#class BufferExtension
{
    #method prop'$readFrom : aReader
    [
        aReader $readAsBuffer:self.
    ]

    #method prop'$writeTo : aWriter
    [
        aWriter $writeAsBuffer:self.
    ]
}

// --- ByteArrayType ---

#symbol ByteArrayType =
{
    of : anObject = anObject $getByteArray.

    create : aParameter
    [
        ^ #inline standard'82 (#type mem'ByteArray, aParameter Length).
    ]
}.

// --- BufferType ---

#symbol BufferType =
{
    $getGroup = BufferExtension.

    of : anObject = anObject $getDump.

    create : aParam = mem'Buffer::aParam.
}.


