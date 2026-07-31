// --- namespace shortcuts ---
#define basic'*  = std'basic'*.
#define api'*      = win32'api'*.
#define consts'*   = win32'api'constants'*.
#define controls'* = win32'api'controls'*.
#define factories'* = win32'api'factories'*.

// === Class styles ===

// --- SDIWindowClass ---

#symbol SDIWindowClass =
{
    $getStyle = consts'WS_ELENAWINDOW.

    $getClassName = "ELENA.SDIWINDOW.3.0".

    $getBackground = consts'COLOR_BACKGROUND.

    $createClass
    [
        ^ api'ClassType create:self.
    ]
}.

// --- PanelClass ---

#symbol PanelClass =
{
    $getStyle = consts'CS_VREDRAW or:consts'CS_HREDRAW.

    $getClassName = "ELENA.PANEL.3.0".

    $getBackground = consts'COLOR_BACKGROUND.

    $createClass
    [
        ^ api'ClassType create:self.
    ]
}.

// --- PaintboxClass ---

#symbol PaintboxClass =
{
    $getStyle = consts'CS_VREDRAW or:consts'CS_HREDRAW.

    $getClassName = "ELENA.Paintbox.3.0".

    $getBackground = 0.

    $createClass
    [
        ^ api'ClassType create:self.
    ]
}.

#symbol NonvisibleWindowClass =
{
    $getStyle = 0.

    $getClassName = "ELENA.NONVISIBLE.3.0".

    $getBackground = 0.

    $createClass
    [
        ^ api'ClassType create:self.
    ]
}.

// === Form styles ===

// --- SDIWindow ---

#symbol SDIWindow =
{
    $getWindowHandle = controls'Window.

    $getState : aControl = 'system += aControl.

    $getStyle = consts'WS_OVERLAPPEDWINDOW.

    $getExStyle = consts'WS_EX_OVERLAPPEDWINDOW.

    $getClass = factories'SDIWindowClass.

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- Dialog ---

#symbol Dialog =
{
    $getWindowHandle = controls'Window.

    $getState : aControl = 'system += aControl.

    $getStyle = /*consts'WS_CLIPSIBLINGS or:*/consts'WS_POPUPWINDOW or:consts'WS_DLGFRAME.

    $getExStyle = consts'WS_EX_DLGMODALFRAME.

    $getClass = factories'SDIWindowClass.

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- SDIDialog ---

#symbol SDIDialog =
{
    $getWindowHandle = controls'Window.

    $getState : aControl = 'system += aControl.

    $getStyle = consts'WS_OVERLAPPED or:consts'WS_CAPTION or:consts'WS_SYSMENU.

    $getExStyle = consts'WS_EX_DLGMODALFRAME or:consts'WS_EX_APPWINDOW.

    $getClass = factories'SDIWindowClass.

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- StaticFrame ---

#symbol StaticFrame =
{
    $getWindowHandle = controls'Control.

    $getState = 0.

    $getStyle = consts'WS_CHILD or:consts'SS_ETCHEDFRAME.

    $getExStyle = consts'WS_EX_TRANSPARENT.

    $getClass = "Static".

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- StaticLabel ---

#symbol StaticLabel =
{
    $getWindowHandle = controls'Control.

    $getState = 0.

    $getStyle = consts'WS_CHILD/* or:consts'SS_SIMPLE*/.

    $getExStyle = 0.

    $getClass = "Static".

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- GroupBox3D --

#symbol GroupBox3D =
{
    $getWindowHandle = controls'Control.

    $getState = 0.

    $getStyle = consts'WS_CHILD or:consts'BS_GROUPBOX.

    $getExStyle = consts'WS_EX_TRANSPARENT.

    $getClass = "Button".

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- Edit3D ---

#symbol Edit3D =
{
    $getWindowHandle = controls'Control.

    $getState = 0.

    $getStyle = consts'WS_CHILD or:consts'WS_BORDER or:consts'WS_TABSTOP.

    $getExStyle = consts'WS_EX_CLIENTEDGE.

    $getClass = "Edit".

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- UpDown3D ---

#symbol UpDown3D =
{
    $getWindowHandle = controls'UpDown.

    $getState = 0.

    $getStyle = consts'WS_CHILD or:consts'UDS_SETBUDDYINT or:consts'UDS_ALIGNRIGHT or:consts'UDS_AUTOBUDDY or:consts'UDS_ARROWKEYS.

    $getExStyle = consts'WS_EX_NOPARENTNOTIFY.

    $getClass = "msctls_updown32".

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- Button3D ---

#symbol Button3D =
{
    $getWindowHandle = controls'Control.

    $getState = 0.

    $getStyle = consts'WS_CHILD or:consts'BS_DEFPUSHBUTTON or:consts'WS_TABSTOP.

    $getExStyle = 0.

    $getClass = "Button".

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- RadioButton3D ---

#symbol RadioButton3D =
{
    $getWindowHandle = controls'Button.

    $getState = 0.

    $getStyle = consts'WS_CHILD or:consts'BS_AUTORADIOBUTTON or:consts'WS_TABSTOP.

    $getExStyle = 0.

    $getClass = "Button".

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- Combobox3D ---

#symbol Combobox3D =
{
    $getWindowHandle = controls'Combobox.

    $getState = 0.

    $getStyle = consts'WS_CHILD or:consts'CBS_DROPDOWNLIST or:consts'WS_TABSTOP.

    $getExStyle = consts'WS_EX_CLIENTEDGE.

    $getClass = "Combobox".

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- Listbox3D ---

#symbol Listbox3D =
{
    $getWindowHandle = controls'Listbox.

    $getState = 0.

    $getStyle = consts'WS_CHILD.

    $getExStyle = consts'WS_EX_CLIENTEDGE.

    $getClass = "Listbox".

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- StatusBar3D ---

#symbol StatusBar3D =
{
    $getWindowHandle = controls'Statusbar.

    $getState = 0.

    $getStyle = consts'WS_CHILD.

    $getExStyle = consts'WS_EX_CLIENTEDGE.

    $getClass = "msctls_statusbar32".

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- Panel3D ---

#symbol Panel3D =
{
    $getWindowHandle = controls'Window.

    $getState : aControl = 'system += aControl.

    $getStyle = consts'WS_CHILD or:consts'WS_CLIPSIBLINGS.

    $getExStyle = consts'WS_EX_CONTROLPARENT.

    $getClass = factories'PanelClass.

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- Paintbox --

#symbol Paintbox =
{
    $getWindowHandle = controls'Paintbox.

    $getState : aControl = 'system += aControl.

    $getStyle = consts'WS_CHILD.

    $getExStyle = consts'WS_EX_TRANSPARENT.

    $getClass = factories'PaintboxClass.

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- Imagebox --

#symbol Imagebox =
{
    $getWindowHandle = controls'Imagebox.

    $getState : aControl = 'system += aControl.

    $getStyle = consts'WS_CHILD.

    $getExStyle = consts'WS_EX_TRANSPARENT.

    $getClass = factories'PaintboxClass.

    $createHandler
    [
        ^ api'HWNDType create:self.
    ]
}.

// --- Parent ---

#symbol Parent : aParent =
{
    $getParent = aParent $getHWND.
}.

// --- Owner --

#symbol Owner : anOwner =
{
    $getOwner = anOwner.
}.

// --- Width ---

#symbol Width : aWidth =
{
    $getWidth = aWidth.
}.

// --- Color ---

#symbol Color : aColor = 
{
    $getColor = aColor.
}.

// --- Destination ---

#symbol Destination : aCoord =
{
    $getDestination = aCoord.
}.

// --- Source ---

#symbol Source : aCoord =
{
    $getSource = aCoord.
}.

// --- Text ---

#symbol Text : aText =
{
    $getText = basic'LiteralType of:aText.
}.

// --- FileName --

#symbol FileName : aText =
{
    $getFileName = basic'LiteralType of:aText.
}.

// --- ImageCopy --

#symbol ImageCopy : aImage =
{
    $getSize = aImage $getSize.
    $getHDC  = aImage $getHDC.
    $getOperation = consts'SRCCOPY.
    $getSource = { X = 0. Y = 0. }.
}.
