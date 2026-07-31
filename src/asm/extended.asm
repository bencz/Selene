
// --- Extended Binary Package

// --- EXT_RANDOMIZE (DEST, SOUR) ---

procedure extended'1 (seed)
 
  sub  esp, 8h
  mov  eax, esp
  sub  esp, 10h
  lea  ebx, [esp]
  push eax 
  push ebx
  push ebx
  call 'dlls'kernel32.GetSystemTime
  call 'dlls'kernel32.SystemTimeToFileTime
  add  esp, 10h
  pop  ebx
  pop  edx
  mov  eax, seed
  mov  [eax], ebx
  ret

end

procedure extended'2 (retval, rnd, maxn)

   mov  eax, rnd
   mov  ebx, [eax+4] // NUM.RE
   mov  esi, [eax]   // NUM.FR
   mov  eax, ebx
   mov  ecx, 15Ah
   mov  ebx, 4E35h
   test eax, eax
   jz   short Lab1
   mul  ebx
Lab1: 
   xchg eax, ecx
   mul  esi
   add  eax, ecx
   xchg eax, esi
   mul  ebx
   add  edx, esi
   add  eax, 1
   adc  edx, 0
   mov  ebx, eax
   mov  esi, edx
   mov  ecx, rnd
   mov  [ecx+4], ebx
   mov  eax, esi
   and  eax, 7FFFFFFFh
   mov  [ecx] , esi
   cdq
   mov  ecx, maxn
   mov  ecx, [ecx]
   idiv ecx
   mov  eax, retval
   mov  [eax], edx
   ret

end

procedure extended'3 (date)

  mov  eax, date
  sub  esp, 10h
  lea  ebx, [esp]
  push eax 
  push ebx
  push ebx
  call 'dlls'kernel32.GetSystemTime
  call 'dlls'kernel32.SystemTimeToFileTime
  add  esp, 10h
  ret

end
