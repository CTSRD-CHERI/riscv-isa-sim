// See LICENSE_CHERI for license details.

reg_t new_pc = CS1.cursor() & ~(reg_t(1));
reg_t min_insn_bytes = p->supports_extension('C') ? 2 : 4;

if (!CS1.tag) {
  CHERI->raise_trap(CAUSE_CHERI_TAG_FAULT, insn.cs1());
} else if (!CS2.tag) {
  CHERI->raise_trap(CAUSE_CHERI_TAG_FAULT, insn.cs2());
} else if (!CS1.sealed()) {
  CHERI->raise_trap(CAUSE_CHERI_SEAL_FAULT, insn.cs1());
} else if (!CS2.sealed()) {
  CHERI->raise_trap(CAUSE_CHERI_SEAL_FAULT, insn.cs2());
} else if (CS1.otype != CS2.otype) {
  CHERI->raise_trap(CAUSE_CHERI_TYPE_FAULT, insn.cs1());
} else if (!(CS1.perms & BIT(CHERI_PERMIT_CCALL))) {
  CHERI->raise_trap(CAUSE_CHERI_PERMIT_CCALL_FAULT, insn.cs1());
} else if (!(CS2.perms & BIT(CHERI_PERMIT_CCALL))) {
  CHERI->raise_trap(CAUSE_CHERI_PERMIT_CCALL_FAULT, insn.cs2());
} else if (!(CS1.perms & BIT(CHERI_PERMIT_EXECUTE))) {
  CHERI->raise_trap(CAUSE_CHERI_PERMIT_EXECUTE_FAULT, insn.cs1());
} else if (CS2.perms & BIT(CHERI_PERMIT_EXECUTE)) {
  CHERI->raise_trap(CAUSE_CHERI_PERMIT_EXECUTE_FAULT, insn.cs2());
} else if (new_pc < CS1.base()) {
  CHERI->raise_trap(CAUSE_CHERI_LENGTH_FAULT, insn.cs1());
} else if (new_pc + min_insn_bytes > CS1.top()) {
  CHERI->raise_trap(CAUSE_CHERI_LENGTH_FAULT, insn.cs1());
} else if (CS1.base() & ~p->pc_alignment_mask()) {
  CHERI->raise_trap(CAUSE_CHERI_UNALIGNED_BASE_FAULT, insn.cs1());
} else if (new_pc & ~p->pc_alignment_mask()) {
  throw trap_instruction_address_misaligned(new_pc);
} else {
  cheri_reg_t icc = CS1;
  icc.otype = OTYPE_UNSEALED;
  SET_SCR(CHERI_SCR_PCC, icc);
  set_pc(TO_ARCH_PC(new_pc));

  /* IDC */
  cheri_reg_t idc = CS2;
  idc.otype = OTYPE_UNSEALED;
  WRITE_CREG(31, idc); // TODO: define IDC=31 elsewhere
}
