// --- Window Messages ---

#symbol WM_QUIT            = 0012h.

// --- Window Styles ---

#symbol CS_VREDRAW = 1.
#symbol CS_HREDRAW = 2.

#symbol WS_ELENAWINDOW = 0100Bh.

#symbol SS_SIMPLE           = 0000000Bh.
#symbol SS_LEFT             = 00000000h.
#symbol SS_ETCHEDFRAME      = 00000012h.
#symbol SS_GRAYRECT         = 00000005h.
#symbol WS_OVERLAPPEDWINDOW = 00CF0000h.
#symbol WS_OVERLAPPED       = 00000000h.
#symbol WS_CAPTION          = 00C00000h.
#symbol WS_DLGFRAME         = 00400000h.
#symbol WS_VISIBLE          = 10000000h.
#symbol WS_CHILD            = 40000000h.
#symbol WS_BORDER           = 00800000h.
#symbol WS_SYSMENU          = 00080000h.
#symbol WS_TABSTOP          = 00010000h.
#symbol WS_GROUP            = 00020000h.
#symbol WS_POPUPWINDOW      = 80880000h.
#symbol WS_CLIPSIBLINGS     = 04000000h.

#symbol WS_EX_DLGMODALFRAME = 00000001h.
#symbol WS_EX_CLIENTEDGE    = 00000200h.
#symbol WS_EX_TRANSPARENT   = 00000020h.
#symbol WS_EX_WINDOWEDGE    = 00000100h.
#symbol WS_EX_OVERLAPPEDWINDOW = 00000300h.
#symbol WS_EX_TOOLWINDOW    = 00000080h. 
#symbol WS_EX_APPWINDOW     = 00040000h.
#symbol WS_EX_TOPMOST       = 00000008h.
#symbol WS_EX_CONTROLPARENT = 00010000h.
#symbol WS_EX_NOPARENTNOTIFY= 00000004h.

#symbol BS_DEFPUSHBUTTON    = 1. 
#symbol BS_GROUPBOX         = 7.
#symbol BS_AUTORADIOBUTTON  = 9.

#symbol BM_SETCHECK         = 00F1h.
#symbol BM_GETCHECK         = 00F0h.
#symbol BM_GETSTATE         = 00F2h.

#symbol BST_CHECKED         = 1.

#symbol CBS_DROPDOWN        = 2.
#symbol CBS_DROPDOWNLIST    = 3.

#symbol CB_ADDSTRING        = 0143h.
#symbol CB_DELETESTRING     = 0144h.
#symbol CB_INSERTSTRING     = 014Ah.
#symbol CB_GETCOUNT         = 0146h.
#symbol CB_GETCURSEL        = 0147h.
#symbol CB_GETLBTEXT        = 0148h.
#symbol CB_GETLBTEXTLEN     = 0149h.

#symbol LB_ADDSTRING        = 0180h.
#symbol LB_INSERTSTRING     = 0181h.
#symbol LB_DELETESTRING     = 0182h.
#symbol LB_GETCURSEL        = 0188h.
#symbol LB_GETTEXT          = 0189h.
#symbol LB_GETTEXTLEN       = 018Ah.
#symbol LB_GETCOUNT         = 018Bh.

#symbol EN_CHANGE           = 0300h.

#symbol EM_LINELENGTH       = 00C1h.

#symbol CW_USEDEFAULT       = 80000000h.

#symbol SB_SETTEXT          = 040Bh.
#symbol SB_SETPARTS	        = 0404h.
#symbol SB_GETPARTS	        = 0406h.
#symbol SB_GETTEXTLENGTH    = 040Ch.
#symbol SB_GETTEXT	        = 040Dh.

#symbol UDS_SETBUDDYINT	    = 02h.
#symbol UDS_ALIGNRIGHT      = 04h.
#symbol UDS_AUTOBUDDY	    = 10h.
#symbol UDS_ARROWKEYS	    = 20h.

// --- Window Color Constants

#symbol COLOR_BACKGROUND = 5.
#symbol COLOR_BTNFACE    = 15.
#symbol COLOR_3DFACE     = 15.

#symbol TRANSPARENT = 0.
#symbol OPAQUE = 2.

// --- Notifications ---

#symbol BN_CLICKED = 0.

// --- Cursor constants ---

#symbol IDC_ARROW = 7F00h.
#symbol IDC_IBEAM = 7F01h.

// --- Resource constants ---

#symbol IMAGE_BITMAP = 0.

#symbol LR_DEFAULTCOLOR = 0000h.
#symbol LR_LOADFROMFILE = 0010h.

#symbol BLACKNESS       = 0042h.

// --- File Access Modes ---
#symbol FILE_READ_DATA     = 00000001h.
#symbol FILE_APPEND_DATA   = 00000004h.
#symbol FILE_GENERIC_READ  = 80000000h.
#symbol FILE_GENERIC_WRITE = 40000000h.

#symbol OPEN_EXISTING      = 0003h.
#symbol CREATE_ALWAYS      = 0002h.
#symbol OPEN_ALWAYS        = 0004h.

#symbol FILE_SHARE_READ    = 00000001h.
#symbol FILE_SHARE_WRITE   = 00000002h.

#symbol WM_USER         = 0400h.

// --- Raster Operation Codes ---

#symbol SRCCOPY         = 00CC0020h.

// --- Custom messages ---

#symbol UDM_SETRANGE    = WM_USER+101.

// --- Pen Styles ---

#symbol PS_SOLID        = 0.