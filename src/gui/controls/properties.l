// --- namespace shortcuts ---
#define basic'* = std'basic'*.
#define ctrl'*  = std'patterns'*.
#define prop'*  = std'properties'*.
#define list'*  = std'collections'*.

#define controls'* = gui'controls'*.
#define graph'*    = gui'graphics'*.

// --- compiler hints ---
#define[const] LiteralExtension = basic'LiteralExtension.
#define[const] StringExtension  = basic'StringExtension.
#define[const] IndexerExtension = prop'IndexerExtension.
#define[const] BooleanExtension = basic'BooleanExtension.
#define[const] ListExtension    = list'ListExtension.
#define[const] ArrayExtension   = basic'ArrayExtension.
#define[const] IntegerExtension = basic'IntegerExtension.

// === Common properties ===

// --- Caption ---

#symbol Caption : aControl = #annex(#group(LiteralExtension, StringExtension, IndexerExtension))
{
    << aLiteral
    [
        aControl $setCaption:aLiteral.
    ]

    basic'$toLiteral = aControl $getCaption.
}.

// --- Range ---

#symbol Range : aControl =
{
    << aRange
    [
        aControl $setRange:aRange.
    ]

    prop'$getRange = aControl $getRange.
}.

// --- Size ---

#symbol Size : aControl =
{
    << aSize
    [
        aControl $setSize:(graph'SizeType of:aSize).
    ]

    graph'$getSize = aControl $getSize.
}.

// --- Location ---

#symbol Location : aControl =
{
    << aPoint
    [
        aControl $setLocation:(graph'PointType of:aPoint).
    ]

    graph'$getCoord = aControl $getLocation.
}.

// --- Checked ---

#symbol Checked : aControl = #annex(BooleanExtension)
{
    << aBool
    [
        #if ctrl'Control 
            if:(basic'True == aBool) [ aControl $check. ]
            | if:(basic'True == aBool) [ aControl $uncheck. ]    
            | [ self fail. ].
    ]

    basic'$getBool = aControl $ifChecked back:basic'True | back:basic'False.
}.

// --- Enabled ---

#symbol Enabled : aControl = #annex(BooleanExtension)
{
    << aBool
    [
        #if basic'True 
            if:(basic'True == aBool) [ aControl $enable. ]
            | if:(basic'False == aBool) [ aControl $disable. ]    
            | [ self fail. ].
    ]

    basic'$getBool = aControl $ifEnabled back:basic'True | back:basic'False.
}.

// --- Visible ---

#symbol Visible : aControl = #annex(BooleanExtension)
{
    << aBool
    [
        #if basic'True
            if:(basic'True == aBool) [ aControl open. ]
            | if:(basic'False == aBool) [ aControl close. ]    
            | [ self fail. ].
    ]

    basic'$getBool = aControl $ifVisible back:basic'True | back:basic'False.
}.

// --- Items ---

#symbol Items : aControl = #annex((IndexerExtension, ArrayExtension))
{
    Count = aControl $getItemCount.

    $getControl = aControl.

    prop'$getRange = { from = 0. till = aControl $getItemCount - 1. for = aControl. }.

    += aValue
    [
        aControl $addItem:aValue.

        aControl $getObjects += nil.
    ]

    prop'$getIndexer = controls'Indexer::self.
}.

// --- Item --

#class Item
{
    #method $getTag = self $getObject.

    #method basic'$toLiteral = basic'LiteralType of:(self $getCaption).

    #annex (LiteralExtension, IndexerExtension).
}

// --- Tag ---

#class Tag
{
    #field theControl.

    #method new : aControl
    [   
        theControl := aControl.
    ]

    #method prop'$getContent = theControl $getTag.

    #method prop'$setContent : anObject
    [
        theControl $setTag:anObject.
    ]

    #annex (theControl $getTag).
}

// --- DialogResult ---

#symbol DialogResult : aWindow =
{ 
    << aResult [ aWindow $exitWithResult:aResult. ] 
}.

// --- SelectedIndex ---

#symbol SelectedIndex : aControl = #annex(IntegerExtension)
{
    basic'$toInteger = basic'IntNumber::(aControl $getSelectedIndex).

    << aValue
    [
        aControl $setSelectedIndex:(basic'IntegerType of:aValue).
    ]
}.

// --- SelectedItem ---

#symbol SelectedItem : aControl
    = aControl $getItemAt:(aControl $getSelectedIndex).

// --- Controls ---

#class Controls
{
    #field theOwner.

    #method new : anOwner
    [
        theOwner := anOwner.
    ]

    #method prop'$getEnumerator = theOwner $getControls $invoke.

    #annex (ListExtension, IndexerExtension).
}
