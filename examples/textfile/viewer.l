#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.

#symbol Program =
    Arguments =>
    [
        #if (Arguments Count == 1)?
            [  'program'output << "Please provide the path to the file to view". $self fail. ].

        #var aFileName := basic'LiteralType of:(Arguments@1).

        prop'Enumerator::ext'io'TextFile::aFileName run:ext'patterns'Printing.

        'program'input get. // wait for any key
    ].