// --- WinSock Binary Package --

define AF_INET     2

define SOCK_STREAM 1

define SOMAXCONN   7fffffffh


// --- WS_ASYNCSELECT ---

procedure winsock'1 (handle, hwnd, msg, events)

  mov  ebx, events
  push [ebx]                // flags
  mov  ebx, msg
  push [ebx]
  mov  ebx, hwnd
  push [ebx]
  mov  ebx, handle
  push [ebx]
   
  call 'dlls'Ws2_32.WSAAsyncSelect
  test eax, eax
  jnz  short lErr

  mov  eax, handle
  ret

lErr:
  xor  eax, eax
  ret

end

// --- WS_CLEANUP ---

procedure winsock'2 (obj)

  call 'dlls'Ws2_32.WSACleanup
  mov  eax, obj
  ret 
 
end

// --- WS_STARTUP ---

procedure winsock'3 (obj, version)

  lea  esp, [esp-400]
  mov  edx, esp
  mov  ebx, version 
  push edx
  push [ebx]

  call 'dlls'Ws2_32.WSAStartup

  lea  esp, [esp+400] 
  mov  eax, obj
  ret

end

// --- WS_CLOSE ---

procedure winsock'4 (socket)

  mov  eax, socket
  push [eax]
  call 'dlls'Ws2_32.closesocket
  mov  eax, socket
  ret
 
end

// --- WS_LISTEN ---

procedure winsock'5 (handle)

  mov  eax, handle
  push SOMAXCONN
  push [eax]
  call 'dlls'Ws2_32.listen // listen(s, SOMAXCONN)
  test eax, eax
  jnz  short lErr
  mov  eax, handle
  ret
lErr:
  xor  eax, eax
  ret

end

// --- WS_RECV2 ---

procedure winsock'6 (socket, buffer)

  mov  edx, buffer
  mov  ecx, [edx-8]
  shl  ecx, 2
  mov  ebx, [edx]
  sub  ecx, ebx 

  lea  edx, [edx+4]
  add  edx, ebx
              
  push 0
  push ecx
  push edx 
  mov  eax, socket
  push [eax]
  call 'dlls'Ws2_32.recv
  cmp  eax, 0FFFFFFFFh
  jz   short lErr
  mov  edx, buffer
  add  [edx], eax

  mov  eax, socket
  ret

lErr:
  xor  eax, eax
  ret
 
end

// --- WS_SENDINT32 ---

procedure winsock'7 (socket, buffer)

  mov  edx, buffer

  push 0             // flag
  mov  ecx, 4
  push ecx           // length
  push edx           // buffer
  mov  eax, socket
  push [eax]         // socket
  call 'dlls'Ws2_32.send
  cmp  eax, 0FFFFFFFFh
  jz  short lErr
  mov  eax, socket
  ret

lErr:
  xor  eax, eax
  ret
 
end

// --- WS_SENDSTR ---

procedure winsock'8 (socket, buffer)

  mov  edx, buffer

  push 0             // flag
  mov  ecx, [edx]
  shl  ecx, 1
  push ecx           // length
  lea  edx, [edx+4]
  push edx           // buffer
  mov  eax, socket
  push [eax]         // socket
  call 'dlls'Ws2_32.send
  cmp  eax, 0FFFFFFFFh
  jz  short lErr
  mov  eax, socket
  ret

lErr:
  xor  eax, eax
  ret
 
end

// --- WS_SEND2 ---

procedure winsock'9 (socket, buffer)

  mov  edx, buffer

  push 0             // flag
  mov  ecx, [edx]  
  push ecx           // length
  lea  edx, [edx+4]
  push edx           // buffer
  mov  eax, socket
  push [eax]         // socket
  call 'dlls'Ws2_32.send
  cmp  eax, 0FFFFFFFFh
  jz  short lErr
  mov  eax, socket
  ret

lErr:
  xor  eax, eax
  ret
 
end

// --- WS_CREATELISTENER ---

procedure winsock'10 (handle, nport)

  lea  esp, [esp-100h]
  mov  edx, esp
  push 0FFh
  push edx
  call 'dlls'Ws2_32.gethostname

  mov  edx, esp
  push edx
  call 'dlls'Ws2_32.gethostbyname
  
  mov  ecx, [eax+0Ch]
  mov  ecx, [ecx]
  mov  eax, [ecx]
  push eax                     // save ipaddress

  push 0
  push SOCK_STREAM
  push AF_INET
  call 'dlls'Ws2_32.socket     // socket(AF_INET, SOCK_STREAM, 0)

  pop  ecx                     // restor ipaddress

  mov  ebx, handle             // save socket
  mov  [ebx], eax
  test eax, eax                // check if not zero
  jz   short labEnd

  push 0                       // stockaddr_in.sin_zero 
  push 0  
  push ecx                     // .s_addr
  mov  ebx, nport
  push word ptr [ebx]          // .sin_port
  push word ptr AF_INET        // .sin_family

  mov  edx, esp                // stockaddr_in
  push 16                   
  push edx
  push eax

  call 'dlls'Ws2_32.bind   // bind (sd, stockaddr_in, sizeof(stockaddr_in))      

  lea  esp, [esp+16]       // release stockaddr_in
  test eax, eax
  jnz  short lErr

  mov  eax, handle
labEnd:
  lea  esp, [esp+100h]
  ret
lErr:
  lea  esp, [esp+100h]
  xor  eax, eax
  ret
end


// --- WS_ACCEPT ---

procedure winsock'11 (newsocket, handle)

  push 0
  push 0
  mov  eax, handle
  push [eax]
  call 'dlls'Ws2_32.accept
  test eax, eax
  jz   short labEnd

  mov  ebx, newsocket
  mov  [ebx], eax
  mov  eax, ebx  

labEnd:
  ret
end


// --- WS_CREATE ---

procedure winsock'12 (handle)

  push 0
  push SOCK_STREAM
  push AF_INET
  call 'dlls'Ws2_32.socket // socket(AF_INET, SOCK_STREAM, 0)
  test eax, eax
  cmp  eax, 0FFFFFFFFh
  jz   short lErr

  mov  ebx, eax
  mov  eax, handle
  mov  [eax] , ebx
  ret

lErr:
  xor  eax, eax
  ret
  
end


// --- WS_CONNECT ---

procedure winsock'13 (handle, ipaddress, nport)

  mov  ebx, ipaddress 
  mov  ebx, [ebx]
  mov  ecx, [ebx-4]
  cmp  ecx, 20
  jb   short lStart
  mov  ecx, 20 
lStart:
  lea  esp, [esp-22]
  mov  edx, esp
  mov  esi, edx
lCopy:
  mov  eax, [ebx]
  mov  byte ptr [esi], al
  lea  ebx, [ebx+2]
  add  esi, 1
  sub  ecx, 1
  jnz  short lCopy
  xor  eax, eax
  mov  byte ptr [esi], al
  
  push edx                
  call 'dlls'Ws2_32.inet_addr  // inet_addr(localIP);  
  lea  esp, [esp+22]
	
  push 0                   // stockaddr_in.sin_zero 
  push 0  
  push eax                 // .s_addr
  mov  ebx, nport         
  push word ptr [ebx]      // .sin_port
  push word ptr AF_INET    // .sin_family
  mov  edx, esp            // stockaddr_in

  push 16                   
  push edx
  mov  eax, handle
  push [eax]

  call 'dlls'Ws2_32.connect // bind (sd, stockaddr_in, sizeof(stockaddr_in))      
  lea  esp, [esp+16]       // release stockaddr_in

  test eax, eax
  jnz  short lErr

  mov  eax, handle
  ret

lErr:
  xor  eax, eax
  ret

end

