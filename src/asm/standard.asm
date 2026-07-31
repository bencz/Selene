// --- Standard Binary Package

// --- WSTR_EQUAL (S1, S2) ---

inline standard'strequal

   pop  eax
   mov  esi, eax              // s2
   mov  edx, [esp]            // s1
   mov  ecx, [edx]            // s1.length
   cmp  ecx, [esi]            // compare with s2.length
   jnz  short Lab1
   add  edx, 4
   add  esi, 4
   test ecx, ecx
   jz   short Lab3
Lab2:
   mov  ebx, [esi]
   cmp  bx,  word ptr [edx]
   jnz  short Lab1
   lea  esi, [esi+2]
   lea  edx, [edx+2]
   sub  ecx, 1
   jnz  short Lab2
   nop
   jmp  short Lab3
Lab1:
   xor  eax, eax
Lab3:

end

// --- WSTR_LESS (S1, S2) ---

inline standard'strless

   pop  eax
   mov  esi, eax                // s2
   mov  edx, [esp]              // s1
   mov  ecx, [edx]              // s1 length

   cmp  ecx, [esi]
   jbe  short Lab3
   mov  ecx, [esi]
Lab3:
   test ecx, ecx
   jz   Lab4
   add  edx, 4
   add  esi, 4
Lab2:
   mov  ebx, [edx]              // s1[i] 
   cmp  bx, word ptr [esi]      // compare s2[i] with 
   jb   short LabEnd
   ja   short Lab1
   lea  esi, [esi+2]
   lea  edx, [edx+2]
   sub  ecx, 1
   jnz  short Lab2
Lab4:
   mov  ecx, [eax]              // s2 length    
   mov  edx, [esp]     
   mov  edx, [edx]              // s1 length
   cmp  edx, ecx
   jb   short LabEnd
Lab1:
   xor  eax, eax
LabEnd:

end

// --- INT_COPY (DEST, SOUR) ---

inline standard'loadint

  pop eax 
  mov edx, [eax] 
  mov eax, [esp]
  mov [eax], edx

end

// --- WSTR_CPY (DEST, SOUR) ---

inline standard'strcopy

  pop  eax
  mov  esi, eax                 // sour 
  mov  eax, [esp]               
  mov  edx, eax                 // dest
  mov  ecx, [esi]               // get sour length
  test ecx, ecx
  jz   short Lab2
  add  ecx, 4                   // to include tailing zero and size field
  shr  ecx, 1
Lab1:
  mov  ebx, [esi]
  mov  [edx], ebx
  add  esi, 4
  add  edx, 4
  sub  ecx, 1
  jnz  short Lab1
Lab2:

end

// --- WSTR_ADD (DEST, SOUR) ---

inline standard'strconcat

  pop  eax
  mov  esi, eax                // sour 
  mov  eax, [esp]              // dest
  lea  edx, [eax+4]
  add  edx, [eax]
  add  edx, [eax]
  mov  ecx, [esi]
  add  esi, 4
  test ecx, ecx
  jz   short labEnd
  add  [eax], ecx
  add  ecx, 2
  shr  ecx, 1

labNext:
  mov  ebx, [esi]
  mov  [edx], ebx
  add  esi, 4
  add  edx, 4
  sub  ecx, 1
  jnz  short labNext
labEnd:

end

// --- INT_EQUAL (N1, N2) ---

inline standard'i32equal

  pop eax
  mov edx, [eax]
  mov eax, [esp]
  cmp [eax], edx
  jz  short Lab1
  xor eax, eax
Lab1:

end

// --- INT_LESS (N1, N2) ---

inline standard'i32less

   pop  eax
   mov  edx, [eax]
   mov  eax, [esp]
   cmp  [eax], edx
   jl   short Lab1
   xor  eax, eax
Lab1:

end

// --- INT_ADD (DEST, SOUR) ---
	
inline standard'i32add

  pop eax
  mov ebx, [eax]
  mov eax, [esp]
  add [eax], ebx

end

// --- INT_SUB (DEST, SOUR) ---

inline standard'i32sub

  pop eax
  mov ebx, [eax]
  mov eax, [esp] 
  sub [eax], ebx

end

// --- INT_MUL (DEST, SOUR) ---

inline standard'i32mul

  pop eax
  mov edx, [eax]
  mov eax, [esp]
  mov eax, [eax]
  mul edx
  mov ebx, eax
  mov eax, [esp]
  mov [eax], ebx

end

// --- INT_DIV (DEST, SOUR) ---

inline standard'i32div

  pop  eax
  mov  ebx, [eax]
  mov  eax, [esp]
  mov  eax, [eax]
  cdq
  idiv ebx
  mov  ebx, eax
  mov  eax, [esp]
  mov  [eax], ebx           // copy to field

end

// --- INT_BAND (DEST, SOUR) ---

inline standard'i32and

  pop  eax
  mov ebx, [eax]
  mov eax, [esp]
  and [eax], ebx

end

// --- INT_BOR (DEST, SOUR) ---

inline standard'i32or

  pop  eax
  mov ebx, [eax]
  mov eax, [esp]
  or  [eax], ebx

end

// --- INT_BXOR (DEST, SOUR) ---

inline standard'i32xor

  pop  eax
  mov ebx, [eax]
  mov eax, [esp]
  xor [eax], ebx

end


// --- INT_SHIFT (DEST, OFFSET) ---

inline standard'i32shift

  pop  eax
  mov ecx, [eax]
  mov eax, [esp]
  mov edx, [eax]
  and ecx, ecx
  jns short lab1
  neg ecx
  shl edx, cl
  jmp short lab2
lab1:
  shr edx, cl
lab2:
  mov [eax], edx

end

// --- INT_TEST (N1, N2) ---

inline standard'i32anymask

  pop  eax
  mov  edx, [eax]
  mov  eax, [esp]
  test [eax], edx
  jnz  short Lab1
  xor  eax, eax
Lab1:

end

// --- INT_TEST2 (N1, N2) ---

inline standard'i32allmask

  pop  eax
  mov  edx, [eax]      
  mov  eax, [esp]
  mov  ecx, [eax]
  and  ecx, edx      
  cmp  ecx, edx
  jz   short Lab1
  xor  eax, eax
Lab1:

end

// --- INT_NOT ---

procedure standard'i32not

  pop eax
  mov edx, [eax]      
  neg  edx 
  mov eax, [esp]
  mov [eax], edx

end

// --- WCHAR_CPYPTR (CH, PTR, OFS) ---

inline standard'stridxnew

  pop  eax
  pop  edx

  mov  ecx, [edx] 
  lea  edx, [edx+4]
  mov  ebx, [eax]
  xor  eax, eax

  test ebx, ebx
  js   short lEnd

  cmp  ebx, ecx
  jge  short lEnd

  shl  ebx, 1
  add  edx, ebx
  mov  eax, [esp]
  xor  ecx, ecx
  mov  cx,  word ptr [edx]
  mov  [eax], ecx

lEnd:

end


// --- WSTR_INDEXOF (RETVAL, S, OFS, SUBS) ---

inline standard'stridxseek

  pop  eax 
  mov  edx, [esp+4]
  mov  ecx, [esp]

  push edi

  lea  edi, [edx+4]
  push edi

  lea  esi, [eax+4]
  push esi

  mov  ebx, [edi-4]   // get total length  
  mov  edx, [ecx]
  sub  ebx, edx
  jbe  short labEnd

  add  ebx, 1
  sub  edx, 1

labNext:
  add  edx, 1
  sub  ebx, 1
  jz   short labEnd
  mov  esi, [esp]
  mov  ecx, [esi-4]
  cmp  ebx, ecx
  jb   short labEnd
  mov  edi, [esp+4]
  add  edi, edx
  add  edi, edx

labCheck:
  mov  ax, word ptr [edi]
  cmp  ax, word ptr [esi]
  jnz  short labNext
  lea  edi, [edi+2]
  lea  esi, [esi+2]
  sub  ecx, 1
  jnz  short labCheck
  add  esp, 8
  pop  edi
  lea  esp, [esp+8]
  mov  eax, [esp]
  mov  [eax], edx
  jmp  short labEnd2

labEnd:
  add  esp, 8
  pop  edi
  lea  esp, [esp+8]
  mov  eax, [esp]
  mov  [eax], -1
labEnd2:

end

// --- WSTR_INSERT (RETVAL, S, INDEX, SUBS) ---

inline standard'stridxinsert

  pop  eax
  pop  ecx
  pop  esi
  push edi

  mov  edi, [esp+4]    // retval
  lea  edi, [edi+4]

  mov  ecx, [ecx]      // index

  mov  edx, [esi]      // s
  lea  esi, [esi+4]
  
  cmp  edx, ecx
  jae  short lab1
  mov  ecx, edx
lab1:
  sub  edx, ecx

  test ecx, ecx
  jz   short lab2
labCopy:
  mov  ebx, [esi]
  mov  word ptr [edi], bx
  lea  esi, [esi+2]
  lea  edi, [edi+2]
  sub  ecx, 1
  jnz  short labCopy

lab2:
  mov  ecx, [eax]   // asubs
  add  eax, 4
  test ecx, ecx
  jz   short lab3

labCopy2:
  mov  ebx, [eax]
  mov  word ptr [edi], bx
  lea  eax, [eax+2]
  lea  edi, [edi+2]
  sub  ecx, 1
  jnz  short labCopy2

lab3:
  test edx, edx
  jz   short lab4
  add  edx, 1             // to include trailing zero
  
labCopy3:
  mov  eax, [esi]
  mov  word ptr [edi], ax
  lea  esi, [esi+2]
  lea  edi, [edi+2]
  sub  edx, 1
  jnz  short labCopy3

lab4:
  pop  edi
  mov  eax, [esp]

end
  
// --- WSTR_ERASE (RETVAL, S, INDEX, LENGTH) ---

inline standard'stridxdelete

  pop  eax              // length
  pop  ebx              // index
  pop  edx              // s
  pop  ecx              // retval

  push edi

  lea  edi, [ecx+4]     // retval

  mov  ecx, [ebx]       // get index
  cmp  ecx, [edx]       // check if index withing string range
  jbe  short lab1
  mov  ecx, [edx]       // set to the string end
lab1:
  mov  eax, [eax]    // length to delete
  mov  ebx, eax
  add  ebx, ecx      // check if deleting substring within string range
  cmp  ebx, [edx]
  jbe  short lab2
  mov  eax, [edx]
  sub  eax, ecx      // fix the length to delete

lab2:
  mov  ebx, [edx]    // total length
  lea  esi, [edx+4]  // s
  
  mov  edx, eax
  sub  ebx, ecx      // length to move
  sub  ebx, edx

  test ecx, ecx
  jz   short lab3

labCopy1:
  mov  eax, [esi]
  mov  word ptr [edi], ax
  lea  esi, [esi+2]
  lea  edi, [edi+2]
  sub  ecx, 1
  jnz  short labCopy1

lab3:
  shl  edx, 1
  add  esi, edx
  test ebx, ebx
  jz   short lab4

  add  ebx, 1              // to include trailing zero
labCopy2:
  mov  eax, [esi]
  mov  word ptr [edi], ax
  lea  esi, [esi+2]
  lea  edi, [edi+2]
  sub  ebx, 1
  jnz  short labCopy2

lab4:
  pop  edi
  mov  eax, [esp]

end


// --- WSTR_CPYWCHR (S, CH) ---

inline standard'strgetchar

  pop  eax
  xor  ebx, ebx
  mov  bx,  word ptr [eax]
  mov  eax, [esp]
  lea  edx, [eax+4]
  mov  [eax], 1
  mov  [edx], ebx

end


// --- INT_COPYHWORD (DEST, SOUR) ---

inline standard'strwrite
  
  pop  eax
  mov  edx, [eax]
  mov  eax, [esp]
  shr  edx, 16
  mov  [eax], edx

end


// --- INT_COPYLWORD (DEST, SOUR) ---

inline standard'strwritechar

  pop  eax
  mov  edx, [eax]
  and  edx, 0FFFFh
  mov  eax, [esp]
  mov  [eax], edx

end

// --- INT_CPYSTR (N, S) ---

inline standard'strfromlong

  pop  esi                          // get field
  lodsd
  mov  ecx, eax
  xor  ebx, ebx
  cmp  byte ptr [esi], 2Dh
  jnz  short Lab4
  lodsw
  lea  ecx, [ecx-1]
  lea  ebx, [ebx+1]                 // set flag in ebx
Lab4:
  xor  eax, eax
Lab1:
  mov  edx, 10
  mul  edx
  mov  edx, eax
  xor  eax, eax
  lodsw
  sub  al, 30h
  jb   short Lab2
  cmp  al, 9
  ja   short Lab2
  add  eax, edx
  loop Lab1
  and  ebx, ebx
  jz   short Lab5
  neg  eax
Lab5:
  mov  ebx, [esp]
  mov  [ebx], eax
  mov  eax, ebx
  jmp  short Lab3
Lab2:
  xor  eax, eax
Lab3:

end

// --- STR_COPYINT32 (S, N) ---

procedure standard'strwriteint

   pop  eax
   push ebp
   push [eax]
   mov  ebp, esp
   mov  eax, [eax]     // get dword
   xor  ecx, ecx
   cmp  eax, 0
   jns  short Lab6
   neg  eax
Lab6:
   mov  ebx, 0Ah
   cmp  ebx, eax
   jnbe short Lab5
Lab1:
   xor  edx, edx
   idiv ebx
   push edx
   add  ecx, 1
   cmp  eax, 9
   ja   short Lab1
Lab5:   
   push eax
   add  ecx, 1
   mov  eax, [ebp]
   cmp  eax, 0
   jns  short Lab7
   push 0FDh      // to get "-" after adding 0x30
   add  ecx, 1
Lab7:
   mov  esi, [ebp+8]
   mov  [esi], ecx
   lea  esi, [esi+4]
   mov  ebx, 0FFh
Lab2:
   pop  eax
   add  eax, 30h
   and  eax, ebx
   mov  word ptr [esi], ax
   add  esi, 2
   sub  ecx, 1
   jnz  short Lab2
   xor  eax, eax
   mov  word ptr [esi], ax
   pop  ebx
   pop  ebp
   mov  eax, [esp]

end

// --- FLOAT_COPYINT (DEST, SOUR) ---

inline standard'r64get

  pop  eax
  fild dword ptr [eax]
  mov  eax, [esp]
  fstp qword ptr [eax]

end


// --- FLOAT_EQUAL (F1, F2) ---

inline standard'r64equal

  pop    eax 
  fld    qword ptr [eax]
  mov    eax, [esp]
  fld    qword ptr [eax]
  fcomp  st, st(1)
  fnstsw ax
  fstp   st(0)
  test   ah, 44h
  mov    eax, [esp]
  jnp    short lab1
  xor    eax, eax
lab1:

end


// --- FLOAT_LESS (F1, F2) ---

inline standard'r64less

  pop    eax
  mov    ebx, eax
  mov    eax, [esp]
  fld    qword ptr [eax]
  fld    qword ptr [ebx]
  fcomp  st, st(1)
  fnstsw ax
  fstp   st(0)
  test   ah, 41h
  mov    eax, [esp]
  jp     short lab1
  xor    eax, eax
lab1:

end

// --- LONG_COPY (DEST, SOUR) ---

inline standard'r64load

  pop eax 
  mov ecx, [eax]
  mov edx, [eax+4]
  mov eax, [esp] 
  mov [eax], ecx
  mov [eax+4], edx

end

// --- FLOAT_ADD (DEST, SOUR) ---

inline standard'r64add

  pop  eax
  fld  qword ptr [eax]
  mov  eax, [esp]
  fadd qword ptr [eax] 
  fstp qword ptr [eax]

end


// --- FLOAT_SUB (DEST, SOUR) ---

inline standard'r64sub

  pop  ebx
  mov  eax, [esp]
  fld qword ptr [eax]
  fsub qword ptr [ebx] 
  fstp qword ptr [eax]

end


// --- FLOAT_MUL (DEST, SOUR) ---

inline standard'r64mul

  pop  eax
  fld  qword ptr [eax]
  mov  eax, [esp]
  fmul qword ptr [eax] 
  fstp qword ptr [eax]

end


// --- FLOAT_DIV (DEST, SOUR) ---

inline standard'r64div

  pop  ebx
  mov  eax, [esp]
  fld  qword ptr [eax]
  fdiv qword ptr [ebx] 
  fstp qword ptr [eax]

end


// --- STR_COPYFLOAT (S, F) ---

inline standard'r64tostr

   pop   ecx
   mov   eax, [esp]   
   push  ebp
   mov   ebp, esp
   sub   esp, 52  
   push  edi
   lea   edi, [eax+4]
	
   mov   ebx, [eax]          // get the number of decimal digits (minus 2 for sign and dot)
   lea   ebx, [ebx-2]
   cmp   ebx, 13
   jbe   short ftoa1   
   mov   ebx, 13
ftoa1:
   xor   edx, edx

   //-------------------------------------------
   //first examine the value on FPU for validity
   //-------------------------------------------

   fld   qword ptr [ecx]
   fxam                       // examine value on FPU
   fstsw ax                   // get result

   sahf                       // transfer to CPU flags
   jz    short maybezero
   jpo   srcerr               // C3=0 and C2=0 would be NAN or unsupported
   jnc   short getnumsize      // continue if normal finite number

   //--------------------------------
   //value to be converted = INFINITY
   //--------------------------------

   mov   al,43                // "+"
   test  ah,2                 // C1 field = sign
   jz    short ftoa2
   mov   al, 45               // "-"
ftoa2:
   and   eax, 0FFh
   stosw
   mov   eax,4E0049h        // "NI"
   stosd
   mov   eax,490046h        // "IF"
   stosd
   mov   eax,49004Eh        // "IN"
   stosd
   mov   eax,590054h        // "YT"
   stosd
   jmp   finish      

   //-------------------------
   //value to be converted = 0
   //-------------------------
         
maybezero:
   jpe   short getnumsize      // would be denormalized number
   fstp  st(0)                // flush that 0 value off the FPU
   mov   eax,2E0030h          // ".0" szstring
   stosd                      // write it
   mov   eax,30h              // "0" szstring
   stosd                      // write it
   jmp   finish

   //---------------------------
   // get the size of the number
   //---------------------------

getnumsize:
   fldlg2                     // log10(2)
   fld   st(1)                // copy Src
   fabs                       // insures a positive value
   fyl2x                      // ->[log2(Src)]*[log10(2)] = log10(Src)
      
   fstcw word ptr [ebp-4]     // get current control word
   mov   ax, word ptr [ebp-4]
   or    ax,0C00h             // code it for truncating
   mov   word ptr [ebp-8],ax
   fldcw word ptr [ebp-8]     // insure rounding code of FPU to truncating
      
   fist  [ebp-12]             // store characteristic of logarithm
   fldcw word ptr [ebp-4]     // load back the former control word

   ftst                       // test logarithm for its sign
   fstsw ax                   // get result
   sahf                       // transfer to CPU flags
   sbb   [ebp-12],0           // decrement esize if log is negative
   fstp  st(0)                // get rid of the logarithm

   //-----------------------------------------------------------------------
   // get the power of 10 required to generate an integer with the specified
   // number of significant digits
   //-----------------------------------------------------------------------
   
   mov   eax, [ebp-12]
   or    eax, eax
   js    short ftoa21
   cmp   eax, 13
   jbe   short ftoa20
   mov   edx, -1
   mov   ebx, 13
   mov   ecx, ebx
   sub   ecx, eax
   mov   [ebp-16], ecx
   jmp   short ftoa22

ftoa20:
   add   eax, ebx
   cmp   eax, 13
   jbe   short ftoa21
   sub   eax, 13
   sub   ebx, eax      

ftoa21:
   mov   [ebp-16], ebx

ftoa22:

   //----------------------------------------------------------------------------------------
   // multiply the number by the power of 10 to generate required integer and store it as BCD
   //----------------------------------------------------------------------------------------

   fild  dword ptr [ebp-16]
   fldl2t
   fmulp                      // ->log2(10)*exponent
   fld   st(0)
   frndint                    // get the characteristic of the log
   fxch st(1)
   fsub  st(0),st(1)          // get only the fractional part but keep the characteristic
   f2xm1                      // ->2^(fractional part)-1
   fld1
   faddp                      // add 1 back
   fscale                     // re-adjust the exponent part of the REAL number
   fstp  st(1)                // get rid of the characteristic of the log
   fmulp                      // ->16-digit integer

   fbstp tbyte ptr[ebp-28]    // ->TBYTE containing the packed digits
   fstsw ax                   // retrieve exception flags from FPU
   shr   eax,1                // test for invalid operation
   jc    srcerr               // clean-up and return error

   //------------------------------------------------------------------------------
   // unpack BCD, the 10 bytes returned by the FPU being in the little-endian style
   //------------------------------------------------------------------------------

   lea   esi, [ebp-19]        // go to the most significant byte (sign byte)
   push  edi
   lea   edi,[ebp-52]
   mov   eax,3020h
   mov   cl,byte ptr[esi]     // sign byte
   cmp   cl, 80h
   jnz   short ftoa5
   mov   al, 45               // insert sign if negative number
ftoa5:

   stosw
   mov   ecx,9
ftoa6:
   sub   esi, 1
   movzx eax,byte ptr[esi]
   ror   ax,4
   ror   ah,4
   add   eax,3030h
   stosw
   sub   ecx, 1
   jnz   short ftoa6

   pop   edi
   lea   esi,[ebp-52]
   
   cmp   edx, 0
   jnz   short scientific

   //************************
   // REGULAR STRING NOTATION
   //************************

   movsb                      // insert sign
   xor   eax, eax
   stosb

   cmp   byte ptr[esi-1], 20h // test if we insert space
   jnz   short ftoa60
   lea   edi, [edi-2]         // erase it

ftoa60:
   mov   ecx,1                // at least 1 integer digit
   mov   eax, [ebp-12]
   or    eax, eax             // is size negative (i.e. number smaller than 1)
   js    short ftoa61
   add   ecx, eax

ftoa61:
   mov   eax, ebx
   add   eax, ecx             // ->total number of digits to be displayed
   sub   eax, 19
   sub   esi, eax             // address of 1st digit to be displayed
   cmp   byte ptr[esi-1], 49  // "1"
   jnz   ftoa8 
   sub   esi, 1
   add   ecx, 1 
ftoa8:
   test  ecx, ecx
   jz    short ftoa8End
   xor   eax, eax
ftoa8Next:                    // copy required integer digits
   mov   al, byte ptr [esi]
   mov   word ptr [edi], ax
   lea   esi, [esi+1]
   lea   edi, [edi+2]
   sub   ecx, 1
   jnz   short ftoa8Next
ftoa8End:
   mov   ecx,ebx
   or    ecx,ecx
   jz    short ftoa9
   mov   eax,46               // "."
   stosw

   xor   eax, eax
ftoa9Next:                    // copy required decimal digits
   mov   al, byte ptr [esi]
   mov   word ptr [edi], ax
   lea   esi, [esi+1]
   lea   edi, [edi+2]
   sub   ecx, 1
   jnz   short ftoa9Next
ftoa9:
   jmp   finish

scientific:
   movsb                      // insert sign
   xor   eax, eax
   stosb

   cmp   byte ptr[esi-1], 20h // test if we insert space
   jnz   short ftoa90
   lea   edi, [edi-2]         // erase it

ftoa90:
   mov   ecx, ebx
   mov   eax, 18
   sub   eax, ecx
   add   esi, eax
   cmp   byte ptr[esi-1],49   // "1"
   pushfd                     // save flags for extra "1"
   jnz   short ftoa10
   sub   esi, 1
ftoa10:
   movsb                      // copy the integer
   xor   eax, eax
   stosb

   mov   eax,46               // "."
   stosw

   xor   eax, eax
ftoa10Next:                    // copy the decimal digits
   mov   al, byte ptr [esi]
   mov   word ptr [edi], ax
   lea   esi, [esi+1]
   lea   edi, [edi+2]
   sub   ecx, 1
   jnz   short ftoa10Next

   mov   eax,69                // "E"
   stosw
   mov   eax,43                // "+"
   mov   ecx,[ebp-12]
   popfd                      // retrieve flags for extra "1"
   jnz   short ftoa11          // no extra "1"
   add   ecx, 1               // adjust exponent
ftoa11:
   or    ecx,ecx
   jns   short ftoa12
   mov   eax,45                // "-"
   neg   ecx                  // make number positive
ftoa12:
   stosw                      // insert proper sign

// Note: the absolute value of the size could not exceed 4931
   
   xor   ebx, ebx   
   mov   eax,ecx
   mov   cl,100
   div   cl                   // ->thousands & hundreds in al, tens & units in AH
   push  eax
   and   eax,0ffh             // keep only the thousands & hundreds
   mov   cl,10
   div   cl                   // ->thousands in al, hundreds in AH
   add   eax,3030h            // convert to characters
   mov   bl, al               // insert them 
   mov   word ptr [edi], bx
   lea   edi, [edi+2]
   shr   eax, 8
   mov   bl, al
   mov   word ptr [edi], bx
   lea   edi, [edi+2]
   pop   eax
   shr   eax,8                // get the tens & units in al
   div   cl                   // tens in al, units in AH
   add   eax,3030h            // convert to characters

   mov   bl, al               // insert them 
   mov   word ptr [edi], bx
   lea   edi, [edi+2]
   shr   eax, 8
   mov   bl, al
   mov   word ptr [edi], bx
   lea   edi, [edi+2]

finish:
   cmp   word ptr [edi-2], 48 // '0'
   jnz   short finish1
   lea   edi, [edi-2]
   jmp   short finish

finish1:
   cmp   word ptr [edi-2], 46 // '.'
   jnz   short finish2
   lea   edi, [edi+2]

finish2:
   xor   ecx, ecx
   mov   word ptr [edi], cx
   mov   ebx, edi
   pop   edi
   add   esp, 52
   pop   ebp

   mov   eax, [esp]
   sub   ebx, eax
   lea   ebx, [ebx - 4]
   shr   ebx, 1
   mov   [eax], ebx

   jmp   short finish3

srcerr:
   pop   edi
   add   esp, 52
   pop   ebp
   xor   eax,eax
finish3:

/*
oldcw   :-4  (4)
truncw  :-8  (4)
esize   :-12 (4)
tempdw  :-16 (4)
bcdstr  :-28 (12)  // -20
unpacked:- (52)  // -32
*/

end

// --- FLOAT_CPYSTR (F, S) ---

inline standard'r64tostrx

  pop   eax
  push  edi
  lea   esi, [eax+4] 
  sub   esp, 12
  xor   edx, edx
  xor   eax, eax
  xor   ebx, ebx
  mov   edi, esp
  stosd
  stosd
  mov   word ptr [edi], ax
  mov   ecx, 19

atof1:
  lodsw
  cmp   eax, 32                  // " "
  jz    short atof1
  or    eax, eax
  jnz   short atof2

atoflerr:
  add   esp, 12
  pop   edi
  xor   eax, eax
  jmp   atoflend

  //*----------------------
  //* check for leading sign
  //*----------------------

atof2:

  cmp   eax, 43                  // +
  jz    short atof3
  cmp   eax,45                   // -
  jnz   short integer
  mov   dh,80h
atof3:
  mov   byte ptr [edi+1], dh    // put sign byte in bcd string
  xor   edx,edx
  lodsw

  //*------------------------------------
  //*convert the digits to packed decimal
  //*------------------------------------
integer:

  cmp   eax, 46                  // .
  jnz   short atof4
  test  bh, 1
  jnz   short atoflerr           // only one decimal point allowed
  or    bh, 1
  lodsw
atof4:
  cmp   eax, 101                 // "e"
  jnz   short atof5 
  cmp   cl, 19
  jnz   short atof41
  test  bh, 4
  jz    short atoflerr
atof41:  
  jmp   scient
atof5:
  cmp   eax,69                  // "E" 
  jnz   short atof6
  cmp   cl, 19
  jnz   short atof51
  test  bh, 4
  jz    short atoflerr
atof51:  
  jmp   scient
atof6:
  or    eax,eax
  jnz   short atof7
  cmp   cl, 19
  jnz   atof61
  test  bh, 4
  jz    short atoflerr
atof61:
  jmp   laststep1
atof7:
  sub   eax,48                 // "0"
  jc    short atoflerr          // unacceptable character
  cmp   eax,9
  ja    short atoflerr          // unacceptable character
  or    bh,4                   // at least 1 numerical character
  test  bh,1
  jz    short atof8
  add   bl,1                   // bl holds number of decimal digits
  jc    atoflerr               // more than 255 decimal digits
atof8:
  test  eax, eax
  jnz   short atof9
  test  bh,2
  jnz   short atof9
  lodsw
  jmp   short integer
atof9:
  or    bh,2                   // at least 1 non-zero numerical character
  sub   ecx, 1
  jnz   short atof10
  test  bh,1                   // check if decimal point
  jz    atoflerr               // error if more than 18 integer digits in number
  test  eax, eax
  jnz   short atof91            // if trailing decimal 0
  add   ecx, 1
  sub   bl, 1
  lodsw
  jmp   integer
atof91:
  jmp   atoflerr
atof10:
  mov   dh,al
  
integer1:
  lodsw
  cmp   eax, 46                 // "."
  jnz   short atof20
  test  bh,1
  jnz   atoflerr               // only one decimal point allowed
  or    bh, 1                  // use bh bit0 as the decimal point flag
  lodsw
atof20:
  cmp   eax, 101                // "e"
  jnz   short atof30
  mov   ah, dh
  mov   al,0
  rol   al,4
  ror   ax,4
  mov   byte ptr [edi],al
  mov   dh, ah
  jmp   scient
atof30:
  cmp   eax, 69                 // "E"
  jnz   short atof40
  mov   ah, dh
  mov   al,0
  rol   al,4
  ror   ax,4
  mov   byte ptr [edi],al
  mov   dh, ah
  jmp   scient
atof40:  
  or    eax,eax
  jnz   short atof50
  mov   ah, dh
  rol   al,4
  ror   ax,4
  mov   byte ptr [edi],al
  mov   dh, ah
  jmp   short laststep1
atof50:
  sub   eax, 48               // "0"
  jc    atoflerr             // unacceptable character
  cmp   eax,9
  ja    atoflerr             // unacceptable character
  test  bh,1            
  jz    short atof60
  add   bl, 1                // processing decimal digits
atof60:
  sub   ecx, 1
  jnz   short atof70
  test  bh,1                // check if decimal point
  jz    atoflerr            // error if more than 18 integer digits in number
  test  eax, eax
  jnz   short atof602
  add   ecx, 1
  sub   bl, 1
  jmp   integer1
atof602:
  jmp   atoflerr
atof70:
  mov   ah, dh
  rol   al,4
  ror   ax,4
  mov   byte ptr [edi],al
  mov   dh, ah
  sub   edi, 1
  lodsw
  jmp   integer

laststep1:
  cmp   cl,19
  jnz   short laststep
  fldz
  jmp   short laststep2

laststep:
  mov   ah, dh
  xor   edx, edx
  fbld  [esp]
  sub   cl, 1
  add   bl,cl
  movzx eax,bl
  sub   edx,eax

  push  edx
  fild  dword ptr [esp]     // load the exponent
  fldl2t                    // load log2(10)
  fmulp                     // ->log2(10)*exponent
  pop   edx

  // at this point, only the log base 2 of the 10^exponent is on the FPU
  // the FPU can compute the antilog only with the mantissa
  // the characteristic of the logarithm must thus be removed
     
  fld   st(0)             // copy the logarithm
  frndint                 // keep only the characteristic
  fsub  st(1),st(0)       // keeps only the mantissa
  fxch st(1)              // get the mantissa on top

  f2xm1                   // ->2^(mantissa)-1
  fld1
  faddp                   // add 1 back

  // the number must now be readjusted for the characteristic of the logarithm

  fscale                  // scale it with the characteristic
      
  // the characteristic is still on the FPU and must be removed

  fstp  st(1)             // clean-up the register

  fmulp
  fstsw ax                // retrieve exception flags from FPU
  shr   al,1              // test for invalid operation
  jc    atoflerr          // clean-up and return error

laststep2:

  add   esp, 12
  pop   edi
  mov   eax, [esp]
  fstp  qword ptr[eax]    // store result at specified address
  jmp   short atoflend

scient:
  cmp   cl,19
  jnz   short atof80
  fldz
  jmp   short laststep2
  xor   edx, edx

atof80:
  xor   eax,eax
  lodsw
  cmp   ax, 43            // "+"
  jz    atof90
  cmp   ax, 45            // "-"
  jnz   short scient1
  stc
  rcr   eax,1             // keep sign of exponent in most significant bit of EAX
     
atof90:

  lodsw                   // get next digit after sign

scient1:
  push  eax
  and   eax,0ffh
  jnz   short atof100      // continue if 1st byte of exponent is not terminating 0

scienterr:
  pop   eax
  jmp    atoflerr         // no exponent

atof100:
  sub   eax,30h
  jc    short scienterr    // unacceptable character
  cmp   eax,9
  ja    short scienterr    // unacceptable character
  imul  edx,10
  add   edx,eax
  cmp   edx,4931h
  ja    short scienterr    // exponent too large
  lodsw
  or    eax,eax
  jnz   short atof100
  pop   eax               // retrieve exponent sign flag
  rcl   eax,1             // is most significant bit set?
  jnc   short atof200
  neg   edx

atof200:
  jmp   laststep  

atoflend:

end


// --- WSTR_ALLOC (aType, aLen) ---

inline standard'arrenum

  mov  eax, [esp]
  mov  ebx, [eax] 
  shl  ebx, 1
  add  ebx, 6
  mov  ecx, 'gc_empty_object_aligned
  add  ecx, ebx
  and  ecx, 'gc_page_mask
  add  ebx, 3
  shr  ebx, 2
  call @"$package'elena'alloc"
  mov  edx, [esp+4]
  mov  esi, [edx]
  or   [eax-8], 'gc_binary
  mov  [eax-4], esi
  pop  ebx
  mov  ecx, [ebx]
  mov  [eax], ecx
  mov  [esp], eax

end

// OBJ_ALLOC (aType, aPattern, aLen)

inline standard'arrenumnew 

  mov  eax, [esp]
  mov  ebx, [eax]
  mov  ecx, ebx
  shl  ecx, 2
  add  ecx, 'gc_empty_object_aligned
  and  ecx, 'gc_page_mask
  call @"$package'elena'alloc"
  mov  edx, [esp+8]
  mov  esi, [edx]
  mov  [eax-4], esi
  pop  ebx
  mov  ecx, [ebx]
  pop  edx
  mov  [esp], eax
  mov  esi, eax
labNext:
  mov  [esi], edx
  lea  esi, [esi+4]
  sub  ecx, 1
  jnz  short labNext  

end

// --- INT_LOADSTRADDR (INT, S) ---

inline standard'arrfromstr

  pop  eax
  mov  ebx, [esp]
  lea  edx, [eax+4]  
  mov [ebx], edx  

end


// --- ARR_GET (PTR, OFFS) ---

inline standard'arridxnew

  pop  eax
  mov  ebx, [eax]
  mov  edx, [esp]
  mov  ecx, [edx-8]
  xor  eax, eax

  test ebx, ebx
  js   short lEnd
  
  cmp  ebx, ecx
  jge  short lEnd

  shl  ebx, 2
  lea  eax, [edx+ebx]
  mov  eax, [eax]
  mov  [esp], eax
lEnd:

end
  

// --- ARR_SET (PTR, OFFS, OBJ) ---

procedure standard'varset (PTR, OFFS, OBJ)

  mov  edx, OBJ
  mov  ebx, OFFS
  mov  ebx, [ebx]
  mov  esi, PTR
  mov  ecx, [esi-8]
  xor  eax, eax

  test ebx, ebx
  js   short lEnd
  
  cmp  ebx, ecx
  jge  short lEnd

  push edi                      // store edi
  shl  ebx, 2
  mov  eax, esi

  lea  esi, [eax+ebx]           // set parameters
  mov  edi, eax
  mov  eax, edx

  call @"$package'elena'barrier"     // assign eax to [esi]

  pop  edi                      // restor edi
  mov  eax, PTR
lEnd:
  ret

end

// --- ARRAY_LEN (RETVAL, OBJ) ---

inline standard'arrlen

  pop  eax
  mov  edx, [eax-8]
  mov  eax, [esp]
  mov  [eax], edx
  
end


// --- GROUP_ADD (ARRAY, NIL, OBJ)---

inline standard'arrcopy 

  pop  eax
  pop  edx

  push edi
  mov  edi, [esp+4]
  mov  ecx, [edi-8]

  mov  esi, edi
lNext:
  cmp  edx, [esi]
  jz   short lInsert
  lea  esi, [esi+4]
  sub  ecx, 1
  jnz  short lNext
  xor  eax, eax
  jmp  short lEnd

lInsert:  
  call @"$package'elena'barrier"     // assign eax to [esi], edi
  mov  eax, [esp+4]

lEnd:
  pop  edi

end


// --- GROUP_CPY (DEST, SOUR) ---

inline standard'arrfill

  pop  esi                      // sour 
  mov  eax, [esp]               
  mov  edx, eax                 // dest
  mov  ecx, [esi-8]             // get sour length
  test ecx, ecx
  jz   short Lab2
Lab1:
  mov  ebx, [esi]
  mov  [edx], ebx
  add  esi, 4
  add  edx, 4
  sub  ecx, 1
  jnz  short Lab1
Lab2:

end

// --- LONG_COPYINT (DEST, SOUR) ---

inline standard'i64get

  pop eax
  mov eax, [eax]
  cdq
  mov ebx, [esp] 
  mov [ebx], eax
  mov [ebx+4], edx
  mov eax, ebx

end


// --- FLOAT_COPYLONG (DEST, SOUR) ---

inline standard'i64toreal

  pop  eax
  fild qword ptr [eax]
  mov  eax, [esp]
  fstp qword ptr [eax]

end

// --- LONG_EQUAL (N1, N2) ---

inline standard'i64equal

  pop  eax
  mov  edx, [eax]
  mov  ecx, [eax+4]
  mov  eax, [esp]
  cmp  [eax], edx
  jnz  short Lab2
  cmp  [eax+4], ecx
  jz   short Lab1
Lab2:
  xor  eax, eax
Lab1:

end

// --- LONG_LESS (N1, N2) ---

inline standard'i64less

   pop  eax
   mov  ecx, [eax+4]
   mov  edx, [eax]
   mov  eax, [esp]
   cmp  [eax+4], ecx
   jl   short Lab1
   nop
   jnz  short Lab2
   cmp  [eax], edx
   jl   short Lab1
Lab2:
   xor  eax, eax
Lab1:

end

// --- LONG_ADD (DEST, SOUR) ---
	
inline standard'i64add

  pop eax 
  mov esi, [esp]

  mov ebx, [eax]
  mov edx, [esi]

  mov eax, [eax+4]
  mov ecx, [esi+4]

  add ebx, edx
  adc eax, ecx

  mov [esi], ebx
  mov [esi+4], eax
  mov eax, esi

end


// --- LONG_SUB (DEST, SOUR) ---

inline standard'i64sub

  pop eax
  mov esi, [esp]

  mov ebx, [eax]
  mov edx, [esi]

  mov eax, [eax+4]
  mov ecx, [esi+4]

  sub edx, ebx
  sbb ecx, eax

  mov [esi], ebx
  mov [esi+4], eax
  mov eax, esi

end

// --- LONG_MUL (DEST, SOUR) ---

inline standard'i64mul  // SLO * DLO + SLO * DHI + SLO * DHI

  pop  eax
  mov  edx, [esp]     // dest
  mov  esi, eax       // sour
  mov  eax, [esi+4]   // hidword
  mov  ecx, [edx+4]
  or   eax, ecx
  mov  ecx, [edx]
  jnz  short lLong
  mov  eax, [esi]
  mul  ecx
  jmp  short lEnd

lLong:
  push edi
  mov  edi, [esp]
  mul  ecx               // SHI * DHI
  mov  ebx, eax
  mov  eax, dword ptr [esi]
  mul  dword ptr [edi+4]  // SLO * DHI
  add  ebx, eax     
  mov  eax, dword ptr [esi] // SLO * DLO
  mul  ecx
  add  edx, ebx 
  pop  edi

lEnd:
  mov  ebx, [esp]
  mov  [ebx], eax
  mov  [ebx+4], edx
  mov  eax, ebx

end

// --- LONG_DIV (DEST, SOUR) ---

inline standard'i64div

  pop  eax
  push edi
  mov  esi, [esp]
  push [esi+4]    // sour hi dword
  push [esi]      // sour lo dword
  push [eax+4]    // dest hi dword
  push [eax]      // dest lo dword

  xor  edi, edi

  mov  eax, [esp+4]
  or   eax, eax
  jge  short L1
  add  edi, 1
  mov  edx, [esp]
  neg  eax
  neg  edx
  sbb  eax, 0
  mov  [esp], edx
  mov  [esp+4], eax

L1:
  mov  eax, [esi+4] // high dword of sour
  or   eax, eax
  jge  short L2
  add  edi, 1
  mov  edx, [esi]   // low dword of sour
  neg  eax
  neg  edx
  sbb  eax, 0
  mov  [esp+0Ch], eax
  mov  [esp+08], edx

L2:
  or   eax, eax
  jnz  short L3
  mov  ecx, [esp+08]
  mov  eax, [esp+4]
  xor  edx, edx
  div  ecx
  mov  ebx,eax 
  mov  eax, [esp]
  div  ecx
  mov  edx,ebx 
  jmp  short L4

L3:
  mov  ebx, eax 
  mov  ecx, dword ptr [esp+08]  // sour lo
  mov  edx, dword ptr [esp+0Ch] // sour hi
  mov  eax, [esp] // dest lo
L5:
  shr  ebx, 1 
  rcr  ecx, 1
  shr  edx, 1 
  rcr  eax, 1
  or   ebx, ebx 
  jnz  short L5
  div  ecx
  mov  esi, eax  // result

  // check the result with the original
  mul  dword ptr [esp+0Ch]
  mov  ecx, eax 
  mov  eax, dword ptr [esp+8]
  mul  esi
  add  edx, ecx

  // carry means Quotient is off by 1
  jb   short L6

  cmp  edx, dword ptr [esp+4] 
  ja   short L6
  jb   short L7
  cmp  eax,dword ptr [esp] 
  jbe  short L7

L6:
  sub  esi, 1

L7:
  xor  edx, edx
  mov  eax, esi 

L4:
  sub  edi, 1
  jnz  short L8

  neg  edx
  neg  eax
  sbb  edx, 0 

L8:
  lea  esp, [esp+10h]
  pop  edi

  mov  ebx, [esp]
  mov  [ebx], eax
  mov  [ebx+4], edx
  mov  eax, ebx

end


// --- LONG_TEST (N1, N2) ---

inline standard'i64anymask

  pop  eax
  mov  edx, [esp]
  mov  ebx, [edx]
  mov  ecx, [edx+4]
  test [eax], edx
  jz   short L1
  test [eax+4], ecx
  jnz  short L2

L1:
  xor  eax, eax

L2:

end

// --- LONG_TEST2 (N1, N2) ---

inline standard'i64allmask

  pop  eax
  mov  edx, [esp]
  mov  ebx, [edx]
  mov  ecx, [edx+4]
  and  ebx, [eax]
  cmp  ebx, dword ptr [eax]
  jnz  short L1
  and  ecx, [eax+4]
  cmp  [eax+4], ecx
  jnz  short L2

L1:
  xor  eax, eax

L2:

end

// --- LONG_SHIFT (DEST, OFFSET) ---

inline standard'i64shift

  pop  eax
  mov  ecx, [eax]
  mov  ebx, [esp]
  mov  eax, [ebx]
  mov  edx, [ebx+4]

  and  ecx, ecx
  jns  short LR
  neg  ecx

  cmp  cl, 40h 
  jae  short lErr
  cmp  cl, 20h
  jae  short LL32
  shld edx, eax, cl
  shl  eax, cl
  mov  eax, [esp]
  jmp  short lEnd

LL32:
  mov  edx, eax
  xor  eax, eax
  and  cl, 1Fh
  shl  edx, cl 
  mov  eax, [esp]
  jmp  short lEnd

LR:

  cmp  cl, 64
  jae  short lErr

  cmp  cl, 32
  jae  short LR32
  shrd eax, edx, cl
  sar  edx, cl
  mov  eax, [esp]
  jmp  short lEnd

LR32:
  mov  eax, edx
  sar  edx, 31
  and  cl, 31
  sar  eax, cl
  jmp  short lEnd

lClear:
  xor  eax, eax  
  xor  edx, edx

lEnd:
  mov  ebx, [eax]
  mov  [ebx], eax
  mov  [ebx+4], edx
  mov  eax, ebx

end

// --- LONG_NOT ---

inline standard'i64not

  pop eax 
  mov edx, [eax]      // NLO
  mov ebx, [eax+4]    // NHI 

  neg  ebx 
  neg  edx 
  sbb  ebx, 0

  mov eax, [esp]
  mov [eax], edx
  mov [eax+4], ebx

end

// --- LONG_BOR (DEST, SOUR) ---

inline standard'i64or

  pop eax 
  mov ebx, [eax]
  mov ecx, [eax+4]
  mov eax, [esp]
  or  [eax], ebx
  or  [eax+4], ecx

end

// --- LONG_BXOR (DEST, SOUR) ---

inline standard'i64xor

  pop eax
  mov ebx, [eax]
  mov ecx, [eax+4]
  mov eax, [esp]
  xor [eax], ebx
  xor [eax+4], ecx

end

// --- LONG_BAND (DEST, SOUR) ---

inline standard'i64and

  pop eax
  mov ebx, [eax]
  mov ecx, [eax+4]
  mov eax, [esp]
  and [eax], ebx
  and [eax+4], ecx

end

// --- INT_COPYPTR ( N, PTR, OFS) ---

inline standard'bufreadint
  
  pop  eax
  mov  edx, [eax]
  pop  ebx
  mov  ecx, [ebx-8]   
  shl  ecx, 2
  sub  ecx, edx
  jae  short lCopy
  xor  eax, eax
  jmp  short lEnd
lCopy:
  add  ebx, edx
  mov  eax, [esp]
  mov  edx, [ebx]
  mov  [eax], edx 
lEnd:

end

// --- STR_COPYLONG (S, N) ---

inline standard'i64tostr

   pop  eax
   push edi
   push ebp
   push [eax+4]
   mov  ebp, esp
   mov  edx, [eax]     // NLO
   mov  eax, [eax+4]   // NHI
   xor  ecx, ecx
   push ecx 
   or   eax, eax
   jge  short Lab6

   neg  eax 
   neg  edx 
   sbb  eax, 0

Lab6:                 // convert 
   mov  esi, edx      // NLO
   mov  edi, eax      // NHI
   mov  ecx, 10       // LO

Lab1:
   test edi, edi
   jnz  short labConvert
   cmp  esi, 9
   jbe  short Lab5

labConvert:
   mov  eax, edi      // NHI
   xor  edx, edx
   div  ecx
   mov  ebx, eax
   mov  eax, esi      // NLO
   div  ecx
   mov  edi,ebx 
   mov  esi,eax

   push edx
   add  [ebp-4], 1
   jmp  short Lab1

Lab5:   
   push esi

   mov  ecx, [ebp-4]
   add  ecx, 1

   mov  eax, [ebp]
   cmp  eax, 0
   jns  short Lab7
   push 0FDh      // to get "-" after adding 0x30
   add  ecx, 1
Lab7:
   mov  esi, [ebp+12]
   mov  [esi], ecx
   lea  esi, [esi+4]
   mov  ebx, 0FFh
Lab2:
   pop  eax
   add  eax, 30h
   and  eax, ebx
   mov  word ptr [esi], ax
   add  esi, 2
   sub  ecx, 1
   jnz  short Lab2
   xor  eax, eax
   mov  word ptr [esi], ax
   lea  esp, [esp+8]
   pop  ebp
   pop  edi
   mov  eax, [esp]

end

// --- LONG_CPYSTR (N, S) ---

inline standard'i64tostrx

  pop  eax
  push edi

  mov  esi, eax     // get string
  lodsd
  mov  ecx, eax     // get string count

  xor  ebx, ebx
  cmp  byte ptr [esi], 2Dh
  jnz  short labStart

  lea  esi, [esi+2]
  lea  ecx, [ecx-1]
  mov  ebx, 1        // set flag in ebx

labStart:
  push ebx           // save sign flag
  xor  edi, edi      // edi   - DHI
  xor  ebx, ebx      // ebx   - DLO

labConvert:
  mov  edx, 10
  mov  eax, edi
  mul  edx           // DHI * 10
  mov  edi, eax

  mov  eax, ebx
  mov  edx, 10
  mul  edx           // DLO * 10
  add  edi, edx
  mov  ebx, eax

  xor  eax, eax
  lodsw
  sub  al, 30h
  jb   short labErr
  cmp  al, 9
  ja   short labErr

  add ebx, eax       // DLO + EAX
  adc edi, 0         // DHI + CF

  sub  ecx, 1
  jnz  short labConvert

  pop  eax           // restore flag
  test eax, eax
  jz   short labSave

  not  edi           // invert number
  neg  ebx

labSave:

  mov  edx, edi
  pop  edi

  mov  eax, [esp]
  mov  [eax], ebx
  mov  [eax+4], edx
  jmp  short labEnd

labErr:
  xor  eax, eax
  pop  ebx
  pop  edi

labEnd:

end


// --- WSTR_SUBSTR (RETVAL, S, INDEX) ---

inline standard'strwritereal

  pop  eax
  pop  edx
  mov  ebx, [eax]
  shl  ebx, 1
  mov  eax, [esp]

  lea  edx, [edx+4]
  add  edx, ebx
  lea  esi, [eax+4]
  mov  ecx, [eax]
  test ecx, ecx
  jz   short lab2
lab1:
  mov  bx, word ptr [edx]
  mov  word ptr[esi], bx
  lea  esi, [esi+2]
  lea  edx, [edx+2]
  sub  ecx, 1
  jnz  short lab1
lab2:
  xor  ebx, ebx
  mov  word ptr[esi], bx

end

// --- FLOAT_ABS (DEST, SOUR) ---

inline standard'r64abs

  pop   eax
  fld   qword ptr [eax]  
  fabs
  mov   eax, [esp]
  fstp  qword ptr [eax]    // store result 

end

// --- INT_ROUNDREAL (EAX - N, EBX - F) ---

inline standard'r64conv66

  pop   eax
  fld   qword ptr [eax]

  push  eax               // reserve space on CPU stack
  fstcw word ptr [esp]    // get current control word
  mov   ax,[esp]
  and   ax,0F3FFh         // code it for rounding 
  push  eax
  fldcw word ptr [esp]    // change rounding code of FPU to round

  frndint                 // round the number
  pop   eax               // get rid of last push
  fldcw word ptr [esp]    // load back the former control word

  fstsw ax                // retrieve exception flags from FPU
  shr   al,1              // test for invalid operation
  pop   ebx               // clean CPU stack
  jc    short lErr         // clean-up and return error

  mov   eax, [esp] 
  fistp dword ptr [eax]

  jmp   short lEnd

lErr:
  xor   eax, eax
  ffree st(0)

lEnd:

end

// --- FLOAT_TRUNC (DEST, SOUR) ---

inline standard'r64conv67

  pop   eax
  fld   qword ptr [eax]

  push  ebx                // reserve space on stack
  fstcw word ptr [esp]     // get current control word
  mov   bx, [esp]
  or    bx,0c00h           // code it for truncating
  push  ebx
  fldcw word ptr [esp]    // change rounding code of FPU to truncate

  frndint                  // truncate the number
  pop   ebx                // remove modified CW from CPU stack
  fldcw word ptr [esp]     // load back the former control word
  pop   ebx                // clean CPU stack
      
  fstsw ax                 // retrieve exception flags from FPU
  shr   al,1               // test for invalid operation
  jnc   short labSave       // clean-up and return error

  xor   eax, eax
  jmp   short labEnd

labSave:

  mov   eax, [esp]
  fstp  qword ptr [eax]    // store result

labEnd:

end

// --- FLOAT_ARCTAN (DEST, SOUR) ---

inline standard'r64conv68

  pop   eax
  fld   qword ptr [eax]  
  fld1
  fpatan                  // i.e. arctan(Src/1)

  fstsw ax                // retrieve exception flags from FPU
  shr   eax,1             // test for invalid operation
  jc    short lErr        // clean-up and return error

  mov   eax, [esp]
  fstp  qword ptr [eax]   // store result 
  jmp   short lEnd

lErr:
  xor   eax, eax 

lEnd:

end

// --- FLOAT_COS (DEST, SOUR) ---

inline standard'r64conv69

  pop   eax
  fld   qword ptr [eax]  
  fldpi
  fadd  st(0),st(0)       // ->2pi
  fxch

lReduce:
  fprem                   // reduce the angle
  fcos
  fstsw ax                // retrieve exception flags from FPU
  shr   al,1              // test for invalid operation
  jc    short lErr         // clean-up and return error
  sahf                    // transfer to the CPU flags
  jpe   short lReduce      // reduce angle again if necessary
  fstp  st(1)             // get rid of the 2pi

  mov   eax, [esp]
  fstp  qword ptr [eax]   // store result 
  jmp   short lEnd

lErr:
  xor   eax, eax 

lEnd:

end

// --- FLOAT_EXP (DEST, SOUR) ---

inline standard'r64conv70

  pop   eax
  fld   qword ptr [eax]   // Src

  fldl2e                  // ->log2(e)
  fmul st(0), st(1)       // ->log2(e)*Src
      
  // the FPU can compute the antilog only with the mantissa
  // the characteristic of the logarithm must thus be removed
      
  fld   st(0)             // copy the logarithm
  frndint                 // keep only the characteristic
  fsub  st(1),st(0)       // keeps only the mantissa
  fxch                    // get the mantissa on top

  f2xm1                   // ->2^(mantissa)-1
  fld1
  fadd  st(0), st(1)      // add 1 back

  // the number must now be readjusted for the characteristic of the logarithm

  fscale                  // scale it with the characteristic
      
  fstsw ax                // retrieve exception flags from FPU
  shr   al,1              // test for invalid operation
  jc    short lErr        // clean-up and return if error
      
  // the characteristic is still on the FPU and must be removed

  fstp  st(1)             // get rid of the characteristic

  mov   eax, [esp]
  fstp  qword ptr [eax]    // store result 
  jmp   short lEnd

lErr:
  xor   eax, eax 

lEnd:

end

// --- FLOAT_LN (DEST, SOUR) ---

inline standard'r64conv71

  pop   eax
  fld   qword ptr [eax]  

  fldln2
  fxch
  fyl2x                   // ->[log2(Src)]*ln(2) = ln(Src)

  fstsw ax                // retrieve exception flags from FPU
  shr   al,1              // test for invalid operation
  jc    short lErr         // clean-up and return error

  mov   eax, [esp]
  fstp  qword ptr [eax]    // store result 
  jmp   short lEnd

lErr:
  xor   eax, eax 

lEnd:

end

// --- FLOAT_SIN (DEST, SOUR) ---

inline standard'r64conv72

  pop   eax
  fld   qword ptr [eax]  
  fldpi
  fadd  st(0),st(0)       // ->2pi
  fxch

lReduce:
  fprem                   // reduce the angle
  fsin
  fstsw ax                // retrieve exception flags from FPU
  shr   al,1              // test for invalid operation
  jc    short lErr         // clean-up and return error
  sahf                    // transfer to the CPU flags
  jpe   short lReduce      // reduce angle again if necessary
  fstp  st(1)             // get rid of the 2pi

  mov   eax, [esp]
  fstp  qword ptr [eax]    // store result 
  jmp   short lEnd

lErr:
  xor   eax, eax 

lEnd:

end

// --- FLOAT_SQRT (DEST, SOUR) ---

inline standard'r64conv73

  pop   eax
  fld   qword ptr [eax]  
  fsqrt
  mov   eax, [esp]
  fstp  qword ptr [eax]    // store result 

end


// --- FLOAT_PI (DEST) ---

inline standard'r64conv74

  pop   eax
  fldpi
  mov   eax, [esp]
  fstp  qword ptr [eax]    // store result 

end

// --- DUMP_LEN (RETVAL, OBJ) ---

inline standard'bytearrlen

  pop  eax
  mov  edx, [eax-8]
  shl  edx, 2 
  mov  eax, [esp]
  mov  [eax], edx
  
end

// --- DUMP_CPY (DEST, SOUR)

inline standard'bufappend

  pop  eax
  mov  esi, eax                 // sour 
  mov  eax, [esp]               
  mov  edx, eax                 // dest
  mov  ecx, [esi-8]             // get sour length
  and  ecx, 7FFFFFFFh
  test ecx, ecx
  jz   short Lab2
Lab1:
  mov  ebx, [esi]
  mov  [edx], ebx
  add  esi, 4
  add  edx, 4
  sub  ecx, 1
  jnz  short Lab1
Lab2:

end

// --- WSTR_CPYPTR (STR, PTR, OFFS)

inline standard'bufreadstr

  pop  eax
  pop  esi
  mov  edx, [eax]
  
  mov  ecx, [esi-8]
  shl  ecx, 2
  sub  ecx, edx
  jae  short lCopy
  xor  eax, eax
  jmp  short lEnd

lCopy:
  add  esi, edx
  mov  edx, [esp]

  add  ecx, 1
  shr  ecx, 1
  cmp  ecx, [edx]
  jae  short lCopy2

  mov  [edx], ecx

lCopy2:
  mov  ecx, [edx]
  lea  edx, [edx+4]
  add  ecx, 1
  shr  ecx, 1

lNext:
  mov  ebx, [esi]
  mov  [edx], ebx
  lea  esi, [esi+4]
  lea  edx, [edx+4]
  sub  ecx, 1
  jnz  short lNext
  mov  eax, [esp]
  mov  ebx, [eax]
  shl  ebx, 1
  add  ebx, eax
  lea  ebx, [ebx+4] 
  mov  word ptr [ebx], cx
lEnd:

end

// --- DUMP_READ2BUF (BUF, DUMP, OFFS)

inline standard'bufreadbuf

  pop  eax          
  pop  esi          // dump
  mov  edx, [esp]   // buf
  mov  ebx, [eax]   // offs

  mov  ecx, [edx]   // buffer size

  mov  eax, [esi-8] // dump size
  shl  eax, 2
  sub  eax, ebx    // the size should be read
  ja   short lCheck
  xor  eax, eax
  jmp  short lEnd

lCheck:               
  cmp  eax, ecx       // define number to copy
  ja   short lStart
  mov  ecx, eax

lStart:
  mov  [edx], ecx
  add  esi, ebx
  add  edx, 4

  mov  eax, ebx
  and  ebx, 3
  jz   short lCopy2

lCopyBytes:
  mov  bl, byte ptr [esi]
  mov  byte ptr [edx], bl
  add  edx, 1
  add  esi, 1
  sub  ecx, 1
  jz   short lCopyEnd
  sub  eax, 1
  jnz  short lCopyBytes

lCopy2:
  add  ecx, 3
  shr  ecx, 2

lNext:
  mov  ebx, [esi]
  mov  [edx], ebx
  lea  esi, [esi+4]
  lea  edx, [edx+4]
  sub  ecx, 1
  jnz  short lNext

lCopyEnd:
  mov  eax, [esp]

lEnd:

end

// --- PTR_ADDW (DEST, OFFS, SOUR) ---

inline standard'bufwritestr

  pop  eax
  pop  edx                     // offs
  mov  esi, eax                // sour 
  mov  eax, [esp]              // dest

  mov  edx, [edx]

  mov  ecx, [eax-8]
  shl  ecx, 2
  sub  ecx, edx
  mov  ebx, [esi]
  add  ebx, 1
  shr  ebx, 1
  sub  ecx, ebx
  jae  short lCopy
  xor  eax, eax
  jmp  short labEnd

lCopy:
  add  edx, eax
  mov  ecx, [esi]
  test ecx, ecx
  jz   short labEnd
  add  ecx, 1
  shr  ecx, 1
  lea  esi, [esi+4]
labNext:
  mov  ebx, [esi]
  mov  [edx], ebx
  add  esi, 4
  add  edx, 4
  sub  ecx, 1
  jnz  short labNext
labEnd:

end

// --- PTR_COPYINT32 (PTR, OFS, N) ---

inline standard'bufwriteint

  pop  eax
  pop  edx            // OFS
  mov  ebx, [eax]     // N
  mov  eax, [esp]     // PTR

  mov  edx, [edx]
  mov  ecx, [eax-8]   
  shl  ecx, 2
  sub  ecx, edx
  jae  short lCopy
  xor  eax, eax
  jmp  short lEnd
lCopy:
  add  edx, eax
  mov  [edx], ebx
lEnd:

end

// --- PTR_ADDBUF (BUF, DUMP, OFFS)

inline standard'bufwritebuf

  pop  eax
  pop  esi          // dump
  mov  edx, [esp]   // buf
  mov  ebx, [eax]   // offs

  mov  ecx, [edx]   // get size

  mov  eax, [esi-8] // check if enough place
  shl  eax, 2
  sub  eax, ebx
  ja   short lCheck
  xor  eax, eax
  jmp  short lEnd

lCheck:               
  cmp  eax, ecx
  jae  short lCopy
  xor  eax, eax
  jmp  short lEnd

  lea  edx, [edx+4]

lCopy:  
  add  esi, ebx

  mov  eax, ebx
  and  ebx, 3
  jz   short lCopy2

lCopyBytes:
  mov  bl, byte ptr [edx]
  mov  byte ptr [esi], bl
  add  edx, 1
  add  esi, 1
  sub  ecx, 1
  jz   short lCopyEnd
  sub  eax, 1
  jnz  short lCopyBytes

lCopy2:
  add  ecx, 3
  shr  ecx, 2

lNext:
  mov  ebx, [edx]
  mov  [esi], ebx
  lea  esi, [esi+4]
  lea  edx, [edx+4]
  sub  ecx, 1
  jnz  short lNext

lCopyEnd:
  mov  eax, [esp]

lEnd:

end

// --- DUMP_ALLOC (aType, aLen) ---

inline standard'bufwriteto

  mov  eax, [esp]
  mov  ebx, [eax] 
  mov  ecx, 'gc_empty_object_aligned
  add  ecx, ebx
  and  ecx, 'gc_page_mask
  add  ebx, 3
  shr  ebx, 2
  call @"$package'elena'alloc"
  mov  edx, [esp+4]
  mov  esi, [edx]
  or   [eax-8], 'gc_binary
  mov  [eax-4], esi
  pop  ebx
  mov  ecx, [ebx]
  mov  [eax], ecx
  mov  [esp], eax

end
