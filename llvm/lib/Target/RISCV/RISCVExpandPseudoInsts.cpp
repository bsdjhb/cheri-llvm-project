//===-- RISCVExpandPseudoInsts.cpp - Expand pseudo instructions -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands pseudo instructions into target
// instructions. This pass should be run after register allocation but before
// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVTargetMachine.h"

#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define RISCV_EXPAND_PSEUDO_NAME "RISCV pseudo instruction expansion pass"

namespace {

class RISCVExpandPseudo : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVExpandPseudo() : MachineFunctionPass(ID) {
    initializeRISCVExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return RISCV_EXPAND_PSEUDO_NAME; }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandAtomicBinOp(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI, AtomicRMWInst::BinOp,
                         bool IsMasked, int Width, bool PtrIsCap,
                         MachineBasicBlock::iterator &NextMBBI);
  bool expandAtomicMinMaxOp(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MBBI,
                            AtomicRMWInst::BinOp, bool IsMasked, int Width,
                            bool PtrIsCap,
                            MachineBasicBlock::iterator &NextMBBI);
  bool expandAtomicCmpXchg(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI, bool IsMasked,
                           int Width, bool PtrIsCap,
                           MachineBasicBlock::iterator &NextMBBI);
  bool expandAtomicCmpXchgCap(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI, bool PtrIsCap,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandAuipcInstPair(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI,
                           MachineBasicBlock::iterator &NextMBBI,
                           unsigned FlagsHi, unsigned SecondOpcode);
  bool expandLoadLocalAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadAddress(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI,
                         MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadTLSIEAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadTLSGDAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);

  bool expandAuipccInstPair(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MBBI,
                            MachineBasicBlock::iterator &NextMBBI,
                            unsigned FlagsHi, unsigned SecondOpcode);
  bool expandCapLoadGlobalCap(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandCapLoadTLSIEAddress(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 MachineBasicBlock::iterator &NextMBBI);
  bool expandCapLoadTLSGDCap(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MBBI,
                             MachineBasicBlock::iterator &NextMBBI);
};

char RISCVExpandPseudo::ID = 0;

bool RISCVExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);
  return Modified;
}

bool RISCVExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool RISCVExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 MachineBasicBlock::iterator &NextMBBI) {
  switch (MBBI->getOpcode()) {
  case RISCV::PseudoAtomicLoadNand32:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Nand, false, 32, false,
                             NextMBBI);
  case RISCV::PseudoAtomicLoadNand64:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Nand, false, 64, false,
                             NextMBBI);
  case RISCV::PseudoMaskedAtomicSwap32:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Xchg, true, 32, false,
                             NextMBBI);
  case RISCV::PseudoMaskedAtomicLoadAdd32:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Add, true, 32, false,
                             NextMBBI);
  case RISCV::PseudoMaskedAtomicLoadSub32:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Sub, true, 32, false,
                             NextMBBI);
  case RISCV::PseudoMaskedAtomicLoadNand32:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Nand, true, 32, false,
                             NextMBBI);
  case RISCV::PseudoMaskedAtomicLoadMax32:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::Max, true, 32, false,
                                NextMBBI);
  case RISCV::PseudoMaskedAtomicLoadMin32:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::Min, true, 32, false,
                                NextMBBI);
  case RISCV::PseudoMaskedAtomicLoadUMax32:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::UMax, true, 32, false,
                                NextMBBI);
  case RISCV::PseudoMaskedAtomicLoadUMin32:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::UMin, true, 32, false,
                                NextMBBI);
  case RISCV::PseudoCmpXchg32:
    return expandAtomicCmpXchg(MBB, MBBI, false, 32, false, NextMBBI);
  case RISCV::PseudoCmpXchg64:
    return expandAtomicCmpXchg(MBB, MBBI, false, 64, false, NextMBBI);
  case RISCV::PseudoMaskedCmpXchg32:
    return expandAtomicCmpXchg(MBB, MBBI, true, 32, false, NextMBBI);
  case RISCV::PseudoLLA:
    return expandLoadLocalAddress(MBB, MBBI, NextMBBI);
  case RISCV::PseudoLA:
    return expandLoadAddress(MBB, MBBI, NextMBBI);
  case RISCV::PseudoLA_TLS_IE:
    return expandLoadTLSIEAddress(MBB, MBBI, NextMBBI);
  case RISCV::PseudoLA_TLS_GD:
    return expandLoadTLSGDAddress(MBB, MBBI, NextMBBI);
  case RISCV::PseudoCLGC:
    return expandCapLoadGlobalCap(MBB, MBBI, NextMBBI);
  case RISCV::PseudoCLA_TLS_IE:
    return expandCapLoadTLSIEAddress(MBB, MBBI, NextMBBI);
  case RISCV::PseudoCLC_TLS_GD:
    return expandCapLoadTLSGDCap(MBB, MBBI, NextMBBI);
  case RISCV::PseudoCmpXchgCap:
    return expandAtomicCmpXchgCap(MBB, MBBI, false, NextMBBI);
  case RISCV::PseudoCheriAtomicSwap8:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Xchg, false, 8, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicSwap16:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Xchg, false, 16, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadAdd8:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Add, false, 8, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadAdd16:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Add, false, 16, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadAnd8:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::And, false, 8, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadAnd16:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::And, false, 16, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadOr8:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Or, false, 8, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadOr16:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Or, false, 16, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadXor8:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Xor, false, 8, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadXor16:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Xor, false, 16, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadNand8:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Nand, false, 8, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadNand16:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Nand, false, 16, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadSub8:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Sub, false, 8, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadSub16:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Sub, false, 16, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadNand32:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Nand, false, 32, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadNand64:
    return expandAtomicBinOp(MBB, MBBI, AtomicRMWInst::Nand, false, 64, true,
                             NextMBBI);
  case RISCV::PseudoCheriAtomicLoadMax8:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::Max, false, 8, true,
                                NextMBBI);
  case RISCV::PseudoCheriAtomicLoadMax16:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::Max, false, 16, true,
                                NextMBBI);
  case RISCV::PseudoCheriAtomicLoadMin8:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::Min, false, 8, true,
                                NextMBBI);
  case RISCV::PseudoCheriAtomicLoadMin16:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::Min, false, 16, true,
                                NextMBBI);
  case RISCV::PseudoCheriAtomicLoadUMax8:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::UMax, false, 8, true,
                                NextMBBI);
  case RISCV::PseudoCheriAtomicLoadUMax16:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::UMax, false, 16, true,
                                NextMBBI);
  case RISCV::PseudoCheriAtomicLoadUMin8:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::UMin, false, 8, true,
                                NextMBBI);
  case RISCV::PseudoCheriAtomicLoadUMin16:
    return expandAtomicMinMaxOp(MBB, MBBI, AtomicRMWInst::UMin, false, 16, true,
                                NextMBBI);
  case RISCV::PseudoCheriCmpXchg8:
    return expandAtomicCmpXchg(MBB, MBBI, false, 8, true, NextMBBI);
  case RISCV::PseudoCheriCmpXchg16:
    return expandAtomicCmpXchg(MBB, MBBI, false, 16, true, NextMBBI);
  case RISCV::PseudoCheriCmpXchg32:
    return expandAtomicCmpXchg(MBB, MBBI, false, 32, true, NextMBBI);
  case RISCV::PseudoCheriCmpXchg64:
    return expandAtomicCmpXchg(MBB, MBBI, false, 64, true, NextMBBI);
  case RISCV::PseudoCheriCmpXchgCap:
    return expandAtomicCmpXchgCap(MBB, MBBI, true, NextMBBI);
  }

  return false;
}

static unsigned getLRForRMW8(bool PtrIsCap, AtomicOrdering Ordering) {
  assert(PtrIsCap);
  switch (Ordering) {
  default:
    llvm_unreachable("Unexpected AtomicOrdering");
  case AtomicOrdering::Monotonic:
    return RISCV::CLR_B;
  case AtomicOrdering::Acquire:
    return RISCV::CLR_B_AQ;
  case AtomicOrdering::Release:
    return RISCV::CLR_B;
  case AtomicOrdering::AcquireRelease:
    return RISCV::CLR_B_AQ;
  case AtomicOrdering::SequentiallyConsistent:
    return RISCV::CLR_B_AQ_RL;
  }
}

static unsigned getSCForRMW8(bool PtrIsCap, AtomicOrdering Ordering) {
  assert(PtrIsCap);
  switch (Ordering) {
  default:
    llvm_unreachable("Unexpected AtomicOrdering");
  case AtomicOrdering::Monotonic:
    return RISCV::CSC_B;
  case AtomicOrdering::Acquire:
    return RISCV::CSC_B;
  case AtomicOrdering::Release:
    return RISCV::CSC_B_RL;
  case AtomicOrdering::AcquireRelease:
    return RISCV::CSC_B_RL;
  case AtomicOrdering::SequentiallyConsistent:
    return RISCV::CSC_B_AQ_RL;
  }
}

static unsigned getLRForRMW16(bool PtrIsCap, AtomicOrdering Ordering) {
  assert(PtrIsCap);
  switch (Ordering) {
  default:
    llvm_unreachable("Unexpected AtomicOrdering");
  case AtomicOrdering::Monotonic:
    return RISCV::CLR_H;
  case AtomicOrdering::Acquire:
    return RISCV::CLR_H_AQ;
  case AtomicOrdering::Release:
    return RISCV::CLR_H;
  case AtomicOrdering::AcquireRelease:
    return RISCV::CLR_H_AQ;
  case AtomicOrdering::SequentiallyConsistent:
    return RISCV::CLR_H_AQ_RL;
  }
}

static unsigned getSCForRMW16(bool PtrIsCap, AtomicOrdering Ordering) {
  assert(PtrIsCap);
  switch (Ordering) {
  default:
    llvm_unreachable("Unexpected AtomicOrdering");
  case AtomicOrdering::Monotonic:
    return RISCV::CSC_H;
  case AtomicOrdering::Acquire:
    return RISCV::CSC_H;
  case AtomicOrdering::Release:
    return RISCV::CSC_H_RL;
  case AtomicOrdering::AcquireRelease:
    return RISCV::CSC_H_RL;
  case AtomicOrdering::SequentiallyConsistent:
    return RISCV::CSC_H_AQ_RL;
  }
}

static unsigned getLRForRMW32(bool PtrIsCap, AtomicOrdering Ordering) {
  switch (Ordering) {
  default:
    llvm_unreachable("Unexpected AtomicOrdering");
  case AtomicOrdering::Monotonic:
    return PtrIsCap ? RISCV::CLR_W : RISCV::LR_W;
  case AtomicOrdering::Acquire:
    return PtrIsCap ? RISCV::CLR_W_AQ : RISCV::LR_W_AQ;
  case AtomicOrdering::Release:
    return PtrIsCap ? RISCV::CLR_W : RISCV::LR_W;
  case AtomicOrdering::AcquireRelease:
    return PtrIsCap ? RISCV::CLR_W_AQ : RISCV::LR_W_AQ;
  case AtomicOrdering::SequentiallyConsistent:
    return PtrIsCap ? RISCV::CLR_W_AQ_RL : RISCV::LR_W_AQ_RL;
  }
}

static unsigned getSCForRMW32(bool PtrIsCap, AtomicOrdering Ordering) {
  switch (Ordering) {
  default:
    llvm_unreachable("Unexpected AtomicOrdering");
  case AtomicOrdering::Monotonic:
    return PtrIsCap ? RISCV::CSC_W : RISCV::SC_W;
  case AtomicOrdering::Acquire:
    return PtrIsCap ? RISCV::CSC_W : RISCV::SC_W;
  case AtomicOrdering::Release:
    return PtrIsCap ? RISCV::CSC_W_RL : RISCV::SC_W_RL;
  case AtomicOrdering::AcquireRelease:
    return PtrIsCap ? RISCV::CSC_W_RL : RISCV::SC_W_RL;
  case AtomicOrdering::SequentiallyConsistent:
    return PtrIsCap ? RISCV::CSC_W_AQ_RL : RISCV::SC_W_AQ_RL;
  }
}

static unsigned getLRForRMW64(bool PtrIsCap, AtomicOrdering Ordering) {
  switch (Ordering) {
  default:
    llvm_unreachable("Unexpected AtomicOrdering");
  case AtomicOrdering::Monotonic:
    return PtrIsCap ? RISCV::CLR_D : RISCV::LR_D;
  case AtomicOrdering::Acquire:
    return PtrIsCap ? RISCV::CLR_D_AQ : RISCV::LR_D_AQ;
  case AtomicOrdering::Release:
    return PtrIsCap ? RISCV::CLR_D : RISCV::LR_D;
  case AtomicOrdering::AcquireRelease:
    return PtrIsCap ? RISCV::CLR_D_AQ : RISCV::LR_D_AQ;
  case AtomicOrdering::SequentiallyConsistent:
    return PtrIsCap ? RISCV::CLR_D_AQ_RL : RISCV::LR_D_AQ_RL;
  }
}

static unsigned getSCForRMW64(bool PtrIsCap, AtomicOrdering Ordering) {
  switch (Ordering) {
  default:
    llvm_unreachable("Unexpected AtomicOrdering");
  case AtomicOrdering::Monotonic:
    return PtrIsCap ? RISCV::CSC_D : RISCV::SC_D;
  case AtomicOrdering::Acquire:
    return PtrIsCap ? RISCV::CSC_D : RISCV::SC_D;
  case AtomicOrdering::Release:
    return PtrIsCap ? RISCV::CSC_D_RL : RISCV::SC_D_RL;
  case AtomicOrdering::AcquireRelease:
    return PtrIsCap ? RISCV::CSC_D_RL : RISCV::SC_D_RL;
  case AtomicOrdering::SequentiallyConsistent:
    return PtrIsCap ? RISCV::CSC_D_AQ_RL : RISCV::SC_D_AQ_RL;
  }
}

static unsigned getLRForRMW(bool PtrIsCap, AtomicOrdering Ordering, int Width) {
  if (Width == 8)
    return getLRForRMW8(PtrIsCap, Ordering);
  if (Width == 16)
    return getLRForRMW16(PtrIsCap, Ordering);
  if (Width == 32)
    return getLRForRMW32(PtrIsCap, Ordering);
  if (Width == 64)
    return getLRForRMW64(PtrIsCap, Ordering);
  llvm_unreachable("Unexpected LR width\n");
}

static unsigned getSCForRMW(bool PtrIsCap, AtomicOrdering Ordering, int Width) {
  if (Width == 8)
    return getSCForRMW8(PtrIsCap, Ordering);
  if (Width == 16)
    return getSCForRMW16(PtrIsCap, Ordering);
  if (Width == 32)
    return getSCForRMW32(PtrIsCap, Ordering);
  if (Width == 64)
    return getSCForRMW64(PtrIsCap, Ordering);
  llvm_unreachable("Unexpected SC width\n");
}

static unsigned getLRForRMWCap(bool PtrIsCap, AtomicOrdering Ordering, int CLen) {
  switch (Ordering) {
  default:
    llvm_unreachable("Unexpected AtomicOrdering");
  case AtomicOrdering::Monotonic:
    return CLen == 64 ? (PtrIsCap ? RISCV::CLR_C_64 : RISCV::LR_C_64)
                      : (PtrIsCap ? RISCV::CLR_C_128 : RISCV::LR_C_128);
  case AtomicOrdering::Acquire:
    return CLen == 64 ? (PtrIsCap ? RISCV::CLR_C_AQ_64 : RISCV::LR_C_AQ_64)
                      : (PtrIsCap ? RISCV::CLR_C_AQ_128 : RISCV::LR_C_AQ_128);
  case AtomicOrdering::Release:
    return CLen == 64 ? (PtrIsCap ? RISCV::CLR_C_64 : RISCV::LR_C_64)
                      : (PtrIsCap ? RISCV::CLR_C_128 : RISCV::LR_C_128);
  case AtomicOrdering::AcquireRelease:
    return CLen == 64 ? (PtrIsCap ? RISCV::CLR_C_AQ_64 : RISCV::LR_C_AQ_64)
                      : (PtrIsCap ? RISCV::CLR_C_AQ_128 : RISCV::LR_C_AQ_128);
  case AtomicOrdering::SequentiallyConsistent:
    return CLen == 64 ? (PtrIsCap ? RISCV::CLR_C_AQ_RL_64 : RISCV::LR_C_AQ_RL_64)
                      : (PtrIsCap ? RISCV::CLR_C_AQ_RL_128 : RISCV::LR_C_AQ_RL_128);
  }
}

static unsigned getSCForRMWCap(bool PtrIsCap, AtomicOrdering Ordering, int CLen) {
  switch (Ordering) {
  default:
    llvm_unreachable("Unexpected AtomicOrdering");
  case AtomicOrdering::Monotonic:
    return CLen == 64 ? (PtrIsCap ? RISCV::CSC_C_64 : RISCV::SC_C_64)
                      : (PtrIsCap ? RISCV::CSC_C_128 : RISCV::SC_C_128);
  case AtomicOrdering::Acquire:
    return CLen == 64 ? (PtrIsCap ? RISCV::CSC_C_AQ_64 : RISCV::SC_C_AQ_64)
                      : (PtrIsCap ? RISCV::CSC_C_AQ_128 : RISCV::SC_C_AQ_128);
  case AtomicOrdering::Release:
    return CLen == 64 ? (PtrIsCap ? RISCV::CSC_C_64 : RISCV::SC_C_64)
                      : (PtrIsCap ? RISCV::CSC_C_128 : RISCV::SC_C_128);
  case AtomicOrdering::AcquireRelease:
    return CLen == 64 ? (PtrIsCap ? RISCV::CSC_C_AQ_64 : RISCV::SC_C_AQ_64)
                      : (PtrIsCap ? RISCV::CSC_C_AQ_128 : RISCV::SC_C_AQ_128);
  case AtomicOrdering::SequentiallyConsistent:
    return CLen == 64 ? (PtrIsCap ? RISCV::CSC_C_AQ_RL_64 : RISCV::SC_C_AQ_RL_64)
                      : (PtrIsCap ? RISCV::CSC_C_AQ_RL_128 : RISCV::SC_C_AQ_RL_128);
  }
}

static void doAtomicBinOpExpansion(const RISCVInstrInfo *TII, MachineInstr &MI,
                                   DebugLoc DL, MachineBasicBlock *ThisMBB,
                                   MachineBasicBlock *LoopMBB,
                                   MachineBasicBlock *DoneMBB,
                                   AtomicRMWInst::BinOp BinOp, int Width,
                                   bool PtrIsCap) {
  Register DestReg = MI.getOperand(0).getReg();
  Register ScratchReg = MI.getOperand(1).getReg();
  Register AddrReg = MI.getOperand(2).getReg();
  Register IncrReg = MI.getOperand(3).getReg();
  AtomicOrdering Ordering =
      static_cast<AtomicOrdering>(MI.getOperand(4).getImm());

  // .loop:
  //   lr.[w|d] dest, (addr)
  //   binop scratch, dest, val
  //   sc.[w|d] scratch, scratch, (addr)
  //   bnez scratch, loop
  BuildMI(LoopMBB, DL, TII->get(getLRForRMW(PtrIsCap, Ordering, Width)), DestReg)
      .addReg(AddrReg);
  switch (BinOp) {
  default:
    llvm_unreachable("Unexpected AtomicRMW BinOp");
  case AtomicRMWInst::Xchg:
    BuildMI(LoopMBB, DL, TII->get(RISCV::ADD), ScratchReg)
        .addReg(RISCV::X0)
        .addReg(IncrReg);
    break;
  case AtomicRMWInst::Add:
    BuildMI(LoopMBB, DL, TII->get(RISCV::ADD), ScratchReg)
        .addReg(DestReg)
        .addReg(IncrReg);
    break;
  case AtomicRMWInst::Sub:
    BuildMI(LoopMBB, DL, TII->get(RISCV::SUB), ScratchReg)
        .addReg(DestReg)
        .addReg(IncrReg);
    break;
  case AtomicRMWInst::And:
    BuildMI(LoopMBB, DL, TII->get(RISCV::AND), ScratchReg)
        .addReg(DestReg)
        .addReg(IncrReg);
    break;
  case AtomicRMWInst::Or:
    BuildMI(LoopMBB, DL, TII->get(RISCV::OR), ScratchReg)
        .addReg(DestReg)
        .addReg(IncrReg);
    break;
  case AtomicRMWInst::Xor:
    BuildMI(LoopMBB, DL, TII->get(RISCV::XOR), ScratchReg)
        .addReg(DestReg)
        .addReg(IncrReg);
    break;
  case AtomicRMWInst::Nand:
    BuildMI(LoopMBB, DL, TII->get(RISCV::AND), ScratchReg)
        .addReg(DestReg)
        .addReg(IncrReg);
    BuildMI(LoopMBB, DL, TII->get(RISCV::XORI), ScratchReg)
        .addReg(ScratchReg)
        .addImm(-1);
    break;
  }
  BuildMI(LoopMBB, DL, TII->get(getSCForRMW(PtrIsCap, Ordering, Width)), ScratchReg)
      .addReg(AddrReg)
      .addReg(ScratchReg);
  BuildMI(LoopMBB, DL, TII->get(RISCV::BNE))
      .addReg(ScratchReg)
      .addReg(RISCV::X0)
      .addMBB(LoopMBB);
}

static void insertMaskedMerge(const RISCVInstrInfo *TII, DebugLoc DL,
                              MachineBasicBlock *MBB, Register DestReg,
                              Register OldValReg, Register NewValReg,
                              Register MaskReg, Register ScratchReg) {
  assert(OldValReg != ScratchReg && "OldValReg and ScratchReg must be unique");
  assert(OldValReg != MaskReg && "OldValReg and MaskReg must be unique");
  assert(ScratchReg != MaskReg && "ScratchReg and MaskReg must be unique");

  // We select bits from newval and oldval using:
  // https://graphics.stanford.edu/~seander/bithacks.html#MaskedMerge
  // r = oldval ^ ((oldval ^ newval) & masktargetdata);
  BuildMI(MBB, DL, TII->get(RISCV::XOR), ScratchReg)
      .addReg(OldValReg)
      .addReg(NewValReg);
  BuildMI(MBB, DL, TII->get(RISCV::AND), ScratchReg)
      .addReg(ScratchReg)
      .addReg(MaskReg);
  BuildMI(MBB, DL, TII->get(RISCV::XOR), DestReg)
      .addReg(OldValReg)
      .addReg(ScratchReg);
}

static void doMaskedAtomicBinOpExpansion(
    const RISCVInstrInfo *TII, MachineInstr &MI, DebugLoc DL,
    MachineBasicBlock *ThisMBB, MachineBasicBlock *LoopMBB,
    MachineBasicBlock *DoneMBB, AtomicRMWInst::BinOp BinOp, int Width) {
  assert(Width == 32 && "Should never need to expand masked 64-bit operations");
  Register DestReg = MI.getOperand(0).getReg();
  Register ScratchReg = MI.getOperand(1).getReg();
  Register AddrReg = MI.getOperand(2).getReg();
  Register IncrReg = MI.getOperand(3).getReg();
  Register MaskReg = MI.getOperand(4).getReg();
  AtomicOrdering Ordering =
      static_cast<AtomicOrdering>(MI.getOperand(5).getImm());

  // .loop:
  //   lr.w destreg, (alignedaddr)
  //   binop scratch, destreg, incr
  //   xor scratch, destreg, scratch
  //   and scratch, scratch, masktargetdata
  //   xor scratch, destreg, scratch
  //   sc.w scratch, scratch, (alignedaddr)
  //   bnez scratch, loop
  BuildMI(LoopMBB, DL, TII->get(getLRForRMW32(false, Ordering)), DestReg)
      .addReg(AddrReg);
  switch (BinOp) {
  default:
    llvm_unreachable("Unexpected AtomicRMW BinOp");
  case AtomicRMWInst::Xchg:
    BuildMI(LoopMBB, DL, TII->get(RISCV::ADDI), ScratchReg)
        .addReg(IncrReg)
        .addImm(0);
    break;
  case AtomicRMWInst::Add:
    BuildMI(LoopMBB, DL, TII->get(RISCV::ADD), ScratchReg)
        .addReg(DestReg)
        .addReg(IncrReg);
    break;
  case AtomicRMWInst::Sub:
    BuildMI(LoopMBB, DL, TII->get(RISCV::SUB), ScratchReg)
        .addReg(DestReg)
        .addReg(IncrReg);
    break;
  case AtomicRMWInst::Nand:
    BuildMI(LoopMBB, DL, TII->get(RISCV::AND), ScratchReg)
        .addReg(DestReg)
        .addReg(IncrReg);
    BuildMI(LoopMBB, DL, TII->get(RISCV::XORI), ScratchReg)
        .addReg(ScratchReg)
        .addImm(-1);
    break;
  }

  insertMaskedMerge(TII, DL, LoopMBB, ScratchReg, DestReg, ScratchReg, MaskReg,
                    ScratchReg);

  BuildMI(LoopMBB, DL, TII->get(getSCForRMW32(false, Ordering)), ScratchReg)
      .addReg(AddrReg)
      .addReg(ScratchReg);
  BuildMI(LoopMBB, DL, TII->get(RISCV::BNE))
      .addReg(ScratchReg)
      .addReg(RISCV::X0)
      .addMBB(LoopMBB);
}

bool RISCVExpandPseudo::expandAtomicBinOp(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    AtomicRMWInst::BinOp BinOp, bool IsMasked, int Width, bool PtrIsCap,
    MachineBasicBlock::iterator &NextMBBI) {
  assert(!(IsMasked && PtrIsCap) &&
         "Should never used masked operations with capabilities");

  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  MachineFunction *MF = MBB.getParent();
  auto LoopMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  auto DoneMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());

  // Insert new MBBs.
  MF->insert(++MBB.getIterator(), LoopMBB);
  MF->insert(++LoopMBB->getIterator(), DoneMBB);

  // Set up successors and transfer remaining instructions to DoneMBB.
  LoopMBB->addSuccessor(LoopMBB);
  LoopMBB->addSuccessor(DoneMBB);
  DoneMBB->splice(DoneMBB->end(), &MBB, MI, MBB.end());
  DoneMBB->transferSuccessors(&MBB);
  MBB.addSuccessor(LoopMBB);

  if (!IsMasked)
    doAtomicBinOpExpansion(TII, MI, DL, &MBB, LoopMBB, DoneMBB, BinOp, Width,
                           PtrIsCap);
  else
    doMaskedAtomicBinOpExpansion(TII, MI, DL, &MBB, LoopMBB, DoneMBB, BinOp,
                                 Width);

  NextMBBI = MBB.end();
  MI.eraseFromParent();

  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *LoopMBB);
  computeAndAddLiveIns(LiveRegs, *DoneMBB);

  return true;
}

static void insertSext(const RISCVInstrInfo *TII, DebugLoc DL,
                       MachineBasicBlock *MBB, Register ValReg,
                       Register ShamtReg) {
  BuildMI(MBB, DL, TII->get(RISCV::SLL), ValReg)
      .addReg(ValReg)
      .addReg(ShamtReg);
  BuildMI(MBB, DL, TII->get(RISCV::SRA), ValReg)
      .addReg(ValReg)
      .addReg(ShamtReg);
}

bool RISCVExpandPseudo::expandAtomicMinMaxOp(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    AtomicRMWInst::BinOp BinOp, bool IsMasked, int Width, bool PtrIsCap,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();
  MachineFunction *MF = MBB.getParent();
  auto LoopHeadMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  auto LoopIfBodyMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  auto LoopTailMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  auto DoneMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());

  // Insert new MBBs.
  MF->insert(++MBB.getIterator(), LoopHeadMBB);
  MF->insert(++LoopHeadMBB->getIterator(), LoopIfBodyMBB);
  MF->insert(++LoopIfBodyMBB->getIterator(), LoopTailMBB);
  MF->insert(++LoopTailMBB->getIterator(), DoneMBB);

  // Set up successors and transfer remaining instructions to DoneMBB.
  LoopHeadMBB->addSuccessor(LoopIfBodyMBB);
  LoopHeadMBB->addSuccessor(LoopTailMBB);
  LoopIfBodyMBB->addSuccessor(LoopTailMBB);
  LoopTailMBB->addSuccessor(LoopHeadMBB);
  LoopTailMBB->addSuccessor(DoneMBB);
  DoneMBB->splice(DoneMBB->end(), &MBB, MI, MBB.end());
  DoneMBB->transferSuccessors(&MBB);
  MBB.addSuccessor(LoopHeadMBB);

  if (IsMasked) {
    assert(!PtrIsCap &&
           "Should never used masked operations with capabilities");
    assert(Width == 32 &&
           "Should never need to expand masked 64-bit operations");

    Register DestReg = MI.getOperand(0).getReg();
    Register Scratch1Reg = MI.getOperand(1).getReg();
    Register Scratch2Reg = MI.getOperand(2).getReg();
    Register AddrReg = MI.getOperand(3).getReg();
    Register IncrReg = MI.getOperand(4).getReg();
    Register MaskReg = MI.getOperand(5).getReg();
    bool IsSigned = BinOp == AtomicRMWInst::Min || BinOp == AtomicRMWInst::Max;
    AtomicOrdering Ordering =
        static_cast<AtomicOrdering>(MI.getOperand(IsSigned ? 7 : 6).getImm());

    //
    // .loophead:
    //   lr.w destreg, (alignedaddr)
    //   and scratch2, destreg, mask
    //   mv scratch1, destreg
    //   [sext scratch2 if signed min/max]
    //   ifnochangeneeded scratch2, incr, .looptail
    BuildMI(LoopHeadMBB, DL, TII->get(getLRForRMW32(PtrIsCap, Ordering)), DestReg)
        .addReg(AddrReg);
    BuildMI(LoopHeadMBB, DL, TII->get(RISCV::AND), Scratch2Reg)
        .addReg(DestReg)
        .addReg(MaskReg);
    BuildMI(LoopHeadMBB, DL, TII->get(RISCV::ADDI), Scratch1Reg)
        .addReg(DestReg)
        .addImm(0);

    switch (BinOp) {
    default:
      llvm_unreachable("Unexpected AtomicRMW BinOp");
    case AtomicRMWInst::Max: {
      insertSext(TII, DL, LoopHeadMBB, Scratch2Reg, MI.getOperand(6).getReg());
      BuildMI(LoopHeadMBB, DL, TII->get(RISCV::BGE))
          .addReg(Scratch2Reg)
          .addReg(IncrReg)
          .addMBB(LoopTailMBB);
      break;
    }
    case AtomicRMWInst::Min: {
      insertSext(TII, DL, LoopHeadMBB, Scratch2Reg, MI.getOperand(6).getReg());
      BuildMI(LoopHeadMBB, DL, TII->get(RISCV::BGE))
          .addReg(IncrReg)
          .addReg(Scratch2Reg)
          .addMBB(LoopTailMBB);
      break;
    }
    case AtomicRMWInst::UMax:
      BuildMI(LoopHeadMBB, DL, TII->get(RISCV::BGEU))
          .addReg(Scratch2Reg)
          .addReg(IncrReg)
          .addMBB(LoopTailMBB);
      break;
    case AtomicRMWInst::UMin:
      BuildMI(LoopHeadMBB, DL, TII->get(RISCV::BGEU))
          .addReg(IncrReg)
          .addReg(Scratch2Reg)
          .addMBB(LoopTailMBB);
      break;
    }

    // .loopifbody:
    //   xor scratch1, destreg, incr
    //   and scratch1, scratch1, mask
    //   xor scratch1, destreg, scratch1
    insertMaskedMerge(TII, DL, LoopIfBodyMBB, Scratch1Reg, DestReg, IncrReg,
                      MaskReg, Scratch1Reg);

    // .looptail:
    //   sc.w scratch1, scratch1, (addr)
    //   bnez scratch1, loop
    BuildMI(LoopTailMBB, DL, TII->get(getSCForRMW32(PtrIsCap, Ordering)), Scratch1Reg)
        .addReg(AddrReg)
        .addReg(Scratch1Reg);
    BuildMI(LoopTailMBB, DL, TII->get(RISCV::BNE))
        .addReg(Scratch1Reg)
        .addReg(RISCV::X0)
        .addMBB(LoopHeadMBB);
  } else {
    Register DestReg = MI.getOperand(0).getReg();
    Register ScratchReg = MI.getOperand(1).getReg();
    Register AddrReg = MI.getOperand(2).getReg();
    Register IncrReg = MI.getOperand(3).getReg();
    AtomicOrdering Ordering =
        static_cast<AtomicOrdering>(MI.getOperand(4).getImm());

    //
    // .loophead:
    //   lr.[b|h] dest, (addr)
    //   mv scratch, dest
    //   ifnochangeneeded scratch, incr, .looptail
    BuildMI(LoopHeadMBB, DL, TII->get(getLRForRMW(PtrIsCap, Ordering, Width)), DestReg)
        .addReg(AddrReg);
    BuildMI(LoopHeadMBB, DL, TII->get(RISCV::ADDI), ScratchReg)
        .addReg(DestReg)
        .addImm(0);

    switch (BinOp) {
    default:
      llvm_unreachable("Unexpected AtomicRMW BinOp");
    case AtomicRMWInst::Max: {
      BuildMI(LoopHeadMBB, DL, TII->get(RISCV::BGE))
          .addReg(ScratchReg)
          .addReg(IncrReg)
          .addMBB(LoopTailMBB);
      break;
    }
    case AtomicRMWInst::Min: {
      BuildMI(LoopHeadMBB, DL, TII->get(RISCV::BGE))
          .addReg(IncrReg)
          .addReg(ScratchReg)
          .addMBB(LoopTailMBB);
      break;
    }
    case AtomicRMWInst::UMax:
      BuildMI(LoopHeadMBB, DL, TII->get(RISCV::BGEU))
          .addReg(ScratchReg)
          .addReg(IncrReg)
          .addMBB(LoopTailMBB);
      break;
    case AtomicRMWInst::UMin:
      BuildMI(LoopHeadMBB, DL, TII->get(RISCV::BGEU))
          .addReg(IncrReg)
          .addReg(ScratchReg)
          .addMBB(LoopTailMBB);
      break;
    }

    // .loopifbody:
    //   mv scratch, incr
    BuildMI(LoopIfBodyMBB, DL, TII->get(RISCV::ADDI), ScratchReg)
        .addReg(IncrReg)
        .addImm(0);

    // .looptail:
    //   sc.[b|h] scratch, scratch, (addr)
    //   bnez scratch, loop
    BuildMI(LoopTailMBB, DL, TII->get(getSCForRMW(PtrIsCap, Ordering, Width)), ScratchReg)
        .addReg(AddrReg)
        .addReg(ScratchReg);
    BuildMI(LoopTailMBB, DL, TII->get(RISCV::BNE))
        .addReg(ScratchReg)
        .addReg(RISCV::X0)
        .addMBB(LoopHeadMBB);
  }

  NextMBBI = MBB.end();
  MI.eraseFromParent();

  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *LoopHeadMBB);
  computeAndAddLiveIns(LiveRegs, *LoopIfBodyMBB);
  computeAndAddLiveIns(LiveRegs, *LoopTailMBB);
  computeAndAddLiveIns(LiveRegs, *DoneMBB);

  return true;
}

bool RISCVExpandPseudo::expandAtomicCmpXchg(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, bool IsMasked,
    int Width, bool PtrIsCap, MachineBasicBlock::iterator &NextMBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();
  MachineFunction *MF = MBB.getParent();
  auto LoopHeadMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  auto LoopTailMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  auto DoneMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());

  // Insert new MBBs.
  MF->insert(++MBB.getIterator(), LoopHeadMBB);
  MF->insert(++LoopHeadMBB->getIterator(), LoopTailMBB);
  MF->insert(++LoopTailMBB->getIterator(), DoneMBB);

  // Set up successors and transfer remaining instructions to DoneMBB.
  LoopHeadMBB->addSuccessor(LoopTailMBB);
  LoopHeadMBB->addSuccessor(DoneMBB);
  LoopTailMBB->addSuccessor(DoneMBB);
  LoopTailMBB->addSuccessor(LoopHeadMBB);
  DoneMBB->splice(DoneMBB->end(), &MBB, MI, MBB.end());
  DoneMBB->transferSuccessors(&MBB);
  MBB.addSuccessor(LoopHeadMBB);

  Register DestReg = MI.getOperand(0).getReg();
  Register ScratchReg = MI.getOperand(1).getReg();
  Register AddrReg = MI.getOperand(2).getReg();
  Register CmpValReg = MI.getOperand(3).getReg();
  Register NewValReg = MI.getOperand(4).getReg();
  AtomicOrdering Ordering =
      static_cast<AtomicOrdering>(MI.getOperand(IsMasked ? 6 : 5).getImm());

  if (!IsMasked) {
    // .loophead:
    //   lr.[w|d] dest, (addr)
    //   bne dest, cmpval, done
    BuildMI(LoopHeadMBB, DL, TII->get(getLRForRMW(PtrIsCap, Ordering, Width)), DestReg)
        .addReg(AddrReg);
    BuildMI(LoopHeadMBB, DL, TII->get(RISCV::BNE))
        .addReg(DestReg)
        .addReg(CmpValReg)
        .addMBB(DoneMBB);
    // .looptail:
    //   sc.[w|d] scratch, newval, (addr)
    //   bnez scratch, loophead
    BuildMI(LoopTailMBB, DL, TII->get(getSCForRMW(PtrIsCap, Ordering, Width)), ScratchReg)
        .addReg(AddrReg)
        .addReg(NewValReg);
    BuildMI(LoopTailMBB, DL, TII->get(RISCV::BNE))
        .addReg(ScratchReg)
        .addReg(RISCV::X0)
        .addMBB(LoopHeadMBB);
  } else {
    assert(!PtrIsCap &&
           "Should never used masked operations with capabilities");

    // .loophead:
    //   lr.w dest, (addr)
    //   and scratch, dest, mask
    //   bne scratch, cmpval, done
    Register MaskReg = MI.getOperand(5).getReg();
    BuildMI(LoopHeadMBB, DL, TII->get(getLRForRMW(false, Ordering, Width)), DestReg)
        .addReg(AddrReg);
    BuildMI(LoopHeadMBB, DL, TII->get(RISCV::AND), ScratchReg)
        .addReg(DestReg)
        .addReg(MaskReg);
    BuildMI(LoopHeadMBB, DL, TII->get(RISCV::BNE))
        .addReg(ScratchReg)
        .addReg(CmpValReg)
        .addMBB(DoneMBB);

    // .looptail:
    //   xor scratch, dest, newval
    //   and scratch, scratch, mask
    //   xor scratch, dest, scratch
    //   sc.w scratch, scratch, (adrr)
    //   bnez scratch, loophead
    insertMaskedMerge(TII, DL, LoopTailMBB, ScratchReg, DestReg, NewValReg,
                      MaskReg, ScratchReg);
    BuildMI(LoopTailMBB, DL, TII->get(getSCForRMW(false, Ordering, Width)), ScratchReg)
        .addReg(AddrReg)
        .addReg(ScratchReg);
    BuildMI(LoopTailMBB, DL, TII->get(RISCV::BNE))
        .addReg(ScratchReg)
        .addReg(RISCV::X0)
        .addMBB(LoopHeadMBB);
  }

  NextMBBI = MBB.end();
  MI.eraseFromParent();

  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *LoopHeadMBB);
  computeAndAddLiveIns(LiveRegs, *LoopTailMBB);
  computeAndAddLiveIns(LiveRegs, *DoneMBB);

  return true;
}

bool RISCVExpandPseudo::expandAtomicCmpXchgCap(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, bool PtrIsCap,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();
  MachineFunction *MF = MBB.getParent();
  const TargetRegisterInfo *TRI = MF->getSubtarget().getRegisterInfo();
  int CLen = TRI->getRegSizeInBits(RISCV::GPCRRegClass);
  auto LoopHeadMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  auto LoopTailMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  auto DoneMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());

  // Insert new MBBs.
  MF->insert(++MBB.getIterator(), LoopHeadMBB);
  MF->insert(++LoopHeadMBB->getIterator(), LoopTailMBB);
  MF->insert(++LoopTailMBB->getIterator(), DoneMBB);

  // Set up successors and transfer remaining instructions to DoneMBB.
  LoopHeadMBB->addSuccessor(LoopTailMBB);
  LoopHeadMBB->addSuccessor(DoneMBB);
  LoopTailMBB->addSuccessor(DoneMBB);
  LoopTailMBB->addSuccessor(LoopHeadMBB);
  DoneMBB->splice(DoneMBB->end(), &MBB, MI, MBB.end());
  DoneMBB->transferSuccessors(&MBB);
  MBB.addSuccessor(LoopHeadMBB);

  Register DestReg = MI.getOperand(0).getReg();
  Register ScratchReg = MI.getOperand(1).getReg();
  Register AddrReg = MI.getOperand(2).getReg();
  Register CmpValReg = MI.getOperand(3).getReg();
  Register NewValReg = MI.getOperand(4).getReg();
  AtomicOrdering Ordering =
      static_cast<AtomicOrdering>(MI.getOperand(5).getImm());

  // .loophead:
  //   lr.c dest, (addr)
  //   bne dest:sub_cap_addr, cmpval:sub_cap_addr, done
  BuildMI(LoopHeadMBB, DL, TII->get(getLRForRMWCap(PtrIsCap, Ordering, CLen)), DestReg)
      .addReg(AddrReg);
  BuildMI(LoopHeadMBB, DL, TII->get(RISCV::BNE))
      .addReg(TRI->getSubReg(DestReg, RISCV::sub_cap_addr))
      .addReg(TRI->getSubReg(CmpValReg, RISCV::sub_cap_addr))
      .addMBB(DoneMBB);
  // .looptail:
  //   sc.c scratch, newval, (addr)
  //   bnez scratch, loophead
  BuildMI(LoopTailMBB, DL, TII->get(getSCForRMWCap(PtrIsCap, Ordering, CLen)), ScratchReg)
      .addReg(AddrReg)
      .addReg(NewValReg);
  BuildMI(LoopTailMBB, DL, TII->get(RISCV::BNE))
      .addReg(ScratchReg)
      .addReg(RISCV::X0)
      .addMBB(LoopHeadMBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();

  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *LoopHeadMBB);
  computeAndAddLiveIns(LiveRegs, *LoopTailMBB);
  computeAndAddLiveIns(LiveRegs, *DoneMBB);

  return true;
}

bool RISCVExpandPseudo::expandAuipcInstPair(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI, unsigned FlagsHi,
    unsigned SecondOpcode) {
  MachineFunction *MF = MBB.getParent();
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  Register DestReg = MI.getOperand(0).getReg();
  const MachineOperand &Symbol = MI.getOperand(1);

  MachineBasicBlock *NewMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());

  // Tell AsmPrinter that we unconditionally want the symbol of this label to be
  // emitted.
  NewMBB->setLabelMustBeEmitted();

  MF->insert(++MBB.getIterator(), NewMBB);

  BuildMI(NewMBB, DL, TII->get(RISCV::AUIPC), DestReg)
      .addDisp(Symbol, 0, FlagsHi);
  BuildMI(NewMBB, DL, TII->get(SecondOpcode), DestReg)
      .addReg(DestReg)
      .addMBB(NewMBB, RISCVII::MO_PCREL_LO);

  // Move all the rest of the instructions to NewMBB.
  NewMBB->splice(NewMBB->end(), &MBB, std::next(MBBI), MBB.end());
  // Update machine-CFG edges.
  NewMBB->transferSuccessorsAndUpdatePHIs(&MBB);
  // Make the original basic block fall-through to the new.
  MBB.addSuccessor(NewMBB);

  // Make sure live-ins are correctly attached to this new basic block.
  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *NewMBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();
  return true;
}

bool RISCVExpandPseudo::expandLoadLocalAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, RISCVII::MO_PCREL_HI,
                             RISCV::ADDI);
}

bool RISCVExpandPseudo::expandLoadAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineFunction *MF = MBB.getParent();

  unsigned SecondOpcode;
  unsigned FlagsHi;
  if (MF->getTarget().isPositionIndependent()) {
    const auto &STI = MF->getSubtarget<RISCVSubtarget>();
    SecondOpcode = STI.is64Bit() ? RISCV::LD : RISCV::LW;
    FlagsHi = RISCVII::MO_GOT_HI;
  } else {
    SecondOpcode = RISCV::ADDI;
    FlagsHi = RISCVII::MO_PCREL_HI;
  }
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, FlagsHi, SecondOpcode);
}

bool RISCVExpandPseudo::expandLoadTLSIEAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineFunction *MF = MBB.getParent();

  const auto &STI = MF->getSubtarget<RISCVSubtarget>();
  unsigned SecondOpcode = STI.is64Bit() ? RISCV::LD : RISCV::LW;
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, RISCVII::MO_TLS_GOT_HI,
                             SecondOpcode);
}

bool RISCVExpandPseudo::expandLoadTLSGDAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, RISCVII::MO_TLS_GD_HI,
                             RISCV::ADDI);
}

bool RISCVExpandPseudo::expandAuipccInstPair(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI, unsigned FlagsHi,
    unsigned SecondOpcode) {
  MachineFunction *MF = MBB.getParent();
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  bool HasTmpReg = MI.getNumOperands() > 2;
  Register DestReg = MI.getOperand(0).getReg();
  Register TmpReg = MI.getOperand(HasTmpReg ? 1 : 0).getReg();
  const MachineOperand &Symbol = MI.getOperand(HasTmpReg ? 2 : 1);

  MachineBasicBlock *NewMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());

  // Tell AsmPrinter that we unconditionally want the symbol of this label to be
  // emitted.
  NewMBB->setLabelMustBeEmitted();

  MF->insert(++MBB.getIterator(), NewMBB);

  BuildMI(NewMBB, DL, TII->get(RISCV::AUIPCC), TmpReg)
      .addDisp(Symbol, 0, FlagsHi);
  BuildMI(NewMBB, DL, TII->get(SecondOpcode), DestReg)
      .addReg(TmpReg)
      .addMBB(NewMBB, RISCVII::MO_PCREL_LO);

  // Move all the rest of the instructions to NewMBB.
  NewMBB->splice(NewMBB->end(), &MBB, std::next(MBBI), MBB.end());
  // Update machine-CFG edges.
  NewMBB->transferSuccessorsAndUpdatePHIs(&MBB);
  // Make the original basic block fall-through to the new.
  MBB.addSuccessor(NewMBB);

  // Make sure live-ins are correctly attached to this new basic block.
  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *NewMBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();
  return true;
}

bool RISCVExpandPseudo::expandCapLoadGlobalCap(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineFunction *MF = MBB.getParent();

  const auto &STI = MF->getSubtarget<RISCVSubtarget>();
  unsigned SecondOpcode = STI.is64Bit() ? RISCV::CLC_128 : RISCV::CLC_64;
  return expandAuipccInstPair(MBB, MBBI, NextMBBI, RISCVII::MO_CAPTAB_PCREL_HI,
                              SecondOpcode);
}

bool RISCVExpandPseudo::expandCapLoadTLSIEAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineFunction *MF = MBB.getParent();

  const auto &STI = MF->getSubtarget<RISCVSubtarget>();
  unsigned SecondOpcode = STI.is64Bit() ? RISCV::CLD : RISCV::CLW;
  return expandAuipccInstPair(MBB, MBBI, NextMBBI,
                              RISCVII::MO_TLS_IE_CAPTAB_PCREL_HI,
                              SecondOpcode);
}

bool RISCVExpandPseudo::expandCapLoadTLSGDCap(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  return expandAuipccInstPair(MBB, MBBI, NextMBBI,
                              RISCVII::MO_TLS_GD_CAPTAB_PCREL_HI,
                              RISCV::CIncOffsetImm);
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVExpandPseudo, "riscv-expand-pseudo",
                RISCV_EXPAND_PSEUDO_NAME, false, false)
namespace llvm {

FunctionPass *createRISCVExpandPseudoPass() { return new RISCVExpandPseudo(); }

} // end of namespace llvm
