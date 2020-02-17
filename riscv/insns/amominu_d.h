require_extension('A');
require_rv64;
WRITE_RD(CHERI_MODE_AMO(uint64, insn.rs1(), 0, [&](uint64_t lhs) { return std::min(lhs, RS2); }));
