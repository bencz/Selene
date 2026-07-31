// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define ctrl'*  = std'patterns'*.
#define list'*  = std'collections'*.

// --- CommandLineArguments ---

#symbol CommandLineArguments = ctrl'control run: =>
[
    #var aCommandLine := 'program'CommandLine.
    #var aCollection := list'List.

    ext'text'WordParsing::aCommandLine run: anArgument =>
    [
        aCollection += basic'LiteralType of:anArgument.
    ].

    ^ list'ReadOnly::aCollection.
].

// === Base system patterns ===

// --- Simple ---

#symbol Simple = 'Program proceed:CommandLineArguments.

// --- System ---

#symbol System
    = 'system run:('program ifNotNil:'program'Modules).

// --- CommandCycle ---

#symbol CommandCycle = 'Program run: aController => 
[
	#loop ctrl'Break != aController?
    [ 
        'program'input >> aController.
        'program'output << aController. 
    ].
].

// --- Default modules ---

#symbol IdleModules = { ifNotNil [] }. // doesn't metter
