// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.

// --- ListPrintAction ---

#class ListPrintAction
{
    #field theOutput.

    #role First
    {
        #method proceed : anItem
        [
            theOutput << anItem.

            #shift.
        ]    
    }

    // class methods

    #method[def] $getClassName = "ext'io'ListPrintAction".

    // public methods

    #method new : anOutput
    [
        theOutput := prop'TextWriter::anOutput.

        #shift First.
    ]

    #method proceed : anItem
    [
        theOutput << "," << anItem.
    ]
}

// --- ListPrinter ---

#symbol ListPrinter : aList
    = {
        prop'$printOn : aWriter
        [
            prop'Enumerator::aList run:
                ListPrintAction::{ prop'$getTextWriter = aWriter. }.
        ]
    }.

// --- Console ---

#symbol Console =
{
    << aValue
    [
        'program'Output << aValue.
    ]

    >> aVariable
    [
        ^ 'program'Input >> aVariable.
    ]
}.

// --- TextFileEnumerator ---

#class TextFileEnumerator
{
    #field theLine.
    #field theReader.

    #role BOF
    {
        #method next [ self start. ]

        #method start
        [
            #shift.

            self next.
        ]

        #method get [ self fail. ]
    }

    #method new : aStream
    [
        #shift BOF.

        theReader := prop'TextReader::aStream.
    ]

    #method next
    [
        theLine := theReader >> basic'LiteralType.
    ]

    #method get = theLine.
}

// --- TextFile ---

#symbol TextFile : aFileName =
{
    prop'$getEnumerator = TextFileEnumerator::'handlers'TextFileRO::aFileName.
}.

