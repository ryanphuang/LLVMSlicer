; ModuleID = 'test/b.c'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [10 x i8] c"sum = %d\0A\00", align 1
@.str1 = private unnamed_addr constant [14 x i8] c"product = %d\0A\00", align 1
@__ai_state_ = common global i32 0, align 4
@.str2 = private unnamed_addr constant [3 x i8] c"%d\00", align 1

define void @compute(i32 %n) nounwind uwtable {
entry:
  %n.addr = alloca i32, align 4
  %i = alloca i32, align 4
  %sum = alloca i32, align 4
  %product = alloca i32, align 4
  store i32 %n, i32* %n.addr, align 4
  call void @llvm.dbg.declare(metadata !{i32* %n.addr}, metadata !20), !dbg !21
  call void @llvm.dbg.declare(metadata !{i32* %i}, metadata !22), !dbg !24
  store i32 1, i32* %i, align 4, !dbg !25
  call void @llvm.dbg.declare(metadata !{i32* %sum}, metadata !26), !dbg !27
  store i32 0, i32* %sum, align 4, !dbg !28
  call void @llvm.dbg.declare(metadata !{i32* %product}, metadata !29), !dbg !30
  store i32 1, i32* %product, align 4, !dbg !31
  br label %while.cond, !dbg !32

while.cond:                                       ; preds = %while.body, %entry
  %0 = load i32* %i, align 4, !dbg !32
  %1 = load i32* %n.addr, align 4, !dbg !32
  %cmp = icmp slt i32 %0, %1, !dbg !32
  br i1 %cmp, label %while.body, label %while.end, !dbg !32

while.body:                                       ; preds = %while.cond
  %2 = load i32* %sum, align 4, !dbg !33
  %3 = load i32* %i, align 4, !dbg !33
  %add = add nsw i32 %2, %3, !dbg !33
  store i32 %add, i32* %sum, align 4, !dbg !33
  %4 = load i32* %product, align 4, !dbg !35
  %5 = load i32* %i, align 4, !dbg !35
  %mul = mul nsw i32 %4, %5, !dbg !35
  store i32 %mul, i32* %product, align 4, !dbg !35
  %6 = load i32* %i, align 4, !dbg !36
  %add1 = add nsw i32 %6, 1, !dbg !36
  store i32 %add1, i32* %i, align 4, !dbg !36
  br label %while.cond, !dbg !37

while.end:                                        ; preds = %while.cond
  %7 = load i32* %sum, align 4, !dbg !38
  %call = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([10 x i8]* @.str, i32 0, i32 0), i32 %7), !dbg !38
  %8 = load i32* %product, align 4, !dbg !39
  %call2 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([14 x i8]* @.str1, i32 0, i32 0), i32 %8), !dbg !39
  store i32 0, i32* @__ai_state_, align 4, !dbg !40
  ret void, !dbg !41
}

declare void @llvm.dbg.declare(metadata, metadata) nounwind readnone

declare i32 @printf(i8*, ...)

define i32 @main() nounwind uwtable {
entry:
  %retval = alloca i32, align 4
  %n = alloca i32, align 4
  store i32 0, i32* %retval
  call void @llvm.dbg.declare(metadata !{i32* %n}, metadata !42), !dbg !44
  %call = call i32 (i8*, ...)* @__isoc99_scanf(i8* getelementptr inbounds ([3 x i8]* @.str2, i32 0, i32 0), i32* %n), !dbg !45
  %0 = load i32* %n, align 4, !dbg !46
  call void @compute(i32 %0), !dbg !46
  %1 = load i32* %retval, !dbg !47
  ret i32 %1, !dbg !47
}

declare i32 @__isoc99_scanf(i8*, ...)

!llvm.dbg.cu = !{!0}

!0 = metadata !{i32 720913, i32 0, i32 12, metadata !"test/b.c", metadata !"/home/ryan/LLVMSlicer", metadata !"clang version 3.0 (tags/RELEASE_30/final)", i1 true, i1 false, metadata !"", i32 0, metadata !1, metadata !1, metadata !3, metadata !17} ; [ DW_TAG_compile_unit ]
!1 = metadata !{metadata !2}
!2 = metadata !{i32 0}
!3 = metadata !{metadata !4}
!4 = metadata !{metadata !5, metadata !11}
!5 = metadata !{i32 720942, i32 0, metadata !6, metadata !"compute", metadata !"compute", metadata !"", metadata !6, i32 4, metadata !7, i1 false, i1 true, i32 0, i32 0, i32 0, i32 256, i1 false, void (i32)* @compute, null, null, metadata !9} ; [ DW_TAG_subprogram ]
!6 = metadata !{i32 720937, metadata !"test/b.c", metadata !"/home/ryan/LLVMSlicer", null} ; [ DW_TAG_file_type ]
!7 = metadata !{i32 720917, i32 0, metadata !"", i32 0, i32 0, i64 0, i64 0, i32 0, i32 0, i32 0, metadata !8, i32 0, i32 0} ; [ DW_TAG_subroutine_type ]
!8 = metadata !{null}
!9 = metadata !{metadata !10}
!10 = metadata !{i32 720932}                      ; [ DW_TAG_base_type ]
!11 = metadata !{i32 720942, i32 0, metadata !6, metadata !"main", metadata !"main", metadata !"", metadata !6, i32 19, metadata !12, i1 false, i1 true, i32 0, i32 0, i32 0, i32 0, i1 false, i32 ()* @main, null, null, metadata !15} ; [ DW_TAG_subprogram ]
!12 = metadata !{i32 720917, i32 0, metadata !"", i32 0, i32 0, i64 0, i64 0, i32 0, i32 0, i32 0, metadata !13, i32 0, i32 0} ; [ DW_TAG_subroutine_type ]
!13 = metadata !{metadata !14}
!14 = metadata !{i32 720932, null, metadata !"int", null, i32 0, i64 32, i64 32, i64 0, i32 0, i32 5} ; [ DW_TAG_base_type ]
!15 = metadata !{metadata !16}
!16 = metadata !{i32 720932}                      ; [ DW_TAG_base_type ]
!17 = metadata !{metadata !18}
!18 = metadata !{metadata !19}
!19 = metadata !{i32 720948, i32 0, null, metadata !"__ai_state_", metadata !"__ai_state_", metadata !"", metadata !6, i32 2, metadata !14, i32 0, i32 1, i32* @__ai_state_} ; [ DW_TAG_variable ]
!20 = metadata !{i32 721153, metadata !5, metadata !"n", metadata !6, i32 16777219, metadata !14, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!21 = metadata !{i32 3, i32 18, metadata !5, null}
!22 = metadata !{i32 721152, metadata !23, metadata !"i", metadata !6, i32 5, metadata !14, i32 0, i32 0} ; [ DW_TAG_auto_variable ]
!23 = metadata !{i32 720907, metadata !5, i32 4, i32 1, metadata !6, i32 0} ; [ DW_TAG_lexical_block ]
!24 = metadata !{i32 5, i32 7, metadata !23, null}
!25 = metadata !{i32 5, i32 12, metadata !23, null}
!26 = metadata !{i32 721152, metadata !23, metadata !"sum", metadata !6, i32 6, metadata !14, i32 0, i32 0} ; [ DW_TAG_auto_variable ]
!27 = metadata !{i32 6, i32 7, metadata !23, null}
!28 = metadata !{i32 6, i32 14, metadata !23, null}
!29 = metadata !{i32 721152, metadata !23, metadata !"product", metadata !6, i32 7, metadata !14, i32 0, i32 0} ; [ DW_TAG_auto_variable ]
!30 = metadata !{i32 7, i32 7, metadata !23, null}
!31 = metadata !{i32 7, i32 18, metadata !23, null}
!32 = metadata !{i32 8, i32 3, metadata !23, null}
!33 = metadata !{i32 9, i32 5, metadata !34, null}
!34 = metadata !{i32 720907, metadata !23, i32 8, i32 17, metadata !6, i32 1} ; [ DW_TAG_lexical_block ]
!35 = metadata !{i32 10, i32 5, metadata !34, null}
!36 = metadata !{i32 11, i32 5, metadata !34, null}
!37 = metadata !{i32 12, i32 3, metadata !34, null}
!38 = metadata !{i32 13, i32 3, metadata !23, null}
!39 = metadata !{i32 14, i32 3, metadata !23, null}
!40 = metadata !{i32 15, i32 3, metadata !23, null}
!41 = metadata !{i32 16, i32 1, metadata !23, null}
!42 = metadata !{i32 721152, metadata !43, metadata !"n", metadata !6, i32 20, metadata !14, i32 0, i32 0} ; [ DW_TAG_auto_variable ]
!43 = metadata !{i32 720907, metadata !11, i32 19, i32 1, metadata !6, i32 2} ; [ DW_TAG_lexical_block ]
!44 = metadata !{i32 20, i32 7, metadata !43, null}
!45 = metadata !{i32 21, i32 3, metadata !43, null}
!46 = metadata !{i32 22, i32 3, metadata !43, null}
!47 = metadata !{i32 23, i32 1, metadata !43, null}
