; ModuleID = 'elena'
source_filename = "elena"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@elena.vmt.system.LiteralValue = weak global i64 0
@elena.k.02.000001.image = private global { i64, ptr, [24 x i8] } { i64 -24, ptr @elena.vmt.system.LiteralValue, [24 x i8] c"Hello World from ELENA!\00" }

@elena.k.02.000001 = internal alias [24 x i8], getelementptr inbounds ({ i64, ptr, [24 x i8] }, ptr @elena.k.02.000001.image, i32 0, i32 2)

declare ptr @elena_send_vi(ptr, i64, ptr, i64)

declare ptr @elena_bsredirect(ptr, i64, ptr, ptr)

declare ptr @elena_new(ptr, i64)

declare ptr @elena_newbinary(ptr, i64)

declare void @elena_setfield(ptr, i64, ptr)

declare void @elena_throw(ptr)

declare void @elena_hook_push(ptr)

declare void @elena_unhook()

declare ptr @elena_current_exception()

; Function Attrs: returns_twice
declare i32 @_setjmp(ptr) #0

declare i64 @elena_length(ptr, i64)

declare void @elena_validate(ptr)

declare i64 @elena_trylock(ptr)

declare void @elena_freelock(ptr)

declare i64 @elena_external_stub()

define ptr @elena.sym.helloworld.program(ptr %0, i64 %1, ptr %2) {
entry:
  %A = alloca ptr, align 8
  %B = alloca ptr, align 8
  %D = alloca i64, align 8
  %E = alloca i64, align 8
  %F = alloca double, align 8
  %cells = alloca [96 x i64], align 8
  %found = alloca i8, align 1
  store ptr %0, ptr %A, align 8
  store ptr null, ptr %B, align 8
  store i64 0, ptr %D, align 8
  store i64 %1, ptr %E, align 8
  %3 = getelementptr inbounds [96 x i64], ptr %cells, i64 0, i64 95
  store i64 0, ptr %3, align 8
  br label %4

4:                                                ; preds = %entry
  store ptr @elena.k.02.000001, ptr %A, align 8
  %5 = load ptr, ptr %A, align 8
  ret ptr %5
}

define ptr @elena_program() {
entry:
  %0 = call ptr @elena.sym.helloworld.program(ptr null, i64 0, ptr null)
  ret ptr %0
}

declare void @elena_unimplemented(ptr)

attributes #0 = { returns_twice }
