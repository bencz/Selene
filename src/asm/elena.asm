// --- Elena Binary Package ---

define GC_HEAP_ATTRIBUTE                0Dh
define elRoleVMT                        10h 

define GC_MG_COLLECT                    1
define GC_FULL_COLLECT                  2

// --- STD_ALLOC ---

procedure elena'alloc // eax - out, ebx - page size, ecx - total size

  // --- allocate ---
  
  mov  eax, ['gc_yg_heap]
  mov  esi, ecx
  add  esi, eax
  cmp  esi, ['gc_heap_end]
  ja   short labNotEnough
  mov  [eax], ebx
  mov  ['gc_yg_heap], esi
  lea  eax, [eax + 'el_emptyobject] 
  ret

labNotEnough:

  push edi
  mov  edx, ['gs_current_frame]
  mov  eax, edx
  sub  eax, esp
  mov  [edx], eax

  push ebx
  push ebp
  push ecx

  mov  eax, ['gc_flag]
  cmp  eax, GC_MG_COLLECT
  jz   short mgGC
  cmp  eax, GC_FULL_COLLECT
  jz   mgFull

ygGC:

  mov  ebp, ['gc_heap_start]
  lea  ebp, [ebp-4]
  mov  ebx, ['gc_mg_heap]
  mov  edx, ['gc_heap_end] 

  call @"$package'elena'collect"

  mov  ['gc_mg_heap], edi
  mov  ['gc_yg_heap], edi

  mov  eax, edx
  sub  eax, edi
  cmp  eax, 'gc_heap_minimal
  ja   short yg_GCNext
  mov  ['gc_flag], GC_MG_COLLECT
yg_GCNext:

  mov  ecx, [esp]
  cmp  ecx, eax
  ja   short mgGC 

  call fixupHeap

  mov  eax, ['gc_mgptr2]            // ; reset yg2mg table
  mov  ['gc_mgptr2_end], eax 

  pop  ecx
  pop  ebp
  pop  ebx

  mov  eax, ['gc_yg_heap]
  mov  esi, ecx
  add  esi, eax
  mov  [eax], ebx
  mov  ['gc_yg_heap], esi
  lea  eax, [eax + 'el_emptyobject]

  pop  edi
  ret

mgGC:

  mov  ebp, ['gc_heap_start]
  lea  ebp, [ebp-4]
  mov  ebx, ['gc_og_heap]
  mov  edx, ['gc_heap_end] 

  xor  eax, eax
  mov  ['gc_mgptr2], eax
  mov  ['gc_mgptr2_end], eax

  call @"$package'elena'collect"

  mov  ['gc_og_heap], edi
  mov  ['gc_mg_heap], edi
  mov  ['gc_yg_heap], edi

  mov  eax, edi                   // ; shift yg2mg table
  sub  eax, ebp
  and  eax, 'gc_page_mask
  shr  eax, ('gc_page_log - 2)
  neg  eax
  add  eax, ebp
  mov  ['gc_mgptr2], eax
  mov  ['gc_mgptr2_end], eax 

  mov  eax, ['gc_ogptr2]          // ; reset yg2og table
  mov  ['gc_ogptr2_end], eax 

  mov  eax, edx
  sub  eax, edi
  cmp  eax, 'gc_heap_minimal
  ja   short mg_GCNext
  mov  ['gc_flag], GC_FULL_COLLECT
mg_GCNext:

  mov  ecx, [esp]
  cmp  ecx, eax
  ja   short mgFull 

  call fixupHeap

  pop  ecx
  pop  ebp
  pop  ebx

  mov  eax, ['gc_yg_heap]
  mov  esi, ecx
  add  esi, eax
  mov  [eax], ebx
  mov  ['gc_yg_heap], esi
  lea  eax, [eax + 'el_emptyobject]

  pop  edi
  ret

mgFull:

  mov  ebp, ['gc_heap_start]
  mov  ebx, ebp
  lea  ebp, [ebp-4]
  mov  edx, ['gc_heap_end] 

  mov  eax, ['gc_ogptr2]
  mov  ['gc_ogptr2_end], eax

  xor  eax, eax
  mov  ['gc_mgptr2], eax
  mov  ['gc_mgptr2_end], eax

  call @"$package'elena'collect"

  mov  ['gc_og_heap], edi
  mov  ['gc_mg_heap], edi
  mov  ['gc_yg_heap], edi

  mov  eax, edi
  sub  eax, ebp
  and  eax, 'gc_page_mask
  shr  eax, ('gc_page_log - 2)
  neg  eax
  add  eax, ebp
  mov  ['gc_mgptr2], eax
  mov  ['gc_mgptr2_end], eax

  mov  ecx, [esp]

  mov  eax, edx
  sub  eax, edi
  cmp  edi, ecx
  jb   gc_Error

  call fixupHeap

  pop  ecx
  pop  ebp
  pop  ebx

  mov  eax, ['gc_yg_heap]
  mov  esi, ecx
  add  esi, eax
  mov  [eax], ebx
  mov  ['gc_yg_heap], esi
  lea  eax, [eax + 'el_emptyobject]

  pop  edi
  ret
  
gc_Error:

  lea  esp, [esp+10h]

  push 0
  push 0
  push 1
  push 190h
  call 'dlls'kernel32.RaiseException
  ret

// ; --- fixup pointers to yg(0) from mg(1)

fixupHeap:

  mov  esi, ['gc_mgptr2]
  mov  ecx, ['gc_mgptr2_end]
  cmp  ecx, esi
  jz   short fixFixupOG

  push ecx
  
fixMGFixup:
  mov  edi, [esi]
  mov  ecx, [edi-8]
  call fixup
  lea  esi, [esi-4]
  cmp  esi, [esp]
  jnz  short fixMGFixup

  lea  esp, [esp+4]

// ; --- fixup pointers to yg(0) from og(2)

fixFixupOG:

  mov  esi, ['gc_ogptr2]
  mov  ecx, ['gc_ogptr2_end]
  cmp  ecx, esi
  jz   short msFixupStack

  push ecx 
  
fixOGFixup:
  mov  edi, [esi]
  mov  ecx, [edi-8]
  call fixup
  lea  esi, [esi-4]
  cmp  esi, [esp]
  jnz  short fixOGFixup

  lea  esp, [esp+4]

msFixupStack:

// ; --- fixup stack roots ---

  mov  esi, ['gs_current_frame]

ygFixupStack:

  mov  ecx, [esi]
  mov  edi, esi
  sub  edi, ecx
  shr  ecx, 2
  call fixup
  mov  esi, [esi+4]
  test esi, esi
  jnz  short ygFixupStack

// ; --- fixup static symbols ---

  mov  edi, 'statroots
  mov  ecx, ['gc_static_size]            
  test ecx, ecx
  jz   short fixEnd
  nop
  call fixup
  
fixEnd:

  ret
  
// ; --- fixup --- (ecx - size, edi - object, ebx - minimal; edx - maximal)

fixup:

  push esi
  push 0

fixStart:
  mov  esi, [edi]
  cmp  esi, ebx
  jb   short fixNext
  cmp  esi, edx
  ja   short fixNext

  mov  eax, esi
  sub  eax, ebp
  and  eax, 'gc_page_mask
  shr  eax, ('gc_page_log - 2)
  neg  eax
  add  eax, ebp

  mov  eax, [eax]
  test eax, eax
  jz   fixSkipFixup
  add  esi, eax
  mov  [edi], esi
fixSkipFixup:

  mov  eax, [esi-8]
  test eax, 'gc_collected
  jz   short fixNext

  and  eax, 'gc_collectedInv
  mov  [esi-8], eax

  test eax, eax
  jle  short fixNext

  push edi
  push ecx
  mov  edi, esi
  mov  ecx, eax
  jmp  short fixStart

fixNext:
  lea  edi, [edi+4]
  sub  ecx, 1
  jnz  short fixStart

  pop  ecx
  pop  edi
  test ecx, ecx
  jnz  short fixNext
  mov  esi, edi

  ret

end

// --- mark and sweep ---

procedure elena'collect // (ebx - minimal, edx - maximal, ebp)

// ; --- collect pointers to yg(0) from mg(1)

  mov  esi, ['gc_mgptr2]
  mov  ecx, ['gc_mgptr2_end]
  cmp  ecx, esi
  jz   short msCollectFromOG

  push ecx
  
msMGCollect:
  mov  edi, [esi]
  mov  ecx, [edi-8]
  call collect
  lea  esi, [esi-4]
  cmp  esi, [esp]
  jnz  short msMGCollect
  lea  esp, [esp+4]

msCollectFromOG:

  mov  esi, ['gc_ogptr2]
  mov  ecx, ['gc_ogptr2_end]
  cmp  ecx, esi
  jz   short msCollectStack

  push ecx 
  
msOGCollect:
  mov  edi, [esi]
  mov  ecx, [edi-8]
  call collect
  lea  esi, [esi-4]
  cmp  esi, [esp]
  jnz  short msOGCollect
  lea  esp, [esp+4]

// ; --- collect stack roots ---

msCollectStack:
  mov  esi, ['gs_current_frame]

msCollectStackNext:
  mov  ecx, [esi]
  mov  edi, esi
  sub  edi, ecx
  shr  ecx, 2
  call collect 
  mov  esi, [esi+4]
  test esi, esi
  jnz  short msCollectStackNext

// ; --- collect static symbols ---

  mov  edi, 'statroots
  mov  ecx, ['gc_static_size]            
  test ecx, ecx
  jz   short msCompact
  nop
  call collect

msCompact:

// --- compact ---

  mov  esi, ebx
  mov  edi, ebx
check_heap_lab:
  cmp  esi, edx
  jae  short compact_end
  mov  eax, [esi]
  mov  ecx, eax
  shl  ecx, 2
  add  ecx, 'gc_empty_object_aligned
  and  ecx, 'gc_page_mask
  test eax, 'gc_collected
  jz   short skip_compact_lab2

  mov  eax, esi
  sub  eax, ebp
  and  eax, 'gc_page_mask
  shr  eax, ('gc_page_log - 2)
  neg  eax
  add  eax, ebp

  mov  [eax], edi
  sub  [eax], esi

  cmp  esi, edi
  jz   short skip_compact_lab
move_lab:
  mov  eax, [esi]
  lea  esi, [esi+4]
  mov  [edi], eax
  lea  edi, [edi+4]
  sub  ecx, 4
  jnz  short move_lab
  nop
  jmp  short check_heap_lab
skip_compact_lab:
  add  edi, ecx
skip_compact_lab2:
  add  esi, ecx
  jmp  short check_heap_lab
compact_end:
  nop
  ret

// ; --- collect --- (ecx - size, edi - object, ebx - minimal; edx - maximal)

collect:

  push esi
  push 0

colStart:
  mov  esi, [edi]
  cmp  esi, ebx
  jb   short colNext
  cmp  esi, edx
  ja   short colNext
  mov  eax, [esi-8]
  test eax, 'gc_collected
  jnz  short colNext

  or   [esi-8], 'gc_collected
  test eax, eax
  jle  short colNext

  push edi
  push ecx
  mov  edi, esi
  mov  ecx, eax
  jmp  short colStart

colNext:
  lea  edi, [edi+4]
  sub  ecx, 1
  jnz  short colStart

  pop  ecx
  pop  edi
  test ecx, ecx
  jnz  short colNext
  mov  esi, edi

  ret

end

// STD_ENTRY

procedure elena'startup

  // initialize
  mov  ecx, ['gc_static_size]
  mov  edi, 'statroots
  mov  eax, 'nil

clear:
  mov  [edi], eax     
  lea  edi, [edi+4]
  sub  ecx, 1
  jnz  short clear

  mov  ebx, 'gc_heapsize   // calculate total heap size
  mov  eax, ebx
  shl  eax, 'gc_page_log   
  shl  ebx, 2
  add  eax, ebx  

  push eax                 // create heap
  push GC_HEAP_ATTRIBUTE
  call 'dlls'kernel32.GetProcessHeap
  push eax 
  call 'dlls'kernel32.HeapAlloc

  mov  ebx, 'gc_heapsize   // calculate fixup table size
  shl  ebx, 2
  add  eax, ebx
  mov  ['gc_heap_start], eax
  mov  ['gc_yg_heap], eax 
  mov  ['gc_mg_heap], eax          
  mov  ['gc_og_heap], eax          

  lea  edx, [eax-4]
  mov  ['gc_mgptr2], edx    // reset list of inter generation references
  mov  ['gc_ogptr2], edx
  mov  ['gc_mgptr2_end], edx 
  mov  ['gc_ogptr2_end], edx

  mov  ebx, 'gc_heapsize
  shl  ebx, 'gc_page_log
  add  eax, ebx
  mov  ['gc_heap_end], eax 

  xor  ebx, ebx
  push ebx                      
  push ebx
  mov  ['gs_current_frame], esp // set stack frame pointer

  mov  edi, 1                 // start
  push edi
  call @'starter
  lea  esp, [esp+12]

  mov  eax, 0                         // exit code
  push eax
  call 'dlls'kernel32.ExitProcess     // exit

  ret

end

// prep
inline elena'prep

  push ebp
  mov  ebp, esp 

end

// sprep
inline elena'sprep

  push edi
  mov  edi, eax
  push ebp
  mov  ebp, esp

end

// return
inline elena'return
  pop  eax
  mov  esp, ebp
  pop  ebp
  mov  [esp+4], eax
  ret
end

// iocall(index)  (__arg1 contains message ID, __arg2 contains message index)
inline elena'iocalln

   pop  ebx
   mov  eax, [esp]             // ; get object ptr
   mov  edx, __arg1

 labStart:
   mov  eax, [eax-4]           // ; get vmt
   test eax, eax               
   jz   short labEnd           // ; go to the end if no VMT pointer
   lea  esi, [eax + __arg2]    // ; else set esi to the vmt hash table cell
   mov  ecx, [esi]
   test ecx, ecx
   jz   short labCall
 labNext:  
   cmp  ecx, edx             // ; compare with message id
   lea  esi, [esi+8]
   jg   short labStart
   mov  ecx, [esi]
   jl   short labNext

 labCall:
   mov  eax, [esp]             // ; load self pointer
   call [esi-4]

 labEnd:

end

// sexit
inline elena'sexit

  mov  esp, ebp
  pop  ebp
  pop  edi 
  mov  eax, edi
  ret
 
end

// sreturn
inline elena'sreturn
  pop  eax
  mov  esp, ebp
  pop  ebp
  pop  edi
  mov  [esp+4], eax
  ret
end

// rreturnif (__arg1obj - constant)
inline elena'rreturnif
  pop  eax
  cmp  eax, __arg1obj
  jz   short lbContinue
  mov  [esp+4], eax
  ret
lbContinue:
  push eax
end

// ocreate (ecx - totalsize, __arg1 - size, __arg2vmt - vmt)
inline elena'ocreate 
  mov  ebx, __arg1
  call @"$package'elena'alloc"

  mov  [eax-4], __arg2vmt
  mov  esi, eax
labClear:
  mov  [esi], 'nil
  add  esi, 4
  sub  ecx, 1
  jnz  short labClear
  push eax

end

// ocreate2 (ecx - totalsize, __arg1 - size, __arg2vmt - vmt)
inline elena'ocreate2
  mov  ebx, __arg1
  call @"$package'elena'alloc"
  mov  [eax-4], __arg2vmt
  mov  [eax], 'nil
  mov  [eax+4], 'nil
  push eax

end

// ocreate4 (ecx - totalsize, __arg1 - size, __arg2vmt - vmt)
inline elena'ocreate4
  mov  ebx, __arg1
  call @"$package'elena'alloc"
  mov  [eax-4], __arg2vmt
  mov  [eax], 'nil
  mov  [eax+4], 'nil
  mov  [eax+8], 'nil
  mov  [eax+0Ch], 'nil
  push eax

end

// ocreate6 (ecx - totalsize, __arg1 - size, __arg2vmt - vmt)
inline elena'ocreate6
  mov  ebx, __arg1
  call @"$package'elena'alloc"
  mov  [eax-4], __arg2vmt
  mov  [eax], 'nil
  mov  [eax+4], 'nil
  mov  [eax+8], 'nil
  mov  [eax+0Ch], 'nil
  mov  [eax+10h], 'nil
  mov  [eax+14h], 'nil
  push eax

end

// ocreate0 (ecx - totalsize, __arg1 - size, __arg2vmt - vmt)
inline elena'ocreate0
  mov  ebx, __arg1
  call @"$package'elena'alloc"

  mov  [eax-4], __arg2vmt
  push eax

end

// callext
inline elena'callext
  push edi
  mov  eax, ['gs_current_frame]
  push eax                              // save previous pointer 
  mov  ecx, eax
  sub  ecx, esp
  mov  [eax], ecx
  push ebp
  lea  ebp, [esp + 8]
  call __arg1fun
  pop  ebp
  pop  edx
  mov  ['gs_current_frame], edx
  pop  edi
end

// prepredir
inline elena'prepredir

  push edx
  push edi
  mov  edi, eax
  push ebp
  mov  ebp, esp

end

// exitredir
inline elena'exitredir

  mov  esp, ebp
  pop  ebp
  pop  edi 
  xor  eax, eax
  pop  edx
  ret

end

// redirect
inline elena'redirect

   mov  edx, [ebp+8]           // ; get message id
   mov  eax, [esp]             // ; get object ptr
   mov  ebx, [ebp-4]

 labStart:
   mov  eax, [eax-4]           // ; get vmt
   test eax, eax               
   jz   short labEnd           // ; go to the end if no VMT pointer
   lea  esi, [eax + 8]         // ; else set esi to the vmt hash table cell
   mov  ecx, [esi]
   test ecx, ecx
   jz   short labCall
 labNext:  
   cmp  ecx, edx               // ; compare with message id
   lea  esi, [esi+8]
   jg   short labStart
   mov  ecx, [esi]
   jl   short labNext

 labCall:
   pop  eax                    // ; load self pointer
   call [esi-4]

   test eax, eax               
   jnz  short labExit          // ; if method result is successful exits the procedure

   mov  ebx, [esp]         
   test ebx, ebx
   jnz  short labEnd           // ; if method result is unsuccessful and stack 
                               // ; contains 0 - exits the procedure
 labExit:
   pop  eax
   mov  esp, ebp
   pop  ebp
   pop  edi 
   pop  edx
   mov  [esp+4], eax
   ret
   
 labEnd:  

end

// rredirect
inline elena'rredirect

   mov  edx, [ebp+8]           // ; get message id
   mov  eax, __arg1vmt         // ; get parent vmt
   mov  ebx, [ebp-4]

 labStart:
   mov  eax, [eax-4]           // ; get vmt
   test eax, eax               
   jz   short labEnd           // ; go to the end if no VMT pointer
   lea  esi, [eax + 8]         // ; else set esi to the vmt hash table cell
   mov  ecx, [esi]
   test ecx, ecx
   jz   short labCall
 labNext:  
   cmp  ecx, edx             // ; compare with message id
   lea  esi, [esi+8]
   jg   short labStart
   mov  ecx, [esi]
   jl   short labNext

 labCall:
   mov  eax, [esp]             // ; load self pointer
   call [esi-4]

   test eax, eax               
   jnz  short labExit          // ; if method result is successful exits the procedure

   mov  ebx, [esp]         
   test ebx, ebx
   jnz  short labEnd           // ; if method result is unsuccessful and stack 
                               // ; contains 0 - exits the procedure
 labExit:
   pop  eax
   mov  esp, ebp
   pop  ebp
   pop  edi 
   pop  edx
   mov  [esp+4], eax
   ret
   
 labEnd:  

end

// group
procedure elena'group

   push edi
   push 0                     // index local 
   mov  edi, eax
   push edx
   push ebx

   mov  eax, [esp+14h]
   push eax                    // load VSELF

   xor  ebx, ebx   

 labScan:
   mov  eax, [edi+ebx]

 labStart:
   mov  eax, [eax-4]           // ; get vmt
   test eax, eax               
   jz   short labEnd           // ; go to the end if no VMT pointer
   lea  esi, [eax + 8]         // ; else set esi to the vmt hash table cell
   mov  ecx, [esi]
   test ecx, ecx
   jz   short labCall
 labNext:  
   cmp  ecx, edx              // ; compare with message id
   lea  esi, [esi+8]
   jg   short labStart
   mov  ecx, [esi]
   jl   short labNext

 labCall:
   mov  ecx, ebx
   mov  eax, [edi+ebx]        // ; load self pointer
   mov  [esp+0Ch], ecx        // ; save index

   mov  ebx, [esp+4]          // ; get parameter

   call [esi-4]

   test eax, eax               
   jnz  short labExit          // ; if method result is successful exits the procedure

   mov  ebx, [esp]         
   test ebx, ebx
   jnz  short labCallEnd       // ; if method result is unsuccessful and stack 
                               // ; contains 0 - exits the procedure
 labExit:          
   pop  eax
   lea  esp, [esp+0Ch]
   pop  edi 
   mov  [esp+4], eax
   ret

 labCallEnd:
   mov  edx, [esp+8]           // ; restore message id 
   mov  ebx, [esp+0Ch]         // ; restore index

 labEnd:
   mov  ecx, [edi-8]
   lea  ebx, [ebx+4]
   shl  ecx, 2
   cmp  ebx, ecx
   jl   short labScan

   lea  esp, [esp+10h]
   pop  edi
   xor  eax, eax               // ; put zero to indicate nagative result
   ret

end

// IS_SAME

inline elena'identical    

  pop eax 
  mov ebx, [esp]
  cmp eax, ebx
  jz  short lab1
  xor eax, eax
lab1:

end

// CLASS_REDIRECT

inline elena'vmtof

   pop   ebx
   mov   eax, [esp]
   lea   eax, [eax+4]          // ; to conpencate the offset in the next command

 labStart:
   mov  eax, [eax-4]           // ; get vmt
   test eax, eax               
   jz   short labEnd           // ; go to the end if no VMT pointer
   lea  esi, [eax + 8]         // ; else set esi to the vmt hash table cell
   mov  ecx, [esi]
   test ecx, ecx
   jz   short labCall
 labNext:  
   cmp  ecx, edx             // ; compare with message id
   lea  esi, [esi+8]
   jg   short labStart
   mov  ecx, [esi]
   jl   short labNext

 labCall:
   mov  eax, [esp]             // ; load self pointer
   call [esi-4]

 labEnd:

end

// IS_TYPESAME

inline elena'sametype

  pop eax
  mov edx, [eax] 
  mov ebx, [esp]

  cmp edx, [ebx-4]
  jz  short lab1
  xor eax, eax
lab1:

end

// ioswap

inline elena'ioswap

  mov ebx, [esp]
  mov ecx, [esp-__arg1]
  mov [esp], ebx
  mov [esp-__arg1], ecx

end

// ioset

inline elena'ioset

  mov ebx, [esp]
  mov ecx, __arg2vmt
  mov [ebx+__arg1], ecx

end

// shift

inline elena'shift

  mov edx, [edi-4]
  test [edx-8], elRoleVMT  // ; skip if it is not a role
  jz   short labShift
  mov edx, [edx-4]         // ; get a role owner vmt

labShift:
  mov edx, [edx-0Ch]
  mov ecx, [edx+__arg1]
  mov [edi-4], ecx

end

// unshift

inline elena'unshift 

  mov edx, [edi-4]
  mov edx, [edx-4]
  mov [edi-4], edx

end

// ircall(index)  (eax contains vmt, __arg1 contains message ID, __arg2 contains message index)
inline elena'ircall

   pop  ebx
   mov  edx, __arg1
   jmp  short labStart2

 labStart:
   mov  eax, [eax-4]           // ; get vmt
   test eax, eax               
   jz   short labEnd           // ; go to the end if no VMT pointer
 labStart2:
   lea  esi, [eax + __arg2]    // ; else set esi to the vmt hash table cell
   mov  ecx, [esi]
   test ecx, ecx
   jz   short labCall
 labNext:  
   cmp  ecx, edx             // ; compare with message id
   lea  esi, [esi+8]
   jg   short labStart
   mov  ecx, [esi]
   jl   short labNext

 labCall:
   mov  eax, [esp]             // ; load self pointer
   call [esi-4]

 labEnd:

end

// STD_ASSIGN

procedure elena'barrier                  // eax - object, esi - destination, edi - destination object

  mov  ecx, fs:[4]                  // stack top
  mov  edx, fs:[8]                  // stack low / current
  cmp  eax, ecx
  ja   short labSkip
  cmp  eax, edx
  jb   short labSkip 
  push esi
  call @"$package'elena'alloctemp"        // alloctemp
  pop  esi
labSkip:
  mov  [esi], eax
  cmp  edi, ['gc_mg_heap]
  ja   short labSkip2
  nop
  call @"$package'elena'remember"

labSkip2:  
  ret
end

// STD_ALLOCTEMP

procedure elena'alloctemp // eax - object

  push eax
  mov  ebx, [eax-'el_emptyobject]      // define size
  mov  ecx, 'gc_empty_object_aligned
  mov  edx, ebx
  shl  edx, 2
  add  ecx, edx
  and  ecx, 'gc_page_mask
  call @"$package'elena'alloc"             // allocate
  pop  esi
  mov  ecx, [eax-'el_emptyobject]
  and  ecx, 7FFFFFFFh

  mov  edx, [esi-4]                   // copy vmt
  mov  [eax-4], edx
  mov  edx, eax
lCopy:                                // copy body
  mov  ebx, [esi]
  mov  [edx], ebx
  lea  edx, [edx+4]
  lea  esi, [esi+4]
  sub  ecx, 1
  jnz  short lCopy

  ret
end

// STD_ADDYGPTR

procedure elena'remember // edi, eax

  cmp  edi, eax                    // skip if the referring object older then self
  jae  short labEnd

  mov  ecx, fs:[4]                 // skip if the object is local  
  cmp  edi, ecx
  ja   short labStart 
  cmp  edi, esp
  jnb  short labEnd

labStart:
  cmp  edi, ['gc_og_heap]
  jb   short labOG

labMG:

  cmp  eax, ['gc_mg_heap]
  jb   short labEnd

  mov  edx, ['gc_mgptr2_end]
  mov  esi, ['gc_mgptr2]
labMGCheck:
  cmp  esi, edx
  jz   short labMGAdd
  cmp  edi, [esi]
  lea  esi, [esi-4]
  jnz  short labMGCheck
  ret
labMGAdd:
  mov  [esi], edi
  lea  esi, [esi-4]
  mov  ['gc_mgptr2_end], esi
  ret

labOG:

  mov  edx, ['gc_ogptr2_end]
  mov  esi, ['gc_ogptr2]

labOGCheck:
  cmp  esi, edx
  jz   short labOGAdd
  cmp  edi, [esi]
  lea  esi, [esi-4]
  jnz  short labOGCheck
  ret
labOGAdd:
  mov  [esi], edi
  lea  esi, [esi-4]
  mov  ['gc_ogptr2_end], esi
  ret

labEnd:
  ret

end

// eax - object, esi - destination, edi - destination object

inline elena'assign                  

  mov  ecx, fs:[4]                  // stack top
  mov  edx, fs:[8]                  // stack low / current
  cmp  eax, ecx
  ja   short labSkip
  cmp  eax, edx
  jb   short labSkip 
  call @"$package'elena'alloctemp"        // alloctemp
labSkip:
  cmp  edi, ['gc_mg_heap]
  ja   short labSkip2
  nop
  call @"$package'elena'remember"

labSkip2:  

end

// STD_WIN32ENTRY

procedure elena'startupgui

  // initialize
  mov  ecx, ['gc_static_size]
  mov  edi, 'statroots
  mov  eax, 'nil

clear:
  mov  [edi], eax
  lea  edi, [edi+4]
  sub  ecx, 1
  jnz  short clear

  mov  ebx, 'gc_heapsize   // calculate total heap size
  mov  eax, ebx
  shl  eax, 'gc_page_log   
  shl  ebx, 2
  add  eax, ebx  

  push eax
  push GC_HEAP_ATTRIBUTE
  call 'dlls'kernel32.GetProcessHeap
  push eax 
  call 'dlls'kernel32.HeapAlloc

  mov  ebx, 'gc_heapsize   // calculate fixup table size
  shl  ebx, 2
  add  eax, ebx

  mov  ['gc_heap_start], eax
  mov  ['gc_yg_heap], eax 
  mov  ['gc_mg_heap], eax          
  mov  ['gc_og_heap], eax

  lea  edx, [eax-4]
  mov  ['gc_mgptr2], edx    // reset list of inter generation references
  mov  ['gc_ogptr2], edx
  mov  ['gc_mgptr2_end], edx 
  mov  ['gc_ogptr2_end], edx

  mov  ebx, 'gc_heapsize
  shl  ebx, 'gc_page_log
  add  eax, ebx
  mov  ['gc_heap_end], eax          // @gc_heap_end

  xor  ebx, ebx
  push ebx                      
  push ebx
  mov  ['gs_current_frame], esp     // set stack frame pointer

  push 0
  call @win32'api'instance          // init instance
  push 'nil
  call @std'basic'intnumber
  pop  ebx
  mov  eax, [esp]
  mov  edx, [ebp+8]
  mov  [ebx], edx
 
  mov  edx, #win32'api'$sethandle
lab_start:
  mov  eax, [eax-4]
  test eax, eax
  jz   short lab_end
  mov  esi, eax 
lab_next:
  mov  ecx, [esi]
  test ecx, ecx
  lea  esi, [esi+8]
  jz   short lab_start
  cmp  ecx, edx 
  jnz  short lab_next
  mov  eax, [esp]
  call [esi-4]
lab_end:
  pop  eax

  mov  edi, 1                         // start
  push edi
  call @'starter
  lea  esp, [esp+12]

  mov  eax, 0                         // exit code
  push eax
  call 'dlls'kernel32.ExitProcess     // exit

  ret                                 // in case the ExitProcess fails

end

// cast
procedure elena'cast

   push edi
   push 0                     // index local 
   mov  edi, eax
   push edx
   push ebx

   mov  eax, [esp+14h]
   push eax                    // load VSELF

   xor  ebx, ebx   

 labScan:
   mov  eax, [edi+ebx]

 labStart:
   mov  eax, [eax-4]           // ; get vmt
   test eax, eax               
   jz   short labEnd           // ; go to the end if no VMT pointer
   lea  esi, [eax + 8]         // ; else set esi to the vmt hash table cell
   mov  ecx, [esi]
   test ecx, ecx
   jz   short labCall
 labNext:  
   cmp  ecx, edx              // ; compare with message id
   lea  esi, [esi+8]
   jg   short labStart
   mov  ecx, [esi]
   jl   short labNext

 labCall:
   mov  ecx, ebx
   mov  eax, [edi+ebx]        // ; load self pointer
   mov  [esp+0Ch], ecx        // ; save index

   mov  ebx, [esp+4]          // ; get parameter

   call [esi-4]

   mov  edx, [esp+8]           // ; restore message id 
   mov  ebx, [esp+0Ch]         // ; restore index

 labEnd:
   mov  ecx, [edi-8]
   lea  ebx, [ebx+4]
   shl  ecx, 2
   cmp  ebx, ecx
   jl   short labScan

   lea  esp, [esp+10h]
   pop  edi
   mov  eax, edi
   ret

end

// STD_WINDRPOC

procedure elena'wndproc

  push ebp
  mov  ebp, esp
   
  xor  edi, edi

  mov  eax, ['gs_current_frame]
  push eax                              // save previous pointer 
  push edi                              // size field
  mov  ['gs_current_frame], esp

  push 0FFFFFFEBh  
  push [ebp+08h]
  call 'dlls'user32.GetWindowLongW
  and  eax, eax
  jnz  short eventCheck
  cmp  [ebp+0Ch], 1
  jnz  default
  mov  ebx, [ebp+14h]
  mov  eax, [ebx]

  push eax
  push 0FFFFFFEBh  
  push [ebp+8]
  call 'dlls'user32.SetWindowLongW
  jmp  default

eventCheck:

  mov  ebx, ['system]
  mov  ebx, [ebx]
  lea  ebx, [ebx+eax]

  mov  ecx, [ebp+0Ch]   // get msg

  // find message id mapped to msg
  mov  edx, #win32'api'$onuserevent
  cmp  ecx, 400h
  jae  short lSend

  mov esi, 'structure:"$package'elena'wmtable"
lNext:
  cmp ecx, [esi]
  lea esi, [esi+8]
  ja  short lNext   
  mov edx, [esi-4]
  jnz short default
 
lSend:
  mov  ecx, [ebp+14h]   // lparam
  push ecx
  mov  ecx, [ebp+10h]   // wparam
  push ecx
  mov  ecx, [ebp+0Ch]   // msg
  push ecx
  mov  eax, esp
  push "win32'api'message"
  push 80000003h

  push [ebx]           // push handler to the stack
  
  mov  ebx, eax
  mov  eax, [esp]

lab_start4:
  mov  eax, [eax-4]
  test eax, eax
  jz   short default3
  lea  esi, [eax + 8]
  mov  ecx, [esi]
  test ecx, ecx
  jz   short labCall 

lab_next4:
  cmp  ecx, edx             // ; compare with message id
  lea  esi, [esi+8]
  jg   short lab_start4
  mov  ecx, [esi]
  jl   short lab_next4

labCall:
  mov  eax, [esp]
  call [esi-4]
  lea  esp, [esp+24]
  test eax, eax
  jz   short default
  mov  eax, [eax]
  jmp  short lab_end

default3:
  pop  ebx
  lea  esp, [esp+20]
   
default:

  lea  esp, [esp+4]
  pop  eax
  mov  ['gs_current_frame], eax

  push [ebp+14h]
  push [ebp+10h]
  push [ebp+0Ch]
  push [ebp+08h]
  call 'dlls'user32.DefWindowProcW
  pop  ebp
  ret

lab_end:
  lea  esp, [esp+4]
  pop  edx
  mov  ['gs_current_frame], edx

  pop  ebp
  ret

end

structure elena'wmtable

 dd 00002h
 dd #win32'api'$ondestroy
 dd 00007h
 dd #win32'api'$onsetfocus
 dd 0000Fh
 dd #win32'api'$onpaint
 dd 00010h
 dd #win32'api'$onclose
 dd 00020h
 dd #win32'api'$onsetcursor
 dd 00100h 
 dd #win32'api'$onkeydown
 dd 00102h
 dd #win32'api'$onchar
 dd 00111h
 dd #win32'api'$oncommand
 dd 00135h
 dd #win32'api'$onsetcolorbutton
 dd 00138h
 dd #win32'api'$onsetcolorstatic
 dd 00201h
 dd #win32'api'$onlbutton

 dd 0FFFFh
 dd 0  

end
