// --- namespace shortcuts ---
#define basic'* = std'basic'*.

// --- Abs ---

#symbol Abs : aNumber
    = #inline standard'65(basic'RealNumber, aNumber $getReal).

// --- Rounded ---

#symbol Rounded : aNumber
    = #inline standard'66(basic'IntNumber, aNumber $getReal).

// --- Int ---

#symbol Int : aNumber
    = #inline standard'67(basic'RealNumber, aNumber $getReal).

// --- ArcTan ---

#symbol ArcTan : aNumber
    = #inline standard'68(basic'RealNumber, aNumber $getReal).

// --- Cos ---

#symbol Cos : aNumber
    = #inline standard'69(basic'RealNumber, aNumber $getReal).

// --- Exp ---

#symbol Exp : aNumber
    = #inline standard'70(basic'RealNumber, aNumber $getReal).

// --- Frac ---

#symbol Frac : aNumber
    = (aNumber - Int::aNumber).

// --- Ln ---

#symbol Ln : aNumber
    = #inline standard'71(basic'RealNumber, aNumber $getReal).

// --- Sin ---

#symbol Sin : aNumber
    = #inline standard'72(basic'RealNumber, aNumber $getReal).

// --- Sqrt ---

#symbol Sqrt : aNumber
    = #inline standard'73(basic'RealNumber, aNumber $getReal).

// --- Pi ---

#symbol Pi
    = #inline standard'74(basic'RealNumber).
