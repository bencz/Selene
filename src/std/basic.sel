// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define prop'*  = std'properties'*.
#define ctrl'*  = std'patterns'*.

// === Basic prototype classes ===

// --- Indexer ---

#class Indexer
{
    #field theArray.
    #field theIndex.

    #method prop'$getIndexer = $self.

    #method prop'$getIndex = theIndex.

    #method prop'$goto : anIndex
    [
        #if basic'True if:(anIndex >= 0) if:(anIndex < theArray Length)
        [
            theIndex << anIndex.
        ]
        | [ self fail. ].
    ]

    #method prop'$next
    [
        theIndex += 1.

        #if (theArray Length == theIndex)?
        [
            theIndex -= 1.

            self fail.
        ].
    ]

    #method prop'$previous
    [
        theIndex -= 1.

        #if (theIndex < 0)?
        [
            theIndex += 1.

            self fail.
        ].
    ]
}

// --- Magnitude ---

#class Magnitude
{
    #method == aValue
    [
        ^ self $equal:aValue
            back:basic'True | back:basic'False.
    ]

    #method != aValue
    [
        ^ self $equal:aValue
            back:basic'False | back:basic'True.
    ]

    #method > aValue
    [
        ^ aValue $less:self
            back:basic'True | back:basic'False.
    ]

    #method < aValue
    [
        ^ self $less:aValue
            back:basic'True | back:basic'False.
    ]

    #method >= aValue
    [
        #if(self > aValue)?
            [ ^ basic'True. ]
            | [ ^ self == aValue. ].
    ]

    #method <= aValue
    [
        #if(self < aValue)?
            [ ^ basic'True. ]
            | [ ^ self == aValue. ].
    ]
}

// === Basic Data Types ===

// --- IntNumber ---

#static IntegerGroup = basic'IntegerType $getGroup.

#class[dbg:int] IntNumber (Magnitude)
{
    #field(4).

    // class methods

    #method[def] $getClassName = "std'basic'IntNumber".

    #method[def] $typeCast : anObject = anObject $getInteger.

    // private methods

    #method $getInteger = $self.

    #method $getLong = #inline standard'46(basic'LongNumber, $self).

    #method $getReal = #inline standard'28 (basic'RealNumber, $self).

    #method $equal : aValue
    [
        #inline standard'6($self, aValue $getInteger).
    ]

    #method $less : aValue
    [
        #inline standard'7($self, aValue $getInteger).
    ]

    // public

    #method new : aValue
    [
        #if aValue ifNotNil
        [
            #inline standard'3 ($self, aValue $getInteger).
        ].
    ]

    #method clone = basic'IntNumber::self.

    #method + aValue
    [
        ^ #inline standard'8($self clone, aValue $getInteger).
    ]

    #method - aValue
    [
        ^ #inline standard'9($self clone, aValue $getInteger).
    ]

    #method * aValue
    [
        ^ #inline standard'10($self clone, aValue $getInteger).
    ]

    #method / aValue
    [
        ^ #inline standard'11($self clone, aValue $getInteger).
    ]

    #method anyMask : aValue
    [
        #inline standard'16($self, aValue $getInteger).
    ]

    #method allMask : aValue
    [
        #inline standard'17($self, aValue $getInteger).
    ]

    #method or : aValue
    [
       ^ #inline standard'13($self clone, aValue $getInteger).
    ]

    #method xor : aValue
    [
       ^ #inline standard'14($self clone, aValue $getInteger).
    ]

    #method and : aValue
    [
       ^ #inline standard'12($self clone, aValue $getInteger).
    ]

    #method shift : aValue
    [
       ^ #inline standard'15($self clone, aValue $getInteger).
    ]

    #method not = #inline standard'18($self clone, $self).

    // extension
    #annex IntegerGroup.
}

// --- LongNumber ---

#static LongGroup = basic'LongType $getGroup.

#class[dbg:long] LongNumber (Magnitude)
{
    #field(8).

    // class methods

    #method[def] $getClassName = "std'basic'LongNumber".

    #method[def] $typeCast : anObject = anObject $getLong.

    // private methods
    #method $getLong = $self.

    #method $getInteger
    [
        #if((self < -2147483646)or:(self > 2147483647))?
        [
            self fail.
        ]
        | [
            ^ basic'HiDWord::self.
        ].
    ]

    #method $getReal = #inline standard'47 (basic'RealNumber, $self).

    #method $equal : aValue
    [
        #inline standard'48($self, aValue $getLong).
    ]

    #method $less : aValue
    [
        #inline standard'49($self, aValue $getLong).
    ]

    #method new : aValue
    [
        #if aValue ifNotNil
        [
            #inline standard'31($self, aValue $getLong).
        ].
    ]

    #method clone = basic'LongNumber::self.

    #method + aValue
    [
        ^ #inline standard'50($self clone, aValue $getLong).
    ]

    #method - aValue
    [
        ^ #inline standard'51($self clone, aValue $getLong).
    ]

    #method * aValue
    [
        ^ #inline standard'52($self clone, aValue $getLong).
    ]

    #method / aValue
    [
        ^ #inline standard'53($self clone, aValue $getLong).
    ]

    #method anyMask : aValue
    [
        #inline standard'54($self, aValue $getLong).
    ]

    #method allMask : aValue
    [
        #inline standard'55($self, aValue $getLong).
    ]

    #method or : aValue
    [
       ^ #inline standard'58($self clone, aValue $getLong).
    ]

    #method xor : aValue
    [
       ^ #inline standard'59($self clone, aValue $getLong).
    ]

    #method and : aValue
    [
       ^ #inline standard'60($self clone, aValue $getLong).
    ]

    #method shift : aValue
    [
       ^ #inline standard'56($self clone, aValue $getInteger).
    ]

    #method not = #inline standard'57($self clone, $self).

    #method identify : aCaller
    [
        aCaller $ifLongNumber:$self.
    ]

    #annex LongGroup.
}

// --- RealNumber ---

#static RealGroup = basic'RealType $getGroup.

#class[dbg:real] RealNumber (Magnitude)
{
    #field(8).

    // class methods

    #method[def] $getClassName = "std'basic'RealNumber".

    #method[def] $typeCast : anObject = anObject $getReal.

    // private methods

    #method $getReal = $self.

    #method $equal : aValue
    [    
        #inline standard'29($self, aValue $getReal).
    ]

    #method $less : aValue
    [
        #inline standard'30($self, aValue $getReal).
    ]

    // public methods
    
    #method new : aValue
    [
        #if aValue ifNotNil
        [
            #inline standard'31($self, aValue $getReal).
        ].
    ]

    #method + aValue
    [
        ^ #inline standard'32($self clone, aValue $getReal).
    ]

    #method - aValue
    [
        ^ #inline standard'33($self clone, aValue $getReal).
    ]

    #method * aValue
    [
        ^ #inline standard'34($self clone, aValue $getReal).
    ]

    #method / aValue
    [
        ^ #inline standard'35($self clone, aValue $getReal).
    ]

    #method clone = RealNumber::self.

    // extension
    #annex RealGroup.
}

// --- Literal ---

#static LiteralGroup = basic'LiteralType $getGroup.

#class[dbg:literal] Literal (Magnitude)
{
    #field.

    // class
    
    #method[def] $getClassName = "std'basic'Literal".

    #method[def] $typeCast : anObject = anObject $getLiteral.

    // private

    #method $getLiteral = $self.

    #method $equal : anObject
    [
        #inline standard'1($self, anObject $getLiteral).
    ]

    #method $less : anObject
    [
        #inline standard'2($self, anObject $getLiteral).
    ]

    // public

    #method new [ self fail. ]

    #method Length = #inline standard'3(basic'IntNumber, $self).

    #method prop'$getIndexer = basic'LiteralIndexer::self.

    #method clone
    [
        #var aCopy := basic'LiteralType $create:(self Length).

        #inline standard'4(aCopy, $self).

        ^ aCopy.
    ]

    #method + aValue
    [
        #var aLiteral := aValue $getLiteral.

        #var aNewLiteral := basic'LiteralType create:{ Length = self Length + aLiteral Length. }.

        #inline standard'4(aNewLiteral, $self).
        #inline standard'5(aNewLiteral, aLiteral).

        ^ aNewLiteral.
    ]

    // extension
    #annex LiteralGroup.
}

// --- CharValue ---

#class[dbg:int] CharValue (Magnitude)
{
    #field(2).

    // class
    
    #method[def] $getClassName = "std'basic'CharValue".

    #method[def] $typeCast : anObject = anObject $getChar.

    // private

    #method $getChar = $self.

    #method $equal : aValue
    [
        #inline standard'6($self, aValue $getChar).
    ]

    #method $less : aValue
    [
        #inline standard'7($self, aValue $getChar | $getInteger).
    ]

    #method $getLiteral
    [
        #var aLiteral := basic'LiteralType create:{ Length = 1. }.

        #inline standard'23(aLiteral, $self).

        ^ aLiteral.
    ]

    #method $getInteger = #inline standard'3(basic'IntNumber, $self).

    // public

    #method new : aChar
    [
        #if aChar ifNotNil
        [
            #inline standard'3 ($self, basic'LoWord::(aChar $getInteger)).
        ].
    ]

    #method clone = CharValue::self.

    // extension
    #annex LiteralGroup.
}

// --- Array ---

#static ArrayGroup = basic'ArrayType $getGroup.

#class[dbg:array] Array
{
    #field.

    // class
    
    #method[def] $getClassName = "std'basic'Array".

    #method[def] $typeCast : anObject = anObject $getArray.

    // private

    #method $getArray = $self.

    #method prop'$getIndexer = basic'ArrayIndexer::self.

    // public

    #method Length = #inline standard'43(basic'IntNumber, $self).

    #method new [ self fail. ]

    // extension
    #annex ArrayGroup.
}

// --- True ---

#symbol True =
{
    $getBool = $self.

    ? []

    if : aBool
    [
        #if aBool!
            [ $self fail. ].
    ]

    == aValue
    [
        #if(aValue $getBool) ? [ ^ basic'True. ] | [ ^ basic'False. ].
    ]

    != aValue
    [
        #if(aValue $getBool) ? [ ^ basic'False. ] | [ ^ basic'true. ].
    ]

    and : aValue 
    [ 
        #var aBool := aValue $getBool.

        #if(aBool)? [ ^ basic'True. ] 
            | ! [ ^ basic'False. ]
            | [ $self fail. ].
    ]

    or : aValue 
    [ 
        #var aBool := aValue $getBool.

        #if(aBool)? [ ^ basic'True. ] 
            | ! [ ^ basic'True. ]
            | [ $self fail. ].
    ]

    xor : aValue
    [
        #var aBool := aValue $getBool.

        #if(aBool)? [ ^ basic'False. ] 
            | ! [ ^ basic'True. ]
            | [ $self fail. ].
    ]

    not = basic'False.
}.

// --- False ---

#symbol False =
{
    $getBool = $self.

    ! []

    if : aBool
    [
        #if aBool?
            [ $self fail. ].
    ]

    == aValue
    [
        #if(aValue $getBool)! [ ^ basic'True. ] 
            | [ ^ basic'False. ].
    ]

    != aValue
    [
        #if(aValue $getBool)! [ ^ basic'False. ] 
            | [ ^ basic'True. ].
    ]

    and : aValue 
    [ 
        #var aBool := aValue $getBool.

        #if(aBool)? [ ^ basic'False. ] 
            | ! [ ^ basic'False. ]
            | [ $self fail. ].
    ]

    or : aValue 
    [ 
        #var aBool := aValue $getBool.

        #if(aBool)? [ ^ basic'True. ] 
            | ! [ ^ basic'False. ]
            | [ $self fail. ].
    ]

    xor : aValue
    [
        #var aBool := aValue $getBool.

        #if(aBool)? [ ^ basic'True. ] 
            | ! [ ^ basic'False. ]
            | [ $self fail. ].
    ]
    
    not = basic'True.
}.


// --- VariantValue ---

#class VariantValue
{
    #field theValue.

    #method[def] $getClassName = "std'basic'VariantValue".

    #method new : aValue
    [
        theValue := aValue.
    ]

    #method $getLiteral = theValue $getLiteral | $toLiteral $getLiteral.

    #method $getInteger = theValue $getInteger | $toInteger $getInteger.

    #method $getReal = theValue $getReal | $toReal $getReal.

    #method $getLong = theValue $getLong | $toLong.

    #method $getBool = theValue $getBool | $toBool.

    #annex theValue.
}

// --- Nil ---

#symbol NilValue =
{
    ifNotNil [ $self fail. ]

    == anObject
    [
        #if anObject ifNotNil [ ^ basic'false. ] | [ ^ basic'true. ].        
    ]

    != anObject
    [
        #if anObject ifNotNil [ ^ basic'true. ] | [ ^ basic'false. ].
    ]
}.

// === Basic Variables ===

// --- Integer ---

#class Integer (IntNumber)
{
    // class methods

    #method[def] $getClassName = "std'basic'Integer".

    // public

    #method << aValue
    [
        #inline standard'3($self, aValue $getInteger).
    ]

    #method += aValue
    [
        #inline standard'8($self, aValue $getInteger).
    ]

    #method -= aValue
    [
        #inline standard'9($self, aValue $getInteger).
    ]

    #method *= aValue
    [
        #inline standard'10($self, aValue $getInteger).
    ]

    #method /= aValue
    [
        #inline standard'11($self, aValue $getInteger).
    ]

    #method next
    [
        self += 1.
    ]

    #method previous
    [
        self -= 1.
    ]
}

// --- LongInteger ---

#class LongInteger (LongNumber)
{
    // class methods

    #method[def] $getClassName = "std'basic'LongInteger".

    // public

    #method << aValue
    [
        #inline standard'31($self, aValue $getLong).
    ]

    #method += aValue
    [
        #inline standard'50($self, aValue $getLong).
    ]

    #method -= aValue
    [
        #inline standard'51($self, aValue $getLong).
    ]

    #method *= aValue
    [
        #inline standard'52($self, aValue $getLong).
    ]

    #method /= aValue
    [
        #inline standard'53($self, aValue $getLong).
    ]

    #method next
    [
        self += 1.
    ]

    #method previous
    [
        self -= 1.
    ]
}

// --- Real ---

#class Real (RealNumber)
{
    // class methods

    #method[def] $getClassName = "std'basic'Real".

    // public

    #method << aValue
    [
        #inline standard'31($self, aValue $getReal).
    ]

    #method += aValue
    [
        #inline standard'32($self, aValue $getReal).
    ]

    #method -= aValue
    [
        #inline standard'33($self, aValue $getReal).
    ]

    #method *= aValue
    [
        #inline standard'34($self, aValue $getReal).
    ]

    #method /= aValue
    [
        #inline standard'35($self, aValue $getReal).
    ] 
}

// --- String ---

#static StringGroup = basic'StringType $getGroup.

#class String (Magnitude)
{
    #field theLiteral.

    #role Empty
    {
        #method $getLiteral = "".

        #method $equal : aValue [ basic'True if:(aValue Length==0). ]

        #method $less : aValue [ basic'True if:(aValue Length > 0). ]

        #method Length = 0.

        #method clone = basic'String.

        #method + aValue [ ^ aValue $getLiteral. ]

        #method << aValue
        [
            #shift.

            self << aValue.
        ]

        #method += aValue [ self << aValue. ]

        #method new : aValue
        [
            #shift.

            self new:aValue.
        ]
    }

    // class methods

    #method[def] $getClassName = "std'basic'String".

    // private methods

    #method $getString = $self.

    #method $getLiteral = theLiteral $invoke.

    #method $equal : aValue
    [
        theLiteral $invoke:aValue.
    ]

    #method $less : aValue
    [
        theLiteral $invoke:aValue.
    ]

    // public methods

    #method Length = theLiteral Length.

    #method new : aValue
    [
        #if aValue ifNotNil
        [
            theLiteral := aValue $getLiteral.            
        ]
        | [ #shift Empty. ].
    ]

    #method clone = String << theLiteral.

    #method + aValue
    [
        ^ theLiteral + aValue.
    ]

    #method << aValue
    [
        #var aLiteral := aValue $getLiteral.

        theLiteral := basic'LiteralType create:aLiteral.

        #inline standard'4(theLiteral, aLiteral).
    ]

    #method += aValue
    [
        theLiteral := theLiteral + aValue.
    ]    

    #method clear
    [
        #shift Empty.
    ]

    #annex StringGroup.
}

// --- CharValue ---

#class Char (CharValue)
{
    #method << aValue
    [
        #inline standard'3($self, basic'LoWord::(aValue $getInteger)).
    ]

    #method += aValue
    [
        self << (self $getInteger + aValue $getInteger).
    ]

    #method -= aValue
    [
        self << (self $getInteger - aValue $getInteger).
    ]

    #method *= aValue
    [
        self << (self $getInteger * aValue $getInteger).
    ]

    #method /= aValue
    [
        self << (self $getInteger / aValue $getInteger).
    ]
}

// --- Boolean ---

#static BooleanGroup = basic'BooleanType $getGroup.

#class Boolean
{
    #role TrueValue
    {
        #method ? []

        #method not = basic'False.

        #method $getBool = basic'True.

        #method $setTrue []
    }

    #role FalseValue
    {
        #method ! []

        #method not = basic'True.

        #method $getBool = basic'False.

        #method $setFalse []
    }

    // class
    
    #method[def] $getClassName = "std'basic'Boolean".

    #method[def] $typeCast : anObject = anObject $getBool.

    // private

    #method $setTrue
    [
        #shift TrueValue.
    ]

    #method $setFalse
    [
        #shift FalseValue.
    ]

    // public

    #method new : aValue
    [
        #if aValue ifNotNil
        [
            #if(aValue $getBool)
                ? [ self $setTrue. ] 
                | ! [ self $setFalse. ].
        ].
    ]    

    #method == aBool
    [
        ^ (self $getBool == aBool).
    ]

    #method != aBool
    [
        ^ (self $getBool != aBool).
    ]

    #method << aValue
    [
        #if(aValue $getBool)
            ? [ self $setTrue. ] 
            | ! [ self $setFalse. ]
            | [ self fail. ].
    ]

    #method and : aBool = self $getBool and:aBool.

    #method or : aBool = self $getBool or:aBool.

    #method xor : aBool = self $getBool xor:aBool.    

    #method if : aBool [ self $getBool if:aBool. ]
    
    #annex BooleanGroup.
}

// === Basic Adapters ===

// --- LiteralIndexer ---

#class LiteralIndexer (Indexer)
{
    #field theChar.

    #method new : anOwner
    [
        theArray := anOwner $getLiteral.
        theIndex := basic'Integer << 0.

        theChar := #inline standard'19(basic'CharValue, theArray, theIndex).
    ]

    #method prop'$goto : anIndex
    [
        super $invoke:anIndex.

        theChar := #inline standard'19(basic'CharValue, theArray, theIndex).
    ]

    #method prop'$next
    [
        super $invoke.

        theChar := #inline standard'19(basic'CharValue, theArray, theIndex).
    ]

    #method prop'$previous
    [
        super $invoke.

        theChar := #inline standard'19(basic'CharValue, theArray, theIndex).
    ]

    #method prop'$getContent = theChar.

    #annex theChar.
}

// --- StringIndexer ---

#class StringIndexer (LiteralIndexer)
{
    #field theString.

    #role EOF
    {
        #method prop'$getContent [ self fail. ]

        #method prop'$seek [ self fail. ]

        #method prop'$goto : anIndex
        [
            #if(theArray Length > anIndex)?
            [
                #shift.
            ].

            super $invoke:anIndex.
        ]

        #method prop'$insert : aLiteral
        [
            theString += aLiteral.

            theArray := theString $getLiteral.

            theChar := #inline standard'19(basic'CharValue, theArray, theIndex).
        ]

        #method prop'$delete
        [
        ]
    }

    #method new : aString
    [
        theString := aString $getString.

        #if(theString Length > 0)?
        [
            super new:(aString $getLiteral).
        ]
        | [
            theIndex := basic'Integer << 0.
            theArray := aString $getLiteral.

            self $setEOF.
        ].
    ]

    #method prop'$goto : anIndex
    [
        #if(theArray Length == anIndex)?
        [
            self $setEOF.
        ]
        | [ super $invoke:anIndex. ].
    ]

    #method $setEOF
    [
        theChar := nil.
        theIndex << theArray Length.

        #shift EOF.
    ]

    #method prop'$seek : aValue
    [
        #var aRetVal := IntNumber.

        #inline standard'20(aRetVal, theArray, theIndex, aValue $getLiteral).
        
        #if(aRetVal!=-1)?
        [ 
            theIndex << aRetVal. 
            theChar := #inline standard'19(basic'CharValue, theArray, theIndex).
        ]
        | [ self fail. ].
    ]

    #method prop'$insert : aValue
    [
        #var aSubS := aValue $getLiteral.
        #var aNewS := basic'LiteralType create:{ Length = aSubS Length + theArray Length. }.

        #inline standard'21(aNewS, theArray, theIndex, aSubS).
        theIndex += aSubS Length.

        theArray := aNewS.
        theString new:theArray.
    ]

    #method prop'$delete : aNumber
    [
        #var aLength := aNumber $getInteger.

        #var aNewS := basic'LiteralType create:{ Length = theArray Length - aLength. }.

        #inline standard'22(aNewS, theArray, theIndex, aLength).

        theArray := aNewS.
        theString new:theArray.
    ]
}

// --- ArrayIndexer ---

#class ArrayIndexer (Indexer)
{
    #field theItem.

    #method new : anArray
    [
        theArray := anArray $getArray.
        theIndex := basic'Integer << 0.

        theItem := #inline standard'41(theArray, theIndex).
    ]

    #method prop'$goto : anIndex
    [
        super $invoke:anIndex.

        theItem := #inline standard'41(theArray, theIndex).
    ]

    #method prop'$next
    [
        super $invoke.

        theItem := #inline standard'41(theArray, theIndex).
    ]

    #method prop'$previous
    [
        super $invoke.

        theItem := #inline standard'41(theArray, theIndex).
    ]

    #method prop'$getContent = theItem.

    #method prop'$setContent : anObject
    [
        self $set:anObject.
    ]

    #method $set : anObject
    [
        #external standard'42(theArray, theIndex, anObject).

        theItem := anObject.
    ]

    #annex theItem.
}

// --- StringWriter ---

#class StringWriter
{
    #field theString.

    #method $getLiteral = theString $getLiteral.

    #method new : aString
    [
        #if aString ifNotNil
            [ theString := aString. ]
            | [ theString := basic'String. ].
    ]

    #method $writeAsLiteral : aValue
    [
        theString += aValue.
    ]
}

// === Basic Properties ===

// --- HiWord ---

#symbol HiWord :anInteger
    = #inline standard'24(basic'IntNumber, anInteger $getInteger).

// --- LoWord ---

#symbol LoWord : anInteger
    = #inline standard'25(basic'IntNumber, anInteger $getInteger).

// --- HiDWord ---

#symbol HiDWord :anInteger
    = #inline standard'61(basic'IntNumber, anInteger $getLong, 4).

// --- LoDWord ---

#symbol LoDWord : anInteger
    = #inline standard'3(basic'IntNumber, anInteger $getLong).

// --- SubString ---

#symbol SubString : aRange
    = #inline standard'64(basic'LiteralType create:{ Length = aRange till - aRange from + 1. }, aRange for $getLiteral, aRange from $getInteger).

// === Basic Extensions ===

// --- BasicExtension ---

#symbol BasicExtension =
{
    prop'$input : aReader
    [
        #var aValue := basic'LiteralType $invoke:aReader.

        self << basic'VariantValue::aValue.
    ]

    prop'$printOn : aWriter
    [
        (self $getLiteral | $toLiteral) $invoke:aWriter.
    ]
}.

// --- IntegerExtension ---

#symbol IntegerExtension =
{
    $getInteger = self $toInteger $getInteger.

    $getLong = self $toInteger $getLong.

    $toLiteral = #inline standard'27(basic'LiteralType create:{ Length = 11. }, self $getInteger).

    $getReal = self $toInteger $getReal.

    prop'$readFrom : aReader
    [
        #var aValue := basic'Integer.

        aReader $readAsInt32:aValue.

        self << aValue.
    ]

    prop'$writeTo : aWriter
    [
        aWriter $writeAsInt32:(self $getInteger).
    ]    

    $toBool
    [
        #if (-1 == self)
            ? [ ^ basic'True. ]
            | ! [ ^ basic'False. ]
            | [ self fail. ].
    ]
}.

// --- LongExtension ---

#class LongExtension
{
    #method $getLong = self $toLong $getLong.

    #method $toBool
    [
        #if (-1 == self)
            ? [ ^ basic'True. ]
            | ! [ ^ basic'False. ]
            | [ self fail. ].
    ]

    #method $toLiteral = #inline standard'62(basic'LiteralType create:{ Length = 23. }, self $getLong).
}

// --- RealExtension ---

#symbol RealExtension =
{
    $getReal = self $toReal $getReal.

    $toLiteral = #inline standard'36(basic'LiteralType create:{ Length = 24. }, self $getReal).

    prop'$readFrom : aReader
    [
        #var aValue := basic'RealNumber.

        aReader $readAsReal64:aValue.

        self << aValue.
    ]

    prop'$writeTo : aWriter
    [
        aWriter $writeAsReal64:self.
    ]
}.

// --- LiteralExtension ---

#symbol LiteralExtension =
{
    $getLiteral = self $toLiteral $getLiteral.

    $toInteger = #inline standard'26(basic'IntNumber, self $getLiteral).

    $toLong = #inline standard'63(basic'LongNumber, self $getLiteral).

    $toReal = #inline standard'37(basic'RealNumber, self $getLiteral).

    $toBool
    [
        #if("true" == self)
            ? [ ^ basic'True. ].

        #if("false" == self)
            ? [ ^ basic'False. ].

        self fail.
    ]

    prop'$getEnumerator = prop'Indexer::self.

    prop'$printOn : aWriter
    [
        aWriter $writeAsLiteral:(self $getLiteral).
    ]

    prop'$writeTo : aWriter
    [
        #var aLiteral := self $getLiteral.

        aWriter 
            $writeAsInt32:(aLiteral Length)
            $writeAsLiteral:aLiteral.
    ]

    Length = self $getLiteral Length.
}.

    
// --- StringExtension ---

#symbol StringExtension =
{
    prop'$getIndexer = basic'StringIndexer::self.

    prop'$input : aReader
    [
        #var aBuffer := LiteralType create:{ Length = 32. }.

        #loop self ifNotNil
        [
            aReader $readAsLiteral:aBuffer.

            self += aBuffer.
    
            #if (aBuffer Length < 32)?
                [ ^ self. ].
        ].
    ]

    prop'$readFrom : aReader
    [
        #var aLength := basic'Integer.
        aReader $readAsInt32:aLength.

        #var aLiteral := basic'LiteralType create:{ Length = aLength. }.
        aReader $readAsLiteral:aLiteral.

        self new:aLiteral.
    ]

    prop'$getWriter = basic'StringWriter::(self $getString).

    += aLiteral
    [
        self << (self $getLiteral + aLiteral).
    ]
}.

// --- ArrayExtension ---

#class ArrayExtension
{
    #method prop'$getEnumerator = prop'Indexer::self.
}

// --- BooleanExtension ---

#symbol BooleanExtension =
{
    $toInteger
    [
        #if(self $getBool)
            ? [ ^ -1. ] | ! [ ^ 0. ] | [ self fail. ].
    ]

    $toLong
    [
        #if(self $getBool)
            ? [ ^ basic'LongNumber::-1. ] | ! [ ^ basic'LongNumber::0. ] | [ self fail. ].
    ]

    $toLiteral
    [
        #if(self $getBool)
            ? [ ^ "true". ] | ! [ ^ "false". ] | [ self fail. ].
    ]

    prop'$printOn : aWriter
    [
        self $toLiteral $invoke:aWriter.
    ]

    prop'$writeTo : aWriter
    [
        self $toInteger $invoke:aWriter.
    ]

    prop'$readFrom : aReader
    [
        #var anInteger := basic'Integer.
        
        aReader $invoke:anInteger.

        #if(anInteger!=0)?
        [
            self << basic'True.
        ]
        | [
            self << basic'False.
        ].
    ]
}.

// === Basic Type Symbols ===

// --- IntegerType ---

#symbol IntegerType =
{
    $getGroup = #group(basic'BasicExtension, basic'IntegerExtension).

    prop'$input : aReader
    [
        ^ basic'IntNumber::(basic'Integer $invoke:aReader).
    ]

    of : anObject = anObject $getInteger.
}.

#symbol LongType =
{
    $getGroup = #group(basic'BasicExtension, basic'LongExtension).

    prop'$input : aReader
    [
        ^ basic'LongNumber::(basic'LongInteger $invoke:aReader).
    ]

    of : anObject = anObject $getLong.
}.

// --- RealType ---

#symbol RealType =
{
    $getGroup = #group(basic'BasicExtension, basic'RealExtension).

    of : anObject = anObject $getReal.

    prop'$input : aReader
    [
        ^ basic'RealNumber::(basic'Real $invoke:aReader).
    ]
}.

// --- LiteralType ---

#symbol LiteralType =
{
    // private

    $getGroup = #group(basic'LiteralExtension, prop'IndexerExtension).

    prop'$input : aReader
    [
        ^ basic'String $invoke:aReader $getLiteral.
    ]

    // public methods

    create : aParameter
    [
        ^ #inline standard'38 (#type basic'Literal, aParameter Length).
    ]

    of : anObject = anObject $getLiteral.
}.

// --- StringType ---

#symbol StringType = 
{
    $getGroup = #group(basic'LiteralExtension, prop'IndexerExtension, basic'StringExtension).
}.

// --- ArrayType ---

#symbol ArrayType =
{
    // private

    $getGroup = #group(basic'ArrayExtension, prop'IndexerExtension).

    // public methods

    create : aParameter
    [
        #var anArray := #inline standard'39 (#type basic'Array, nil, aParameter Length).

        prop'Enumerator::anArray run: anItem =>
        [
            #if (prop'Content::anItem) set:(aParameter create:anItem)
            | [
                ^ ctrl'Break.
            ].
        ].

        ^ anArray.
    ]

    of : anObject = anObject $getArray.
}.

// --- BooleanType ---


// --- BooleanType ---

#symbol BooleanType =
{
    of : aParam = aParam $getBool.

    $getGroup = basic'BooleanExtension.

    prop'$input : aReader
    [
        ^ basic'Boolean $invoke:aReader.
    ]
}.


// --- CharType ---

#symbol CharType =
{
    of : anObject = anObject $getChar.
}.
