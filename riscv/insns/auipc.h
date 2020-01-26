#ifdef CHERI_MERGED_RF
if (CHERI->get_mode()) {
  cheri_reg_t cap = PCC;
  cap.set_cursor(CHERI->from_arch_pc(pc) + insn.u_imm());
  WRITE_CD(cap);
} else
#endif
{
  WRITE_RD(sext_xlen(insn.u_imm() + pc));
}
