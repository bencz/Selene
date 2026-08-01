#define basic'* = std'basic'*.
#define math'*  = std'basic'math'*.
#define ctrl'*  = std'patterns'*.

// x ^ y
#symbol Power : aParameter = ctrl'Control run: =>
[
    #var aResult := basic'Real << 1.
    #var aX := aParameter X.

    ctrl'DecrimentLoop::(aParameter Y) run: =>
    [
        aResult << (aResult * aX).
    ].

    ^ aResult.
].

#symbol Program =>
[
    #var MAX := 10000.
    #var i := basic'Integer << 0.
    #var pi := basic'real << 0.

    #loop(i<MAX)?
    [
        pi := pi + (Power::{X = -1. Y = i. } / ((2*i)+1))*4.
        i += 1.
    ].
    'program'output << pi.
].
