// See LICENSE_CHERI for license details.

reg_t new_pc = CS1.cursor() & ~(reg_t(1));
reg_t min_insn_bytes = p->supports_extension('C') ? 2 : 4;

if (!CS1.tag) {
  CHERI->raise_trap(CAUSE_CHERI_TAG_FAULT, insn.cs1());
} else if (CS1.sealed()) {
  CHERI->raise_trap(CAUSE_CHERI_SEAL_FAULT, insn.cs1());
} else if ((CS1.perms & BIT(CHERI_PERMIT_EXECUTE)) != BIT(CHERI_PERMIT_EXECUTE)) {
  CHERI->raise_trap(CAUSE_CHERI_PERMIT_EXECUTE_FAULT, insn.cs1());
} else if (CS1.cursor() < CS1.base()) {
  CHERI->raise_trap(CAUSE_CHERI_LENGTH_FAULT, insn.cs1());
} else if (CS1.cursor() + min_insn_bytes > CS1.top()) {
  CHERI->raise_trap(CAUSE_CHERI_LENGTH_FAULT, insn.cs1());
} else if (CS1.base() & ~p->pc_alignment_mask()) {
  CHERI->raise_trap(CAUSE_CHERI_UNALIGNED_BASE_FAULT, insn.cs1());
} else if (new_pc & ~p->pc_alignment_mask()) {
  throw trap_instruction_address_misaligned(new_pc);
} else {
  cheri_reg_t link = GET_SCR(CHERI_SCR_PCC);
  link.set_offset(npc);
  /* We were tagged and in bounds for this instruction, so at worst next
   * instruction is just out of bounds but still representable. */
  assert(link.tag);

  SET_SCR(CHERI_SCR_PCC, CS1);
  set_pc(CHERI->to_arch_pc(new_pc));
  /* Must come after write to PCC in case CD == CS1 */
  WRITE_CD(link);
}
