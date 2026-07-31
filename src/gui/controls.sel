// --- namespace shortcuts ---
#define basic'*   = std'basic'*.
#define prop'*    = std'properties'*.
#define ctrl'*    = std'patterns'*.
#define events'*  = sys'events'*.
#define list'*    = std'collections'*.

#define gui'prop'* = gui'controls'properties'*.
#define graph'*    = gui'graphics'*.

// --- compiler hints ---
#define[const] IntegerExtension = basic'IntegerExtension.
#define[const] LiteralExtension = basic'LiteralExtension.
#define[const] StringExtension  = basic'StringExtension.
#define[const] ArrayExtension   = basic'ArrayExtension.
#define[const] ListExtension    = list'ListExtension.
#define[const] IndexerExtension = prop'IndexerExtension.

// === Base control ===

#class Control
{
    #field theHandle.
    #field theParent.
    #field theEvents.

    #method $ifEmbeddedControl []

    #method $getControl = $self.

    #method new : aParent
    [
        #if aParent ifNotNil
        [
            theParent := aParent.

            #if self $ifEmbeddedControl
            [
                theParent $addControl:self.
            ].
        ].

        theHandle := self $createHandle.
        theEvents := events'Controller.
    ]

    #method events'$getController = theEvents.

    #annex(theHandle).
}

// --- Container ---

#class Container (Control)
{
    #field theControls.

    #method $getControls = theControls.

    #method $addControl : aControl
    [
        theControls += aControl.
    ]

    #method new : aParent
    [
        super new:aParent.

        theControls := list'List.
    ]

}

// === Controls ===

// --- StaticFrame ---

#class StaticFrame (Control)
{
    #method $createHandle = 'gui'handlerType create:#group('gui'styles'StaticFrame, 'gui'styles'Parent::theParent).
}
    
// --- StaticLabel ---

#class StaticLabel (Control)
{
    #method $createHandle = 'gui'handlerType create:#group('gui'styles'StaticLabel, 'gui'styles'Parent::theParent).
}

// --- GroupBox ---

#class GroupBox (Control)
{
    #method $createHandle = 'gui'handlerType create:#group('gui'styles'GroupBox3D, 'gui'styles'Parent::theParent).
}

// --- InnerStaticFrame ---

#class InnerStaticFrame (StaticFrame)
{
    #method $ifEmbeddedControl [ self fail. ]
}

// --- InnerStaticLabel ---

#class InnerStaticLabel (StaticLabel)
{
    #method $ifEmbeddedControl [ self fail. ]
}

// --- InnerGroupBox ---

#class InnerGroupBox (GroupBox)
{
    #method $ifEmbeddedControl [ self fail. ]
}

// --- Edit ---

#static EditGroup = #group(LiteralExtension, StringExtension, IndexerExtension).

#class Edit (Control)
{
    #method $createHandle = 'gui'handlerType create:#group('gui'styles'Edit3D, 'gui'styles'Parent::theParent).

    #method << aValue
    [
        gui'prop'Caption::self << aValue.
    ]

    #method basic'$toLiteral = basic'LiteralType of:(gui'prop'Caption::self).

    #annex EditGroup.
}

// --- UpDown ---

#class UpDown (Control)
{
    #method $createHandle = 'gui'handlerType create:#group('gui'styles'UpDown3D, 'gui'styles'Parent::theParent).

    //#method prop'$getRange = gui'prop'Range::self.
}

// --- Button ---

#class Button (Control)
{
    #method $createHandle = 'gui'handlerType create:#group('gui'styles'Button3D, 'gui'styles'Parent::theParent).
}

// --- RadioButton ---

#class RadioButton (Button)
{
    #method $createHandle = 'gui'handlerType create:#group('gui'styles'RadioButton3D, 'gui'styles'Parent::theParent).
}

// --- InnerRadioButton ---

#class InnerRadioButton (RadioButton)
{
    #method $ifEmbeddedControl [ self fail. ]

    #method new : aParent
    [
        super new:aParent.

        theParent $addRadioButton:self.
    ]
}

// --- Combobox ---

#class Combobox (Control)
{
    #field theObjects.

    #method $createHandle = 'gui'handlerType create:#group('gui'styles'Combobox3D, 'gui'styles'Parent::theParent).

    #method $getItemAt : anIndex = gui'prop'Item
    {
        $getCaption = theHandle $getItemAt:anIndex.

        $getObject = theObjects @ anIndex.
    }.

    #method $getObjects = theObjects.

    #method $insertItem : aLocation
    [
        theHandle $insertItem:aLocation.

        (prop'Indexer::theObjects << aLocation) insert:(aLocation Value).
    ]

    #method $deleteItem : aLocation
    [
        theHandle $deleteItem:aLocation.

        (prop'Indexer::theObjects << aLocation) delete:aLocation.
    ]

    #method new : aParent
    [
        super new:aParent.

        theObjects := list'List.
    ]
}

// --- Listbox ---

#class Listbox (Control)
{
    #field theObjects.

    #method $createHandle = 'gui'handlerType create:#group('gui'styles'Listbox3D, 'gui'styles'Parent::theParent).

    #method $getObjects = theObjects.

    #method $getItemAt : anIndex = gui'prop'Item
    {
        $getCaption = theHandle $getItemAt:anIndex.

        $getObject = theObjects @ anIndex.
    }.

    #method $insertItem : aLocation
    [
        theHandle $insertItem:aLocation.

        (prop'Indexer::theObjects << aLocation) insert:(aLocation Value).
    ]

    #method $deleteItem : aLocation
    [
        theHandle $deleteItem:aLocation.

        (prop'Indexer::theObjects << aLocation) delete:aLocation.
    ]

    #method new : aParent
    [
        super new:aParent.

        theObjects := list'List.
    ]
}

// --- StatusBar ---

#class StatusBar (Control)
{
    #method $createHandle = 'gui'handlerType create:#group('gui'styles'StatusBar3D, 'gui'styles'Parent::theParent).

    #method $getItemAt : anIndex = gui'prop'Item
    {
        $getCaption = theHandle $getItemAt:anIndex.

        << aValue
        [
            theHandle $setItem:{ Position = anIndex. Value = aValue. }.
        ]
    }.
}

// --- Panel ---

#class Panel (Container)
{
    #field theFrame.

    #method $createHandle 
    [
        #var anOwner := 'gui'styles'Owner::self.

        ^ 'gui'handlerType create:#group('gui'styles'Panel3D, 'gui'styles'Parent::theParent, anOwner).
    ]

    #method $getCaption = theFrame $getCaption.

    #method $setCaption : aCaption
    [ 
        theFrame $setCaption:aCaption.
    ]

    #method new : aParent
    [
        super new:aParent.

        theFrame := InnerGroupBox::self.

        events'Handlers::theParent +=
            { $onInit = self $invoke. }.

        events'Handlers::self +=
            { $onResize : aSize = self $onResize:aSize.  }.
    ]

    #method $onResize : aSize
    [
        gui'prop'Size::theFrame << aSize.
    ]

    #method $onInit
    [
        gui'prop'Location::theFrame << graph'Point{ X=0. Y=0. }.
        gui'prop'Visible::theFrame << basic'True.

        theEvents $onInit.
    ]
}

// --- RadioButtonGroup ---

#class RadioButtonGroup (Panel)
{
    #field theSelectedIndex.
    
    #method $getRadioButtonGroup = self.

    #method new : aParent
    [
        super new:aParent.

        theSelectedIndex := basic'Integer << -1.
    ]

    #method $getSelectedIndex = theSelectedIndex.

    #method $setSelectedIndex : anIndex
    [
        gui'prop'Checked::(theControls @ anIndex) << basic'True.
    ]

    #method $getItemCount = theControls Count.

    #method $getObjects = { += anItem [ ] }. // !! returns idle collection at the moment

//    #method $Controls = theControls->collect'ReadOnly.

    #method $addItem : anItem
    [
        #var aButton := InnerRadioButton::self.
            
        events'Handlers::aButton +=
            { $onClick = self $onClick. }.

        gui'prop'Caption::aButton << anItem.

        aButton open.
    ]

    #method $addControl = self fail.

    #method $addRadioButton : aButton
    [
        theControls += aButton.
    ]

    #method $getItemAt : anIndex = gui'prop'Item
    {
        $getCaption = (theControls @ anIndex) $getCaption.
    }.

    #method $onClick
    [
        prop'IndexEnumerator::theControls run: aControl =>
        [
            #if(basic'True == gui'prop'Checked::aControl)?
            [
                theSelectedIndex << prop'Index::aControl.
                theEvents $onIndexChanged:theSelectedIndex.

                ^ ctrl'Break.
            ].
        ].
    ]

    #method $onResize : aSize
    [
        super $onResize:aSize.

        #var anIndex := basic'Integer << 0.
        prop'Enumerator::theControls run: aControl =>
        [
            gui'prop'Location::aControl << graph'Point{ X=4. Y=18 + (anIndex * 20). }.
            gui'prop'Size::aControl << graph'Size{ Width=aSize Width - 10. Height=20. }.
            
            anIndex += 1.
        ].
    ]

    #method << aValue
    [
        theSelectedIndex << aValue.

        #var aButton := theControls@theSelectedIndex.

        gui'prop'Checked::aButton << basic'True.
    ]

    #method basic'$toInteger = basic'IntNumber::theSelectedIndex.

    #annex IntegerExtension.
}

// --- PaintBox ---

#class Paintbox (Control)
{
    #method new : aParent
    [
        super new:aParent.

        events'Handlers::self += 
        { 
            $onPaint : aHandle = self $onRender:aHandle. 
        }.
    ]

    #method $onRender : aHandle
    [
        #var aCanvas := 'gui'graphics'Canvas::aHandle.

        theEvents $onRender:aCanvas.
    ]

    #method $createHandle 
    [
        #var anOwner := 'gui'styles'Owner::self.

        ^ 'gui'handlerType create:#group('gui'styles'Paintbox, 'gui'styles'Parent::theParent, anOwner).
    ]
}

// --- ImageBox ---

#class ImageBox(Paintbox)
{
    #method $createHandle = 'gui'handlerType create:#group('gui'styles'Imagebox, 'gui'styles'Parent::theParent).
}

// === Common helpers ===

// --- Control item indexer ---

#class Indexer
{
    #field theControl.
    #field theIndex.
    #field theRange.
    #field theItem.

    #method prop'$getIndex = basic'IntNumber::theIndex.    

    #method prop'$getContent = theItem.

    #method prop'$setContent : anItem
    [
        theControl $setItem:{ Position = theIndex. Value = anItem. }.

        theItem := theControl $getItemAt:theIndex.
    ]

    #method new : anObject
    [
        theControl := anObject $getControl.
        theIndex := basic'Integer << 0.
        theRange := prop'Range::anObject.
    ]

    #method $goto : anIndex
    [
        basic'True if:(theRange isValid:anIndex).

        theIndex << anIndex.

        theItem := theControl $getItemAt:theIndex.
    ]

    #method prop'$goto : anIndex
    [
        self $goto:anIndex.
    ]

    #method prop'$next
    [
        theIndex next.        

        #if basic'True if:(theRange isValid:theIndex)
        [   
            theItem := theControl $getItemAt:theIndex.
        ]
        | [
            theIndex previous  fail.
        ].
    ]

    #method prop'$previous
    [
        theIndex previous.

        #if basic'True if:(theRange isValid:theIndex)
        [   
            theItem := theControl $getItemAt:theIndex.
        ]
        | [
            theIndex next fail.
        ].
    ]

    #method prop'$insert : aValue
    [
        theControl $insertItem:{ Position = theIndex. Value = aValue. }.
    ]

    #method prop'$delete : aNumber
    [
        theControl $deleteItem:{ Position = theIndex. Count = 1. }.
    ]

    #annex(theItem).
}

// === Event Actions ===

#class PaintAction
{
    #method proceed : aParam
    [
        ^ events'ControllerType of:($self for) $onPaint:aParam.
    ]
}

#class ClickAction
{
    #method proceed : aParam
    [
        ^ events'ControllerType of:(self for) $onClick:aParam.
    ]
}

#class ChangeAction
{
    #method proceed : aParam
    [
        ^ events'ControllerType of:(self for) $onChange:aParam.
    ]
}

#class CommandAction
{
    #method proceed : aParam
    [
        ^ events'ControllerType of:(self for) $onCommand:aParam.
    ]
}

#class KeyPressedAction
{
    #method proceed : aParam
    [
        ^ events'ControllerType of:(self for) $onKeyPressed:aParam.
    ]
}

#class LButtonAction
{
    #method proceed : aParam
    [
        ^ events'ControllerType of:(self for) $onLButton:aParam.
    ]
}

#class CloseAction
{
    #method proceed : aParam
    [
        ^ events'ControllerType of:(self for) $onClose:aParam.
    ]
}

#class InitAction
{
    #method proceed : aParam
    [
        ^ events'ControllerType of:(self for) $onInit:aParam.
    ]
}

#class ResizeAction
{
    #method proceed : aParam
    [
        ^ events'ControllerType of:(self for) $onResize:aParam.
    ]
}
