// --- namespace shortcuts ---
#define basic'*   = std'basic'*.
#define prop'*    = std'properties'*.

#define api'*    = win32'api'*.
#define styles'* = win32'api'styles'*.

// === Common Factories ===

// --- HandlerType ---

#symbol HandlerType =
{
    create : aStyles
    [
        ^ aStyles $createHandler:aStyles.
    ]
}.

// --- ClassType ---

#symbol ClassType =
{
    create : aStyles
    [
        ^ aStyles $createClass.
    ]
}.

// === Common symbols ===

// --- SDIWindowClass ---

#static SDIWindowClass
    = ClassType create:styles'SDIWindowClass.

// --- PanelClass ---

#static PanelClass
    = ClassType create:styles'PanelClass.

// --- PaintboxClass ---

#static PaintboxClass
    = ClassType create:styles'PaintboxClass.

// --- NonvisibleWindowClass ---

#static NonvisibleWindowClass
    = ClassType create:styles'NonvisibleWindowClass.
