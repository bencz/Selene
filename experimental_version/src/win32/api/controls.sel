// --- namespace shortcuts ---
#define basic'*  = std'basic'*.
#define ctrl'*   = std'patterns'*.
#define prop'*   = std'properties'*.
#define api'*    = win32'api'*.
#define consts'* = win32'api'constants'*.
#define memory'* = std'basic'memory'*.

#define gui'*    = gui'controls'*.
#define gui'prop'* = gui'controls'properties'*.
#define graph'*  = gui'graphics'*.

// --- Control ---

#class Control
{
    #field theHandle.

    #method $getHWND = theHandle.

    #method new : aHandle
    [
        #if aHandle ifNotNil
        [
            theHandle := api'HWNDType of:aHandle.
        ].
    ]

    #method open
    [
        theHandle $show:5.
    ]

    #method close
    [
        theHandle $show:0.
    ]

    #method gui'$ifVisible
    [
        #external win32'52(theHandle).
    ]

    #method gui'$ifEnabled
    [
        #external win32'57(theHandle).
    ]

    #method gui'$enable
    [
        theHandle $enable:-1.
    ]

    #method gui'$disable
    [
        theHandle $enable:0.
    ]

    #method gui'$getCaption = theHandle $getText.

    #method gui'$setCaption : aLiteral
    [
        theHandle $setText:aLiteral.
    ]

    #method gui'$getSize = theHandle $getSize.

    #method gui'$setSize : aSize
    [
        theHandle $setSize:aSize.

        gui'ResizeAction { for = self. } proceed:aSize.
    ]

    #method gui'$getLocation = theHandle $getLocation.

    #method gui'$setLocation : aCoord
    [
        theHandle $setLocation:aCoord.
    ]

    #method free
    [
        theHandle close.
    ]

    #method refresh
    [
        theHandle refresh.
    ]

    #method $onSetCursor : anEvent
    [
        api'Cursor $set:anEvent.

        ^ 1.
    ]

    #method $OnClose
    [
        theHandle free.

        ^ 0.
    ]

    #method $onResize : aSize
    [
    ]

    #method $onSetFocus
    [
    ]

    #method $onDestroy
    [
        ^ 0.  
    ]
}

// --- Window ---

#class Window (Control)
{
    #method $onDestroy
    [
        'system -= self.

        gui'CloseAction { for = self. } proceed:self.

        ^ super $onDestroy.
    ]

    #method win32'applications'$exit
    [
        #external win32'58(theHandle, 0).
    ]   

    #method $onCommand : aMessage
    [
        #var WParam := aMessage $getWParam.
        #var LParam := aMessage $getLParam.

        #var aCode := basic'HiWord::WParam.
        #var aControl := ctrl'SearchAgent::gui'prop'Controls::self proceed:{ == aControl = aControl $getHWND == LParam. }.
        #var anAction := nil.

        #if aCode
            == consts'BN_CLICKED ? [ anAction := gui'ClickAction{ for = aControl. }. ]
            | == consts'EN_CHANGE ? [ anAction := gui'ChangeAction { for = aControl. }. ]
            | [ anAction := gui'CommandAction { for = aControl. }. ].

        anAction proceed:aControl.

        ^ 0.
    ]

    #method $onSetColorStatic : anEvent
    [
        api'HDC::{ $getHDC = anEvent $getWParam. } $setBkColor:api'SystemColor::consts'COLOR_3DFACE.

        ^ api'SystemColorBrush::consts'COLOR_3DFACE.
    ]

    #method $onSetColorButton : anEvent
	[
		api'HDC::{ $getHDC = anEvent $getWParam. } $setBkMode:consts'TRANSPARENT.

		^ api'SystemColorBrush::consts'COLOR_3DFACE.
	]

    #method $onChar : anEvent
    [
       ^ 1. // !! temporal
    ]

    #method $onKeyDown : aMessage
    [
        #var aKeyCode := aMessage $getWParam.
        #var aRetVal := basic'Integer << 1.

        gui'KeyPressedAction { for = self. } proceed:
        {
            gui'$KeyCode = aKeyCode.
            gui'$markAsProcessed [ aRetVal << 0. ]
        }.

        ^ aRetVal.
    ]

    #method $onLButton : aMessage
    [
        #var aCoord := aMessage $getLParam.
        #var anX := basic'LoWord::aCoord.
        #var anY := basic'HiWord::aCoord.
        #var aRetVal := basic'Integer << 1.

        gui'LButtonAction { for = self. } proceed:graph'Point
        { 
           X = anX. 
           Y = anY. 
           gui'$markAsProcessed = aRetVal << 0. 
        }.

        ^ aRetVal.
    ]
}

// --- UpDown ---

#class UpDown (Control)
{
    #method gui'$setRange : aRange
    [
        #var aMinValue := basic'IntegerType of:(aRange From).
        #if(aMinValue < -32767)?
            [ aMinValue := -32767. ].

        #var aMaxValue := basic'IntegerType of:(aRange Till).
        #if(aMaxValue > 32767)?
            [ aMaxValue << 32767. ].

        api'Message::{ $getID = consts'UDM_SETRANGE. $getLParam = aMaxValue shift:-16 + aMinValue. } $send:self.
    ]
}

// --- Button ---

#class Button (Control)
{
    #method $setCheck : aValue
    [
        api'Message::{ $getID = consts'BM_SETCHECK. $getWParam = aValue. } $send:self.
    ]

    #method gui'$ifChecked
    [
		#var aResult := api'Message::{ $getID = consts'BM_GETCHECK. } $return:self.

		basic'True if:(aResult==consts'BST_CHECKED).
    ]

    #method gui'$check
    [
        self $setCheck:consts'BST_CHECKED.
    ]
}

// --- Combobox ---

#class Combobox (Control)
{
    #method gui'$getSelectedIndex = api'Message::{ $getID = consts'CB_GETCURSEL. } $return:self.

    #method gui'$getItemCount = api'Message::{ $getID = consts'CB_GETCOUNT. } $return:self.

    #method gui'$getItemAt : anIndex
    [
        #var aLen := api'Message::{ $getID = consts'CB_GETLBTEXTLEN. $getWParam = anIndex. } $return:self.
        #var aLiteral := basic'LiteralType create:{ Length = aLen. }.

        api'Message::{ $getID = consts'CB_GETLBTEXT. $getWParam = anIndex. $getLParam = api'LPSTR::aLiteral. } $send:self.
        
        ^ aLiteral.
    ]

    #method gui'$addItem : aValue
    [
        #var aLPStr := api'LPSTR::aValue.

        api'Message::{ $getID = consts'CB_ADDSTRING. $getLParam = aLPStr. } $send:self.
    ]

    #method gui'$insertItem : aParam
    [
        #var aLPStr := api'LPSTR::(aParam Value).
        #var anIndex := aParam Position.

        api'Message::{ $getID = consts'CB_INSERTSTRING. $getWParam = anIndex. $getLParam = aLPStr. } $send:self.
    ]

    #method gui'$deleteItem : aParam
    [
        #var anIndex := aParam Position.

        api'Message::{ $getID = consts'CB_DELETESTRING. $getWParam = anIndex. } $send:self.
    ]
}

// --- Listbox ---

#class Listbox (Control)
{
    #method gui'$getSelectedIndex = api'Message::{ $getID = consts'LB_GETCURSEL. } $return:self.

    #method gui'$getItemCount = api'Message::{ $getID = consts'LB_GETCOUNT. } $return:self.

    #method gui'$getItemAt : anIndex
    [
        #var aLen := api'Message::{ $getID = consts'LB_GETTEXTLEN. $getWParam = anIndex. } $return:self.
        #var aLiteral := basic'LiteralType create:{ Length = aLen. }.

        api'Message::{ $getID = consts'LB_GETTEXT. $getWParam = anIndex. $getLParam = api'LPSTR::aLiteral. } $send:self.
        
        ^ aLiteral.
    ]

    #method gui'$addItem : aValue
    [
        #var aLPStr := api'LPSTR::aValue.

        api'Message::{ $getID = consts'LB_ADDSTRING. $getLParam = aLPStr. } $send:self.
    ]

    #method gui'$insertItem : aParam
    [
        #var aLPStr := api'LPSTR::(aParam Value).
        #var anIndex := aParam Position.

        api'Message::{ $getID = consts'LB_INSERTSTRING. $getWParam = anIndex. $getLParam = aLPStr. } $send:self.
    ]

    #method gui'$deleteItem : aParam
    [
        #var anIndex := aParam Position.

        api'Message::{ $getID = consts'LB_DELETESTRING. $getWParam = anIndex. } $send:self.
    ]
}

// --- Statusbar ---

#class Statusbar (Control)
{
    #method gui'$getItemCount = api'Message::{ $getID = consts'SB_GETPARTS. } $return:self.

    #method gui'$getItemAt : anIndex
    [
        #var aLen := api'Message::{ $getID=consts'SB_GETTEXTLENGTH. $getWParam = anIndex. } $return:self.
        #var aLiteral := basic'LiteralType create:{ Length = aLen. }.

        api'Message::{ $getID = consts'SB_GETTEXT. $getWParam = anIndex. $getLParam = api'LPSTR::aLiteral. } $send:self.

        ^ aLiteral.
    ]

    #method gui'$setItem : anItem
    [
        #var anIndex := basic'IntegerType of:(anItem Position).
        #var aLPStr := api'LPSTR::(anItem Value).

        api'Message::{ $getID = consts'SB_SETTEXT. $getWParam = anIndex. $getLParam = aLPStr. } $send:self.
    ]

    #method create : Widths
    [
        #var aCount := Widths Length.
        #var aDump :=  memory'ByteArrayType create:{ Length = aCount * 4. }.
        #var aWriter := prop'Writer::aDump.

        prop'Enumerator::Widths run: aWidth => ( aWriter << basic'IntegerType of:aWidth ).

        api'Message::{ $getID = consts'SB_SETPARTS. $getWParam = aCount. $getLParam = api'LPVOID::aDump. } $send:self.
    ]
}

// --- Paintbox ---

#class Paintbox (Control)
{
    #field thePaintStruct.

    #method new : aHandle
    [
        super new:aHandle.

        thePaintStruct := api'PAINTSTRUCT.
    ]

    #method $onPaint
    [
        thePaintStruct open:self.

        gui'PaintAction { for = self. } proceed:thePaintStruct.

        thePaintStruct close:self.

        ^ 0. 
    ]

    #method $onLButton : aMessage
    [
        #var aCoord := aMessage $getLParam.
        #var anX := basic'LoWord::aCoord.
        #var anY := basic'HiWord::aCoord.
        #var aRetVal := basic'Integer << 1.

        gui'LButtonAction { for = self. } proceed:graph'Point
        { 
           X = anX. 
           Y = anY. 
           gui'$markAsProcessed = aRetVal << 0. 
        }.

        ^ aRetVal.
    ]
}

// --- Imagebox ---

#class Imagebox (Paintbox)
{
    #field theImage.
    #field theImageDC.

    #method $onResize : aSize
    [
        #var aDC := theHandle $getHDC.

        theImage := api'BitmapType create:
        {
            $getSize = aSize.
            $getDC = aDC.
        }.
        theImageDC := theImage $getHDC:aDC.

        super $onResize:aSize.

        self refresh.
    ]

    #method $onClose
    [
        theImage close.
        theImageDC close.

        ^ super $invoke.
    ]

    #method gui'$setSize : aSize
    [
        //super $invoke:aSize.
        theHandle $setSize:aSize.

        self $onResize:aSize.
    ]

    #method refresh
    [
        gui'PaintAction { for = self. } proceed:theImageDC.

        super refresh.
    ]

    #method $onPaint
    [
        thePaintStruct open:self.

        #var aSize := theImage $getSize.

        #external win32'18(thePaintStruct $getHDC, 0, 0, aSize Width, aSize Height, theImageDC, 
                 0, 0, consts'SRCCOPY).        

        thePaintStruct close:self.

        ^ 0. 
    ]
}

// --- CustomControl ---

#class CustomControl(Control)
{
    #method $onDestroy
    [
        'system -= self.

        ^ super $onDestroy.
    ]

    #method $onUserEvent : aMessage
    [
	    //theEventController $onUserEvent:aMessage.
    ]

    #method $onClose
    [
        //theEventController::$onClose.

        ^ 0.
    ]
}
