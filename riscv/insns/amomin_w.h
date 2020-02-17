require_extension('A');
WRITE_RD(sext32(CHERI_MODE_AMO(uint32, insn.rs1(), 0, [&](int32_t lhs) { return std::min(lhs, int32_t(RS2)); })));
