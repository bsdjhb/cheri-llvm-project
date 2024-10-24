; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: %cheri128_purecap_llc %s -o - | FileCheck %s '-D#CAP_SIZE=16'
; ModuleID = 'bit.c'
; Test that we can correctly legalise i128 and generate pointer arithmetic that
; doesn't crash the compiler.
source_filename = "bit.c"
target datalayout = "E-m:e-pf200:256:256-i8:8:32-i16:16:32-i64:64-n32:64-S128-A200"
target triple = "cheri-unknown-freebsd"

%struct.foo = type { i128 }

@x = internal addrspace(200) global %struct.foo zeroinitializer, align 4

; Function Attrs: noinline nounwind
define i32 @main(i32 signext %argc, i8 addrspace(200)* addrspace(200)* %argv) #0 {
; CHECK-LABEL: main:
; CHECK:       # %bb.0: # %entry
; CHECK-NEXT:    cincoffset $c11, $c11, -[[STACKFRAME_SIZE:64|128]]
; CHECK-NEXT:    csc $c24, $zero, [[#CAP_SIZE * 3]]($c11)
; CHECK-NEXT:    csc $c17, $zero, [[#CAP_SIZE * 2]]($c11)
; CHECK-NEXT:    cincoffset $c24, $c11, $zero
; CHECK-NEXT:    cgetaddr $1, $c11
; CHECK-NEXT:    daddiu $2, $zero, -32
; CHECK-NEXT:    and $1, $1, $2
; CHECK-NEXT:    csetaddr $c11, $c11, $1
; CHECK-NEXT:    lui $1, %pcrel_hi(_CHERI_CAPABILITY_TABLE_-8)
; CHECK-NEXT:    daddiu $1, $1, %pcrel_lo(_CHERI_CAPABILITY_TABLE_-4)
; CHECK-NEXT:    cgetpccincoffset $c1, $1
; CHECK-NEXT:    csw $zero, $zero, 28($c11)
; CHECK-NEXT:    clcbi $c1, %captab20(x)($c1)
; CHECK-NEXT:    csw $4, $zero, 24($c11)
; CHECK-NEXT:    csc $c3, $zero, 0($c11)
; CHECK-NEXT:    cld $1, $zero, 8($c1)
; CHECK-NEXT:    lui $2, 65535
; CHECK-NEXT:    ori $2, $2, 65535
; CHECK-NEXT:    dsll $2, $2, 32
; CHECK-NEXT:    daddiu $2, $2, 1
; CHECK-NEXT:    and $1, $1, $2
; CHECK-NEXT:    ori $1, $1, 2
; CHECK-NEXT:    addiu $2, $zero, 0
; CHECK-NEXT:    csd $1, $zero, 8($c1)
; CHECK-NEXT:    cincoffset $c11, $c24, $zero
; CHECK-NEXT:    clc $c17, $zero, [[#CAP_SIZE * 2]]($c11)
; CHECK-NEXT:    clc $c24, $zero, [[#CAP_SIZE * 3]]($c11)
; CHECK-NEXT:    cjr $c17
; CHECK-NEXT:    cincoffset $c11, $c11, [[STACKFRAME_SIZE]]
entry:
  %retval = alloca i32, align 4, addrspace(200)
  %argc.addr = alloca i32, align 4, addrspace(200)
  %argv.addr = alloca i8 addrspace(200)* addrspace(200)*, align 32, addrspace(200)
  store i32 0, i32 addrspace(200)* %retval, align 4
  store i32 %argc, i32 addrspace(200)* %argc.addr, align 4
  store i8 addrspace(200)* addrspace(200)* %argv, i8 addrspace(200)* addrspace(200)* addrspace(200)* %argv.addr, align 32
  %bf.load = load i128, i128 addrspace(200)* getelementptr inbounds (%struct.foo, %struct.foo addrspace(200)* @x, i32 0, i32 0), align 4
  %bf.clear = and i128 %bf.load, -4294967295
  %bf.set = or i128 %bf.clear, 2
  store i128 %bf.set, i128 addrspace(200)* getelementptr inbounds (%struct.foo, %struct.foo addrspace(200)* @x, i32 0, i32 0), align 4
  ret i32 0
}

attributes #0 = { noinline nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-features"="+cheri" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"PIC Level", i32 2}
!1 = !{!"clang version 5.0.0 (ssh://dc552@vica.cl.cam.ac.uk:/home/dc552/CHERISDK/llvm/tools/clang 36fb1dc82f2552b06fa268eab5513cf52c16f41b)"}
