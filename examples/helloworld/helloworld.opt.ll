; ModuleID = 'elena'
source_filename = "elena"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@elena.vmt.system.LiteralValue = weak global i64 0
@elena.k.02.000001.image = private global { i64, ptr, [24 x i8] } { i64 -24, ptr @elena.vmt.system.LiteralValue, [24 x i8] c"Hello World from ELENA!\00" }

@elena.k.02.000001 = internal alias [24 x i8], getelementptr inbounds ({ i64, ptr, [24 x i8] }, ptr @elena.k.02.000001.image, i32 0, i32 2)

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none)
define nonnull ptr @elena.sym.helloworld.program(ptr readnone captures(none) %0, i64 %1, ptr readnone captures(none) %2) local_unnamed_addr #0 {
entry:
  ret ptr @elena.k.02.000001
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none)
define nonnull ptr @elena_program() local_unnamed_addr #0 {
entry:
  ret ptr @elena.k.02.000001
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) }
