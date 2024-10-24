; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
;; https://github.com/CTSRD-CHERI/llvm-project/issues/290
;; This used to have an unfilled branch-delay slot but is ow converted to a conditional move
; RUN: %cheri128_purecap_llc %s -o - | FileCheck %s '-D#CAP_SIZE=16'

; Function Attrs: norecurse nounwind readnone uwtable
define i32 addrspace(200)* @test(i32 addrspace(200)* readnone %inCap, i32 signext %test, i32 signext %inInt) local_unnamed_addr #0 {
; CHECK-LABEL: test:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cincoffset $c11, $c11, -[[#STACKFRAME_SIZE:]]
; CHECK-NEXT:    .cfi_def_cfa_offset [[#STACKFRAME_SIZE]]
; CHECK-NEXT:    csc $c24, $zero, [[#STACKFRAME_SIZE - CAP_SIZE]]($c11)
; CHECK-NEXT:    csc $c17, $zero, 0($c11)
; CHECK-NEXT:    .cfi_offset 96, -[[#STACKFRAME_SIZE - CAP_SIZE]]
; CHECK-NEXT:    .cfi_offset 89, -[[#CAP_SIZE * 2]]
; CHECK-NEXT:    cincoffset $c24, $c11, $zero
; CHECK-NEXT:    .cfi_def_cfa_register 96
; CHECK-NEXT:    addiu $1, $5, 4
; CHECK-NEXT:    dsll $1, $1, 2
; CHECK-NEXT:    cincoffset $c1, $c3, $1
; CHECK-NEXT:    cmovz $c1, $c3, $4
; CHECK-NEXT:    cmove $c3, $c1
; CHECK-NEXT:    cincoffset $c11, $c24, $zero
; CHECK-NEXT:    clc $c17, $zero, 0($c11)
; CHECK-NEXT:    clc $c24, $zero, [[#STACKFRAME_SIZE - CAP_SIZE]]($c11)
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c11, $c11, [[#STACKFRAME_SIZE]]
entry:
  %tobool = icmp eq i32 %test, 0
  %add = add nsw i32 %inInt, 4
  %idx.ext = sext i32 %add to i64
  %add.ptr = getelementptr inbounds i32, i32 addrspace(200)* %inCap, i64 %idx.ext
  %ret.0 = select i1 %tobool, i32 addrspace(200)* %inCap, i32 addrspace(200)* %add.ptr
  ret i32 addrspace(200)* %ret.0
}

attributes #0 = { norecurse nounwind readnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="cheri128" "target-features"="+cheri128,+chericap,+soft-float,-noabicalls" "unsafe-fp-math"="false" "use-soft-float"="true" }
