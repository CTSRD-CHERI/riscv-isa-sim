require_extension('A');
CHERI_MODE_ACQUIRE_LOAD_RESERVATION(int16, insn.rs1(), 0);
WRITE_RD(CHERI_MODE_LOAD(int16, insn.rs1(), 0));
