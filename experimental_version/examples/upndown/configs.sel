// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.
#define coll'*  = std'collections'*.

#define gui'*   = cardgame'gui'forms'*.
#define net'*   = cardgame'net'*.

// --- ExtensionFactory ---

#symbol ExtensionFactoryType =
{
    of : aProfile = aProfile $getFactoryCollection.
}.

// --- FactoryCollection ---

#class FactoryCollection
{
    #field theList.

    #method new
    [
        theList := coll'List.
    ]

    #method create : anObject
    [
        #var aCollection := coll'List.

        prop'Enumerator::theList run: aFactory => ( aCollection += aFactory create:anObject ).

        #var Extensions := basic'ArrayType create:{ Length = aCollection Count. }.

        prop'IndexEnumerator::aCollection run: anExtension =>
        [
            prop'Content::(Extensions@prop'Index::anExtension) set:prop'ContentValue::anExtension.
        ].

        ^ Extensions.
    ]

    #method += aFactory
    [
        theList += aFactory.
    ]
}

// --- HostPlayerExtensionFactories ---

#static HostPlayerExtensionFactories = FactoryCollection.

// --- ClientPlayerExtensionFactories ---

#static ClientPlayerExtensionFactories = FactoryCollection.

// --- RemotePlayerExtensionFactories ---

#static RemotePlayerExtensionFactories = FactoryCollection.

// --- MasterExtensionFactories ---

#static MasterExtensionFactories = FactoryCollection.

// --- Modules ---

#symbol Modules =
(
    gui'Module proceed,
    net'Module proceed
).
