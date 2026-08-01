// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define utils'* = ext'utilities'*.

// --- RandomGenerator ---

#class $RandomGenerator
{
    #field(8).

    #method new
    [
        #external extended'1($self).
    ]
}

#static RandomGenerator = $RandomGenerator.

// --- RandomValue ---

#symbol RandomValue : aMaxValue
    = #external extended'2(basic'IntNumber, RandomGenerator, basic'IntegerType of:aMaxValue).

// --- Date ---

#class Date
{
    #field(8).

    // class methods

    #method[def] $getClassName = "ext'utilities'Date".

    // private methods

    #method $getDate = $self.
}

// --- Timespan ---

#class Timespan
{
    #field(8).
}

// --- DateType ----

#symbol DateType =
{
    of : anObject = anObject $getDate.

    $getCurrent 
    [
        ^ #external extended'3(Date).
    ]
}.

// --- Now ---

#symbol Now
    = DateType $getCurrent.
