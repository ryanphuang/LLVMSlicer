; ModuleID = 'linked.c'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.list = type { i32, %struct.list* }

@.str = private unnamed_addr constant [16 x i8] c"l->next->n != 0\00", align 1
@.str1 = private unnamed_addr constant [9 x i8] c"linked.c\00", align 1
@__PRETTY_FUNCTION__.main = private unnamed_addr constant [11 x i8] c"int main()\00", align 1

define i32 @main() nounwind uwtable {
entry:
  %retval = alloca i32, align 4
  %l = alloca %struct.list*, align 8
  %m = alloca %struct.list, align 8
  store i32 0, i32* %retval
  %0 = bitcast %struct.list* %m to i8*
  call void @llvm.memset.p0i8.i64(i8* %0, i8 0, i64 16, i32 8, i1 false)
  %call = call %struct.list* @append(%struct.list* %m, i32 1)
  store %struct.list* %call, %struct.list** %l, align 8
  %1 = load %struct.list** %l, align 8
  %next = getelementptr inbounds %struct.list* %1, i32 0, i32 1
  %2 = load %struct.list** %next, align 8
  %n = getelementptr inbounds %struct.list* %2, i32 0, i32 0
  %3 = load i32* %n, align 4
  %cmp = icmp ne i32 %3, 0
  br i1 %cmp, label %cond.true, label %cond.false

cond.true:                                        ; preds = %entry
  br label %cond.end

cond.false:                                       ; preds = %entry
  call void @__assert_fail(i8* getelementptr inbounds ([16 x i8]* @.str, i32 0, i32 0), i8* getelementptr inbounds ([9 x i8]* @.str1, i32 0, i32 0), i32 28, i8* getelementptr inbounds ([11 x i8]* @__PRETTY_FUNCTION__.main, i32 0, i32 0)) noreturn nounwind
  unreachable
                                                  ; No predecessors!
  br label %cond.end

cond.end:                                         ; preds = %4, %cond.true
  ret i32 0
}

declare void @llvm.memset.p0i8.i64(i8* nocapture, i8, i64, i32, i1) nounwind

define internal %struct.list* @append(%struct.list* %l, i32 %n) nounwind uwtable {
entry:
  %l.addr = alloca %struct.list*, align 8
  %n.addr = alloca i32, align 4
  %new_el = alloca %struct.list*, align 8
  store %struct.list* %l, %struct.list** %l.addr, align 8
  store i32 %n, i32* %n.addr, align 4
  %call = call noalias i8* @malloc(i64 16) nounwind
  %0 = bitcast i8* %call to %struct.list*
  store %struct.list* %0, %struct.list** %new_el, align 8
  %1 = load %struct.list** %l.addr, align 8
  %2 = load %struct.list** %new_el, align 8
  %next = getelementptr inbounds %struct.list* %2, i32 0, i32 1
  store %struct.list* %1, %struct.list** %next, align 8
  %3 = load i32* %n.addr, align 4
  %4 = load %struct.list** %new_el, align 8
  %n1 = getelementptr inbounds %struct.list* %4, i32 0, i32 0
  store i32 %3, i32* %n1, align 4
  %5 = load %struct.list** %new_el, align 8
  ret %struct.list* %5
}

declare void @__assert_fail(i8*, i8*, i32, i8*) noreturn nounwind

declare noalias i8* @malloc(i64) nounwind
