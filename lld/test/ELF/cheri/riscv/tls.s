# REQUIRES: riscv
# RUN: echo '.tbss; .globl evar; evar: .zero 4' > %t.s

# RUN: %riscv32_cheri_purecap_llvm-mc -filetype=obj %t.s -o %t1.32.o
# RUN: ld.lld -shared -soname=t1.so %t1.32.o -o %t1.32.so

# RUN: %riscv32_cheri_purecap_llvm-mc --defsym PIC=0 -filetype=obj %s -o %t.32.o
# RUN: ld.lld %t.32.o %t1.32.so -o %t.32
# RUN: llvm-readobj -r %t.32 | FileCheck --check-prefix=RV32-REL %s
# RUN: llvm-readelf -x .captable %t.32 | FileCheck --check-prefix=RV32-CAP %s
# RUN: llvm-objdump -d --no-show-raw-insn %t.32 | FileCheck --check-prefix=RV32-DIS %s

# RUN: %riscv32_cheri_purecap_llvm-mc --defsym PIC=1 -filetype=obj %s -o %t.32.pico
# RUN: ld.lld -shared %t.32.pico %t1.32.so -o %t.32.so
# RUN: llvm-readobj -r %t.32.so | FileCheck --check-prefix=RV32-SO-REL %s
# RUN: llvm-readelf -x .captable %t.32.so | FileCheck --check-prefix=RV32-SO-CAP %s
# RUN: llvm-objdump -d --no-show-raw-insn %t.32.so | FileCheck --check-prefix=RV32-SO-DIS %s

# RUN: %riscv64_cheri_purecap_llvm-mc -filetype=obj %t.s -o %t1.64.o
# RUN: ld.lld -shared -soname=t1.so %t1.64.o -o %t1.64.so

# RUN: %riscv64_cheri_purecap_llvm-mc --defsym PIC=0 -filetype=obj %s -o %t.64.o
# RUN: ld.lld %t.64.o %t1.64.so -o %t.64
# RUN: llvm-readobj -r %t.64 | FileCheck --check-prefix=RV64-REL %s
# RUN: llvm-readelf -x .captable %t.64 | FileCheck --check-prefix=RV64-CAP %s
# RUN: llvm-objdump -d --no-show-raw-insn %t.64 | FileCheck --check-prefix=RV64-DIS %s

# RUN: %riscv64_cheri_purecap_llvm-mc --defsym PIC=1 -filetype=obj %s -o %t.64.pico
# RUN: ld.lld -shared %t.64.pico %t1.64.so -o %t.64.so
# RUN: llvm-readobj -r %t.64.so | FileCheck --check-prefix=RV64-SO-REL %s
# RUN: llvm-readelf -x .captable %t.64.so | FileCheck --check-prefix=RV64-SO-CAP %s
# RUN: llvm-objdump -d --no-show-raw-insn %t.64.so | FileCheck --check-prefix=RV64-SO-DIS %s

# RV32-REL:      .rela.dyn {
# RV32-REL-NEXT:   0x12230 R_RISCV_TLS_DTPMOD32 evar 0x0
# RV32-REL-NEXT:   0x12234 R_RISCV_TLS_DTPREL32 evar 0x0
# RV32-REL-NEXT:   0x12240 R_RISCV_TLS_TPREL32 evar 0x0
# RV32-REL-NEXT: }

# RV32-SO-REL:      .rela.dyn {
# RV32-SO-REL-NEXT:   0x3480 R_RISCV_TLS_DTPMOD32 - 0x0
# RV32-SO-REL-NEXT:   0x348C R_RISCV_TLS_TPREL32 - 0x4
# RV32-SO-REL-NEXT:   0x3478 R_RISCV_TLS_DTPMOD32 evar 0x0
# RV32-SO-REL-NEXT:   0x347C R_RISCV_TLS_DTPREL32 evar 0x0
# RV32-SO-REL-NEXT:   0x3488 R_RISCV_TLS_TPREL32 evar 0x0
# RV32-SO-REL-NEXT: }

# RV32-CAP: section '.captable':
# RV32-CAP-NEXT: 0x00012230 00000000 00000000 01000000 04000000
# RV32-CAP-NEXT: 0x00012240 00000000 04000000

# RV32-SO-CAP: section '.captable':
# RV32-SO-CAP-NEXT: 0x00003478 00000000 00000000 00000000 04000000
# RV32-SO-CAP-NEXT: 0x00003488 00000000 00000000

# 0x121e0 - 0x111b4 = 0x0102c (GD evar)
# RV32-DIS:      11200: auipcc ca0, 1
# RV32-DIS-NEXT:        cincoffset ca0, ca0, 48

# 0x121f0 - 0x111bc = 0x01034 (IE evar)
# RV32-DIS:      11208: auipcc ca0, 1
# RV32-DIS-NEXT:        clw a0, 56(ca0)

# 0x121e8 - 0x111c4 = 0x01024 (GD lvar)
# RV32-DIS:      11210: auipcc ca0, 1
# RV32-DIS-NEXT:        cincoffset ca0, ca0, 40

# 0x121f4 - 0x111cc = 0x01028 (IE lvar)
# RV32-DIS:      11218: auipcc ca0, 1
# RV32-DIS-NEXT:        clw a0, 44(ca0)

# RV32-DIS:      11220: lui a0, 0
# RV32-DIS-NEXT:        cincoffset ca0, ctp, a0
# RV32-DIS-NEXT:        cincoffset ca0, ca0, 4

# 0x3288 - 0x1210 = 0x2078 (GD evar)
# RV32-SO-DIS:      1400: auipcc ca0, 2
# RV32-SO-DIS-NEXT:       cincoffset ca0, ca0, 120

# 0x3298 - 0x1218 = 0x2080 (IE evar)
# RV32-SO-DIS:      1408: auipcc ca0, 2
# RV32-SO-DIS-NEXT:       clw a0, 128(ca0)

# 0x3290 - 0x1220 = 0x2070 (GD lvar)
# RV32-SO-DIS:      1410: auipcc ca0, 2
# RV32-SO-DIS-NEXT:       cincoffset ca0, ca0, 112

# 0x329c - 0x1228 = 0x2074 (IE lvar)
# RV32-SO-DIS:      1418: auipcc ca0, 2
# RV32-SO-DIS-NEXT:       clw a0, 116(ca0)

# RV64-REL:      .rela.dyn {
# RV64-REL-NEXT:   0x12320 R_RISCV_TLS_DTPMOD64 evar 0x0
# RV64-REL-NEXT:   0x12328 R_RISCV_TLS_DTPREL64 evar 0x0
# RV64-REL-NEXT:   0x12340 R_RISCV_TLS_TPREL64 evar 0x0
# RV64-REL-NEXT: }

# RV64-SO-REL:      .rela.dyn {
# RV64-SO-REL-NEXT:   0x3470 R_RISCV_TLS_DTPMOD64 - 0x0
# RV64-SO-REL-NEXT:   0x3488 R_RISCV_TLS_TPREL64 - 0x4
# RV64-SO-REL-NEXT:   0x3460 R_RISCV_TLS_DTPMOD64 evar 0x0
# RV64-SO-REL-NEXT:   0x3468 R_RISCV_TLS_DTPREL64 evar 0x0
# RV64-SO-REL-NEXT:   0x3480 R_RISCV_TLS_TPREL64 evar 0x0
# RV64-SO-REL-NEXT: }

# RV64-CAP: section '.captable':
# RV64-CAP-NEXT: 0x00012320 00000000 00000000 00000000 00000000
# RV64-CAP-NEXT: 0x00012330 01000000 00000000 04000000 00000000
# RV64-CAP-NEXT: 0x00012340 00000000 00000000 04000000 00000000

# RV64-SO-CAP: section '.captable':
# RV64-SO-CAP-NEXT: 0x00003460 00000000 00000000 00000000 00000000
# RV64-SO-CAP-NEXT: 0x00003470 00000000 00000000 04000000 00000000
# RV64-SO-CAP-NEXT: 0x00003480 00000000 00000000 00000000 00000000

# 0x122f0 - 0x112b8 = 0x01038 (GD evar)
# RV64-DIS:      112f0: auipcc ca0, 1
# RV64-DIS-NEXT:        cincoffset ca0, ca0, 48

# 0x12310 - 0x112c0 = 0x01050 (IE evar)
# RV64-DIS:      112f8: auipcc ca0, 1
# RV64-DIS-NEXT:        cld a0, 72(ca0)

# 0x12300 - 0x112c8 = 0x01038 (GD lvar)
# RV64-DIS:      11300: auipcc ca0, 1
# RV64-DIS-NEXT:        cincoffset ca0, ca0, 48

# 0x12318 - 0x112d0 = 0x01048 (IE lvar)
# RV64-DIS:      11308: auipcc ca0, 1
# RV64-DIS-NEXT:        cld a0, 64(ca0)

# RV64-DIS:      11310: lui a0, 0
# RV64-DIS-NEXT:        cincoffset ca0, ctp, a0
# RV64-DIS-NEXT:        cincoffset ca0, ca0, 4

# 0x3420 - 0x1350 = 0x20d0 (GD evar)
# RV64-SO-DIS:      1390: auipcc ca0, 2
# RV64-SO-DIS-NEXT:       cincoffset ca0, ca0, 208

# 0x3440 - 0x1358 = 0x20e8 (IE evar)
# RV64-SO-DIS:      1398: auipcc ca0, 2
# RV64-SO-DIS-NEXT:       cld a0, 232(ca0)

# 0x3430 - 0x1360 = 0x20d0 (GD lvar)
# RV64-SO-DIS:      13a0: auipcc ca0, 2
# RV64-SO-DIS-NEXT:       cincoffset ca0, ca0, 208

# 0x3448 - 0x1368 = 0x20e0 (IE lvar)
# RV64-SO-DIS:      13a8: auipcc ca0, 2
# RV64-SO-DIS-NEXT:       cld a0, 224(ca0)

.global _start
_start:
	clc.tls.gd ca0, evar

	cla.tls.ie a0, evar, ca0

	clc.tls.gd ca0, lvar

	cla.tls.ie a0, lvar, ca0

.if PIC == 0
	lui a0, %tprel_hi(lvar)
	cincoffset ca0, ctp, a0, %tprel_cincoffset(lvar)
	cincoffset ca0, ca0, %tprel_lo(lvar)
.endif

.tbss
	.zero 4
lvar:
	.zero 4
