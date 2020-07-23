// See LICENSE_CHERI for license details.

if (!CS1.tag) {
  CHERI->raise_trap(CAUSE_CHERI_TAG_FAULT, insn.cs1());
} else if (CS1.sealed()) {
  CHERI->raise_trap(CAUSE_CHERI_SEAL_FAULT, insn.cs1());
} else if (!(CS1.perms & BIT(CHERI_PERMIT_EXECUTE))) {
  CHERI->raise_trap(CAUSE_CHERI_PERMIT_EXECUTE_FAULT, insn.cs1());
} else {
  cheri_reg_t temp = CS1;
  temp.otype = OTYPE_SENTRY;
  WRITE_CD(temp);
}
