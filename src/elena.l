// --- Object ---

#class Object
{
    #method new      []

    #method ifNotNil []

    #method back : aParam = aParam.

    #method ifSame : anObject = #inline elena'22 (self, anObject).
}

// --- Nil ---

#class $Nil
{
    #method ifNotNil [ self fail. ]
}

// --- $Instance ---

#class $TypeInstance
{
    #field(4).

    #method $getClassName : aParameter = #inline elena'23 (self, aParameter).

    #method $typecast : aParameter = #inline elena'23 (self, aParameter).

    #method $ifSameType : anObject
    [
        #inline elena'24 (self, anObject).
    ]
}

// --- Type ---

#symbol Type =
{
    Name = self $getClassName.

    ifSame : anObject = self $ifSameType:anObject.

    of : anObject = self $typecast:anObject.
}.