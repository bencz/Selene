// --- Win32 Binary Package

// --- WIN32_GETSTDHANDLE ---

procedure win32'1 (handle)

  push 0FFFFFFF5h            // STD_OUTPUT_HANDLE   
  call 'dlls'kernel32.GetStdHandle  
  cmp  eax, -1
  jz   short lErr
  mov  ebx, eax
  mov  eax, handle
  mov  [eax], ebx
  ret
lErr:
  xor  eax, eax
  ret

end

// --- WCON_PRINT (EAX - S) ---

procedure win32'2 (handle, s)
                     
  mov  ebx, s
  push 00
  mov  eax, esp
  push 00                    // place lpReserved
  push eax
  push dword ptr [ebx]       // place lenght
  add  ebx, 4
  push ebx
  mov  eax, handle
  push [eax]
  call 'dlls'kernel32.WriteConsoleW
  pop  ebx
  test eax, eax
  jz   short lEnd
  mov  eax, s
lEnd:
  ret
end

// --- CON_READ ---

procedure win32'3 (handle, buf)

   mov  eax, buf
   mov  ecx, [eax-8]
   shl  ecx, 1
   sub  ecx, 4
   push 00                    // place lpReserved
   push eax
   push ecx
   lea  eax, [eax+4]
   push eax
   mov  eax, handle
   push [eax]
   call 'dlls'kernel32.ReadConsoleW
   test eax, eax
   jz   short lEnd
   mov  eax, buf

   lea  edx, [eax+4]
   add  edx, [eax]
   add  edx, [eax]
   xor  ebx, ebx
   mov  word ptr[edx], bx

   lea  edx, [edx-4]
   cmp  word ptr[edx], 0Dh   
   jnz  short lEnd
   sub  [eax], 2
   mov  word ptr [edx], bx
lEnd:
   ret

end

// --- WIN32_SETINSTDHANDLE ---

procedure win32'4 (handle)

  push 0FFFFFFF6h            // STD_INPUT_HANDLE   
  call 'dlls'kernel32.GetStdHandle  
  cmp  eax, -1
  jz   short lErr
  mov  ebx, eax
  mov  eax, handle
  mov  [eax], ebx
  ret
lErr:
  xor  eax, eax
  ret

end

// --- WIN32_GETCHAR ---

procedure win32'5 (handle, char)

  lea  esp, [esp-20]
  mov  edx, esp
lWait:
  push 0
  push esp
  push 1
  push edx 
  mov  ebx, handle
  push [ebx]  
  call 'dlls'kernel32.ReadConsoleInputW
  pop  ebx
  test eax, eax
  jz   short lErr
  mov  edx, esp
  cmp  word ptr [edx], 1
  jnz  short lWait
  mov  ecx, [edx+14]
  and  ecx, 0FFFFh
  lea  esp, [esp+20]
  mov  eax, char
  mov  [eax], ecx
  ret

lErr:
  lea  esp, [esp+20]
  ret

end


// --- WIN32_SEEKEOL (OutputLength, Buffer, Index) ---

inline win32'6

  pop eax 
  pop ecx

  lea esi, [ecx+4]
  mov ebx, [eax]
  mov edx, ebx
  add esi, ebx  
labSeek:
  cmp ebx, [ecx]
  je  short labEnd
  add ebx, 1
  cmp byte ptr [esi], 10
  je  short labEnd
  add esi, 1
  jmp short labSeek 
  
labEnd:  
  sub ebx, edx
  mov eax, [esp]
  mov [eax], ebx

end

// --- WIN32_COPYWLINE  (Line, Length, Buffer, Index) ---

inline win32'7

  pop  ebx
  pop  eax
  mov  ecx, [esp+4]

  lea  edx, [ecx+4]
  pop  esi
  mov  ecx, [ecx]
  mov  esi, [esi]
  cmp  esi, ecx
  jge  short lStart
  mov  ecx, esi
  mov  [edx-4], ecx

lStart:
  lea  esi, [eax+4]
  add  esi, [ebx]
  add  [ebx], ecx
  test ecx, ecx
  jz   short labEnd  
labCopy:
  mov  ebx, [esi]
  and  ebx, 0FFh
  mov  word ptr [edx], bx
  lea  edx, [edx+2]
  add  esi, 1
  sub  ecx, 1
  jnz  short labCopy
  xor  ebx, ebx
  mov  word ptr [edx], bx

labEnd:

end

// --- WIN32_NOTWEOL  (Buffer, Index) ---

inline win32'8

  pop  eax
  mov  ebx, [eax]
  mov  eax, [esp]
  cmp  [eax], ebx
  jbe  short labEnd
  lea  esi, [eax+3]
  add  esi, ebx
  mov  edx, [esi]
  cmp  dl,  10
  jnz  short labEnd
labFail:
  xor  eax, eax
labEnd:  

end

// --- WIN32_CREATEFILE ---

procedure win32'9 (hHandle, sFileName, dwDesiredAccess, dwShareMode, 
                    dwCreationDisposition, dwFlagsAndAttributes)

  push 0                                  // hTemplateFile
  mov  eax, dwFlagsAndAttributes
  push [eax]                              // dwFlagsAndAttributes
  mov  eax, dwCreationDisposition
  push [eax]                              // dwCreationDisposition
  push 0                                  // lpSecurityAttributes
  mov  eax, dwShareMode
  push [eax]                              // dwShareMode
  mov  eax, dwDesiredAccess
  push [eax]                              // dwDesiredAccess 
  mov  eax, sFileName
  push [eax]                              // lpFileName

  call 'dlls'kernel32.CreateFileW
  cmp  eax, 0FFFFFFFFh
  jz   short lErr

  mov  ebx, eax
  mov  eax, hHandle
  mov  [eax], ebx
  ret
lErr:
  xor  eax, eax
  ret

end

// --- WIN32_READFILE (EAX - hFile, EBX - Buffer) ---

procedure win32'10 (hfile, buffer)

  mov  eax, hfile
  mov  ebx, buffer
  push 0                                  // lpOverlapped
  mov  ecx, [ebx-8]
  shl  ecx, 2
  sub  ecx, 4
  push ebx 
  push ecx
  lea  ecx, [ebx+4]
  push ecx
  push [eax]

  call 'dlls'kernel32.ReadFile
  
  test eax, eax
  jz   short lbFailed
  mov  ebx, buffer
  mov  ebx, [ebx]
  test ebx, ebx
  jz   short lbFailed
  mov  eax, hfile
  ret
lbFailed:
  xor  eax, eax
  ret

end

// --- WIN32_WRITELITERAL ---

procedure win32'11 (hfile, dump, length)

  mov  eax, hfile
  mov  ebx, dump
  push 0
  mov  edx, esp

  push 0                                  // lpOverlapped
  push edx 
  mov  ecx, length
  push [ecx]
  push ebx
  push [eax]

  call 'dlls'kernel32.WriteFile

  pop  ecx
  test eax, eax
  jz   short lbFailed

  mov  eax, hfile
lbFailed:
  ret

end

// --- WIN32_CLOSEHANDLE ---

procedure win32'12 (handle)

  mov  eax, handle

  push [eax]
  call 'dlls'kernel32.CloseHandle
  test eax, eax
  jz   short lEnd

  mov  eax, handle
lEnd:
  ret

end

// --- WIN32_GETCMDLINE_LEN ---

procedure win32'13 (len)

  call 'dlls'kernel32.GetCommandLineW
  xor  ebx, ebx
  mov  edx, eax
lScan:
  cmp  word ptr [edx], bx
  lea  edx, [edx+2]
  jnz  short lScan
  sub  edx, eax
  shr  edx, 1
  sub  edx, 1
  mov  eax, len
  mov  [eax], edx
  ret

end

// --- WIN32_GETCMDLINE ---


procedure win32'14 (str)

  call 'dlls'kernel32.GetCommandLineW
  mov  esi, eax
  mov  eax, str
  mov  ecx, [eax]
  add  ecx, 2
  shr  ecx, 1

  lea  edx, [eax+4]

lCopy:
  mov  ebx, [esi]
  mov  [edx], ebx
  lea  edx, [edx+4]
  lea  esi, [esi+4]
  sub  ecx, 1
  jnz  short lCopy

  ret

end


// --- LP_SET

inline win32'15

  pop  eax
  mov  edx, [esp]
  mov  [edx], eax
  mov  eax, edx

end

// --- WIN32_REFRESH ---

procedure win32'16 (hwnd)

  push 1
  push 0
  mov  eax, hwnd
  push [eax]
  call 'dlls'user32.InvalidateRect
  mov  eax, hwnd
  push [eax]
  call 'dlls'user32.UpdateWindow
  ret

end

// --- WIN32_DESTROY ---

procedure win32'17 (handle)

  mov  eax, handle
  push [eax]
  call 'dlls'user32.DestroyWindow
  mov  eax, handle
  ret

end

// --- WIN32_GETDC ---

procedure win32'18 (hdc, hwnd)

  mov  eax, hwnd
  push [eax]
  call 'dlls'user32.GetDC
  test eax, eax
  jz   short lEnd
  mov  ebx, hdc
  mov  [ebx], eax
  mov  eax, ebx

lEnd:
  ret

end

// --- WIN32_SHOW ---

procedure win32'19 (handle, code)

  mov  ebx, code
  mov  eax, handle
  push [ebx] 
  push [eax]
  call 'dlls'user32.ShowWindow
  mov  eax, handle
  ret

end


// --- WIN32_ENABLE ---

procedure win32'20 (handle, code)

  mov  ebx, code
  mov  eax, handle
  push [ebx] 
  push [eax]
  call 'dlls'user32.EnableWindow
  mov  eax, handle
  ret

end

// --- WIN32_GETTEXT ---

procedure win32'21 (handle, s)

  mov  ebx, s
  mov  eax, handle
  lea  ebx, [ebx+4]
  push ebx
  mov  ecx, [ebx-4]
  add  ecx, 1
  push ecx
  push 13
  push [eax]
  call 'dlls'user32.SendMessageW
  mov  ebx, s
  mov  [ebx], eax
  mov  eax, handle
  ret

end

// --- WIN32_SETTEXTW ---

procedure win32'22 (handle, lpstr)

  mov  eax, handle
  mov  ebx, lpstr
  mov  ebx, [ebx]
  push ebx
  push 0
  push 12
  push [eax]
  call 'dlls'user32.SendMessageW
  cmp eax, 1
  jnz  short lErr
  mov  eax, handle
  ret
lErr:
  xor  eax, eax
  ret

end

// --- WIN32_GETRECT ---

procedure win32'23 (hwnd, x1, y1, x2, y2)

  lea  esp, [esp-16]
  mov  edx, esp
  push edx
  mov  ebx, hwnd
  push [ebx] 
  call 'dlls'user32.GetClientRect
  pop  ebx
  mov  ecx, x1
  mov  [ecx], ebx
  pop  ebx
  mov  ecx, y1
  mov  [ecx], ebx
  pop  ebx
  mov  ecx, x2
  mov  [ecx], ebx
  pop  ebx
  mov  ecx, y2
  mov  [ecx], ebx
  mov  eax, hwnd
  ret 

end

// --- WIN32_SETSIZE ---

procedure win32'24 (handle, wdth, hght)

  xor  eax, eax
  push 36h
  mov  ecx, hght
  push [ecx]
  mov  ebx, wdth
  push [ebx]
  push eax
  push eax
  push eax
  mov  ebx, handle
  push [ebx]
  call 'dlls'user32.SetWindowPos
  test eax, eax
  jnz  short lEnd
lEnd:
  mov  eax, handle
  ret

end

// --- WIN32_SETPOS ---

procedure win32'25 (handle, x, y)

  xor  eax, eax
  push 35h
  push eax
  push eax
  mov  ecx, y
  push [ecx]
  mov  ebx, x
  push [ebx]
  push eax
  mov  ebx, handle
  push [ebx]
  call 'dlls'user32.SetWindowPos
  test eax, eax
  jnz  short lEnd
lEnd:
  mov  eax, handle
  ret

end

// --- WIN32_SETCURSOR (EAX - EVENT, EBX - hICON) ---

procedure win32'26 (event, hicon)

  mov  eax, event
  mov  edx, [eax+4]
  and  edx, 0FFFFh
  cmp  edx, 1
  je   short labClient
  xor  eax, eax
  ret

labClient:
  mov  ebx, hicon
  mov  ebx, [ebx]
  push ebx
  push 0
  call 'dlls'user32.LoadCursorW
  test eax, eax
  jz   short lEnd
  push eax
  call 'dlls'user32.SetCursor
  mov  eax, event
lEnd:
  ret

end

// --- WIN32_PEEKMSG ---

procedure win32'27 (msg, handle)

  mov  eax, msg
  mov  ebx, handle
  xor  ecx, ecx
  push 1
  push ecx  
  push ecx
  push [ebx]
  push eax
  call 'dlls'user32.PeekMessageW
  test eax, eax
  jz   short lab1
  mov  eax, msg
lab1:
  ret 

end

// --- WIN32_PROCMSG2 ---

procedure win32'28 (Msg, Hwnd)

  mov  eax, Msg
  mov  ebx, Hwnd
  push eax  
  push [ebx]
  call 'dlls'user32.IsDialogMessage
  test eax, eax
  jnz  short labEnd
  mov  eax, Msg
  push eax
  push eax
  call 'dlls'user32.TranslateMessage
  call 'dlls'user32.DispatchMessageW
labEnd:
  mov  eax, Msg
  ret

end

// --- WIN32_WAITMSG ---

procedure win32'29 (msg)

  call 'dlls'user32.WaitMessage
  mov  eax, msg
  ret 

end


// --- WIN32_SENDANDRETURNMESSAGEW ---

procedure win32'30 (retval, hndl, msg)

  mov  ebx, msg
  mov  eax, hndl
  push [ebx+8]
  push [ebx+4]
  push [ebx]
  push [eax]
  call 'dlls'user32.SendMessageW
  mov  ebx, eax
  mov  eax, retval
  mov  [eax], ebx
  ret  

end

// --- WIN32_SENDMESSAGEW ---

procedure win32'31 (hndl, msg)

  mov  ebx, msg
  mov  eax, hndl
  push [ebx+8]
  push [ebx+4]
  push [ebx]
  push [eax]
  call 'dlls'user32.SendMessageW
  mov  eax, hndl
  ret

end

// --- WIN32_POSTMESSAGE ---

procedure win32'32 (hndl, msg)

  mov  ebx, msg
  mov  eax, hndl
  push [ebx+8]
  push [ebx+4]
  push [ebx]
  push [eax]
  call 'dlls'user32.PostMessageW
  test eax, eax
  jz   short lEnd
  mov  eax, hndl
lEnd:
  ret

end

// --- WIN32_SETTEXTCOLOR ---

procedure win32'33 (hdc, color)

  mov  eax, hdc
  mov  ebx, color
  push [ebx]
  push [eax]

  call 'dlls'gdi32.SetTextColor
  cmp  eax, 0FFFFFFFFh
  jz   short lErr

  mov  eax, hdc
  ret
lErr:
  xor  eax, eax
  ret

end

// --- WIN32_SETBKMODE ---

procedure win32'34 (hdc, mode)

  mov  eax, hdc
  mov  ebx, mode
  push [ebx]
  push [eax]

  call 'dlls'gdi32.SetBkMode
  test eax, eax
  jz   short lEnd

  mov  eax, hdc
lEnd:
  ret

end

// --- WIN32_SETBKCOLOR ---

procedure win32'35 (hdc, color)

  mov  eax, hdc
  mov  ebx, color
  push [ebx]
  push [eax]

  call 'dlls'gdi32.SetBkColor
  cmp  eax, 0FFFFFFFFh
  jz   short lErr

  mov  eax, hdc
  ret
lErr:
  xor  eax, eax
  ret

end

// --- WIN32_FREEDC ---

procedure win32'36 (hdc)

  mov  eax, hdc
  push [eax]
  call 'dlls'gdi32.DeleteDC
  test eax, eax
  jz   short lEnd
  mov  eax, hdc
lEnd:
  ret

end

// --- WIN32_FREEBITMAP (EAX - hBITMAP) ---

procedure win32'37 (hbitmap)

  mov  eax, hbitmap  
  push [eax]
  call 'dlls'gdi32.DeleteObject
  test eax, eax
  jz   short lEnd
  mov  eax, hbitmap
lEnd:
  ret

end

// --- WIN32_BEGINPAINT ---

procedure win32'38(handle, paintstruct)

  mov  ebx, paintstruct
  push ebx
  mov  eax, handle
  mov  eax, [eax]
  push eax
  call 'dlls'user32.BeginPaint
  test eax, eax
  jz   short lEnd
  mov  eax, handle
lEnd:
  ret   

end


// --- WIN32_ENDPAINT ---

procedure win32'39 (handle, paintstruct)

  mov  ebx, paintstruct
  push ebx
  mov  eax, handle
  mov  eax, [eax]
  push eax
  call 'dlls'user32.EndPaint
  mov  eax, handle
  ret   

end

// --- WIN32_GETDIMBITMAP ---

procedure win32'40 (hbitmap, width, height)
  
  mov  ecx, hbitmap
  mov  edx, [ecx]
  sub  esp, 24
  push esp
  push 24
  push edx
  call 'dlls'gdi32.GetObjectA
  test eax, eax
  jz   short lErr
  pop  eax                   // to skip type 
  pop  eax                   // get width
  mov  ecx, width
  mov  [ecx], eax
  pop  eax                   // get height
  mov  ebx, height
  mov  [ebx], eax
  add  esp, 12
  mov  eax, hbitmap
  ret
lErr:
  lea  esp, [esp+24]
  ret

end

// --- WIN32_GETBITMAPHDC ---

procedure win32'41 (hbitmap, sour, dest)

  mov  eax, sour 
  mov  ebx, dest
  push [eax]
  call 'dlls'gdi32.CreateCompatibleDC
  test eax, eax
  jz   short lEnd
  mov  [ebx], eax  
  mov  ecx, hbitmap
  push [ecx]
  push eax
  call 'dlls'gdi32.SelectObject 
  cmp  eax, 0FFFFFFFFh
  jz   short lErr
  mov  eax, sour
lEnd:
  ret
lErr:
  xor  eax, eax
  ret

end

// --- WIN32_SETWNDPROC ---

procedure win32'42 (obj)

  mov  edx, 'windproc
  mov  eax, obj
  mov  [eax], edx
  ret

end

// --- WIN32_GETSYSCOLORBRUSH ---

procedure win32'43 (hbrush, index)

  mov  ebx, index
  push [ebx]

  call 'dlls'user32.GetSysColorBrush
  test eax, eax
  jz   short lEnd

  mov  ebx, eax
  mov  eax, hbrush
  mov  [eax], ebx

  mov  ebx, index
  push [ebx]
  call 'dlls'user32.GetSysColor
  mov  ebx, eax
  mov  eax, hbrush
  mov  [eax+4], ebx

lEnd:
  ret

end

// --- WIN32_GETSYSCOLOR ---

procedure win32'44 (dwrd, index)

  mov  ebx, index
  push [ebx]

  call 'dlls'user32.GetSysColor

  mov  ebx, eax
  mov  eax, dwrd
  mov  [eax], ebx
  ret

end

// --- WIN32_CREATEWNDW ---

procedure win32'45 (hHandle, dwExStyle, pstrClassName, pstrName, dwStyle, 
                   dwX, dwY, dwWidth, dwHeight, hParent, hMenu, hInstance,
                   lpParam)

  mov  eax, lpParam                            
  push [eax]                              // lpparam
  mov  eax,  hInstance
  push [eax]                              // hinstance
  mov  eax, hMenu
  push [eax]                              // menu
  mov  eax, hParent
  push [eax]                              // wndparent
  mov  eax, dwHeight
  push [eax]                              // height
  mov  eax, dwWidth
  push [eax]                              // width   
  mov  eax, dwY
  push [eax]                              // y
  mov  eax, dwX
  push [eax]                              // x               
  mov  eax, dwStyle
  push [eax]                              // dwstyle
  mov  eax, pstrName
  push [eax]                              // windowname
  mov  eax, pstrClassName
  push [eax]                              // classname 
  mov  eax, dwExStyle
  push [eax]                              // exstyle
  call 'dlls'user32.CreateWindowExW
  test eax, eax
  jz   short lEnd
  mov  ebx, eax
  mov  eax, hHandle
  mov  [eax], ebx
lEnd:
  ret

end

// --- WIN32_REGWNDPROC ---

procedure win32'46 (obj, wndclassrec)

  mov  ebx, wndclassrec
  push ebx 
  call 'dlls'user32.RegisterClassW
  test eax, eax
  jz   short lEnd
  mov  eax, obj
lEnd:
  ret

end

// --- WIN32_NEWBITMAP ---

procedure win32'47 (hbitmap, wh, ht, hdc)

  mov  ebx, ht
  push [ebx]
  mov  ecx, wh
  push [ecx]
  mov  edx, hdc
  push [edx]
  call 'dlls'gdi32.CreateCompatibleBitmap
  mov  ebx, hbitmap
  mov  [ebx], eax
  mov  eax, ebx
  ret

end

// --- WIN32_LOADIMAGEW ---

procedure win32'48 (hBitmap, hInst, pstrFileName, uType, nX, nY, fuLoad)

  mov  eax, fuLoad  
  push [eax]          // fuload
  mov  eax, nY
  push [eax]          // cy
  mov  eax, nX
  push [eax]          // cx
  mov  eax, uType
  push [eax]          // utype
  mov  eax, pstrFileName
  push [eax]          // lpszName
  mov  eax, hInst
  push [eax]          // hinst

  call 'dlls'user32.LoadImageW
  test eax, eax
  jz   short lbEnd

  mov  ebx, eax
  mov  eax, hBitmap
  mov  [eax], ebx

lbEnd:
  ret

end

// --- WIN32_CREATEPEN ---

procedure win32'49 (handle, style, width, color)

  mov  edx, color
  push [edx]
  mov  ebx, width
  push [ebx] 
  mov  eax, style
  push [eax]
  call 'dlls'gdi32.CreatePen
  mov  ebx, eax
  test eax, eax
  jz   short lEnd
  mov  eax, handle
  mov  [eax], ebx
lEnd:
  ret

end


// --- WIN32_CREATEBRUSH ---

procedure win32'50 (handle, color)

  mov  edx, color
  push [edx]
  call 'dlls'gdi32.CreateSolidBrush
  mov  ebx, eax
  test eax, eax
  jz   short lEnd
  mov  eax, handle
  mov  [eax], ebx
lEnd:
  ret

end

// --- WIN32_GETMSGW ---

procedure win32'51 (msg)

  mov  eax, msg
  xor  ecx, ecx
  push ecx  
  push ecx
  push ecx
  push eax
  call 'dlls'user32.GetMessageW
  test eax, eax
  jz   short lab1
  mov  eax, msg
lab1:
  ret 

end

// --- WIN32_ISVISIBLE ---

procedure win32'52 (handle)

  mov  eax, handle
  push [eax]
  call 'dlls'user32.IsWindowVisible
  test eax, eax
  jz   short lEnd
  mov  eax, handle
lEnd:
  ret

end

// --- WIN32_CLEAR ---

procedure win32'53 (handle)

  push 0
  mov  ebx, esp
  push ebx
  push [eax]
  call 'dlls'kernel32.GetNumberOfConsoleInputEvents
  pop  ecx

  test ecx, ecx
  jz   short lEnd

  lea  esp, [esp-20]
  mov  edx, esp
lNext:
  push 0
  push esp
  push 1
  push edx 
  mov  ebx, handle
  push [ebx]  
  call 'dlls'kernel32.ReadConsoleInputW

  sub  ecx, 1
  jnz  short lNext
  lea  esp, [esp+20]

lEnd:  
  mov  eax, handle
  ret

end

// --- WIN32_UNICODETOANSI ---

procedure win32'54 (dump, str, length)

  mov  eax, str
  mov  ebx, dump
  mov  ecx, length
  mov  ecx, [ecx]
  test ecx, ecx
  jz   short lEnd

  lea  eax, [eax+4]  // str

  push 0
  mov  edx, esp
  push 0
  push 63
  mov  esi, esp

  push edx
  push esi
  push ecx
  push ebx
  push ecx
  push eax
  push 400h
  push 1 
  call 'dlls'kernel32.WideCharToMultiByte
  lea  esp, [esp+12]
  test eax, eax
  jz   short lEnd
  mov  eax, str
lEnd:
  ret

end


// --- WIN32_POOLADD ---

procedure win32'55 (pobjects, obj, index)

  push edi
  mov  ebx, index
  mov  edi, pobjects  
  mov  eax, obj
  mov  ecx, [edi-8]
  shl  ecx, 2
  mov  esi, edi
  add  [ebx], 4
  add  esi, [ebx]
labCheck:
  cmp  ecx, [ebx]
  jbe  short labErr
  cmp  [esi], 'nil
  jnz  short labNext

  call @"$package'elena'31"     // assign eax to [esi]
  jmp  short labEnd
labNext:      
  add  [ebx], 4
  add  esi, 4
  jmp  short labCheck

labEnd:
  mov  eax, index
  pop  edi
  ret
labErr:
  pop  edi
  xor  eax, eax
  ret

end

// --- WIN32_POOLREMOVE (EAX - POBJECTS, EBX - Nil) ---

procedure win32'56 (pobjects, object)

  mov  edx, pobjects
  mov  ebx, object
  mov  ecx, [edx-8]
labNext:
  test ecx, ecx
  jz   short labEnd
  cmp  [edx], ebx
  lea  edx, [edx+4]
  sub  ecx, 1
  jnz  short labNext
  mov  [edx], 'nil
labEnd:
  mov  eax, pobjects
  ret

end

// --- WIN32_ISENABLE ---

procedure win32'57 (handle)

  mov  eax, handle
  push [eax]
  call 'dlls'user32.IsWindowEnabled
  test eax, eax
  jz   short lEnd
  mov  eax, handle
lEnd:
  ret

end

// --- WIN32_QUIT ---

procedure win32'58 (handle, code)

  mov  ebx, code
  push [ebx]
  call 'dlls'user32.PostQuitMessage
  mov  eax, handle
  ret

end


// --- WIN32_BITBLT ---

procedure win32'59 (hdcDest, nXDest, nYDest, nWidth, nHeight, hdcSour, nXSour, nYSour, dwOp)

  mov  eax, dwOp
  push [eax]          // dwrop
  mov  eax, nYSour
  push [eax]          // nysrc
  mov  eax, nXSour 
  push [eax]          // nxsrc 
  mov  eax, hdcSour
  push [eax]          // hdcsrc
  mov  eax, nHeight
  push [eax]          // nheight
  mov  eax, nWidth
  push [eax]          // nwidth
  mov  eax, nYDest
  push [eax]          // nydest
  mov  eax, nXDest
  push [eax]          // nxdest 
  mov  eax, hdcDest
  push [eax]          // hdcdest
  call 'dlls'gdi32.BitBlt
  test eax, eax
  jz   short lEnd
  mov  eax, hdcDest
lEnd:
  ret

end

// --- WIN32_MOVETO ---

procedure win32'60 (hdc, x, y)

  xor  ebx, ebx
  push ebx
  mov  ecx, y
  push [ecx]
  mov  edx, x
  push [edx]
  mov  eax, hdc
  push [eax]
  call 'dlls'gdi32.MoveToEx
  test eax, eax
  jz   short lEnd
  mov  eax, hdc
lEnd:
  ret

end

// --- WIN32_LINETO ---

procedure win32'61 (hdc, x, y, hpen)

  mov  eax, hpen
  push [eax]
  mov  ebx, hdc
  push [ebx]
  call 'dlls'gdi32.SelectObject // select the hpen and save previous one
  push eax

  mov  ecx, y
  push [ecx]
  mov  edx, x
  push [edx]
  mov  eax, hdc
  push [eax]
  call 'dlls'gdi32.LineTo
  pop  ebx
  mov  ecx, hdc
  push eax
  push ebx
  push [ecx]

  call 'dlls'gdi32.SelectObject  // restore previous hpen
  
  pop  eax                       // restore result
  test eax, eax
  jz   short lEnd
  mov  eax, hdc
lEnd:
  ret

end

// --- WIN32_TEXTOUT ---

procedure win32'62 (hdc, x, y, text, len)

  mov  ecx, len
  push [ecx]
  mov  edx, text
  push [edx]
  mov  ecx, y
  push [ecx]
  mov  edx, x
  push [edx]
  mov  eax, hdc
  push [eax]
  call 'dlls'gdi32.TextOutW

  test eax, eax
  jz   short lEnd
  mov  eax, hdc
lEnd:
  ret

end

// --- WIN32_SETTEXTCOLOR ---

procedure win32'63 (hdc, x1, y1, x2, y2, brush)

  mov  ecx, y2
  push [ecx]
  mov  ebx, x2
  push [ebx]
  mov  ecx, y1
  push [ecx]
  mov  ebx, x1
  push [ebx]
  mov  edx, esp

  mov  eax, brush
  push [eax]
  push edx
  mov  eax, hdc
  push [eax]
  call 'dlls'user32.FillRect

  lea  esp, [esp+16]
  test eax, eax
  jz   short lEnd
  mov  eax, hdc

lEnd:
  ret

end

