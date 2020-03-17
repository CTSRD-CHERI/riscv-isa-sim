// See LICENSE for license details.
// See LICENSE_CHERI for license details.

#include "disasm.h"

const char* xpr_name[] = {
  "zero", "ra", "sp",  "gp",  "tp", "t0",  "t1",  "t2",
  "s0",   "s1", "a0",  "a1",  "a2", "a3",  "a4",  "a5",
  "a6",   "a7", "s2",  "s3",  "s4", "s5",  "s6",  "s7",
  "s8",   "s9", "s10", "s11", "t3", "t4",  "t5",  "t6"
};

const char* fpr_name[] = {
  "ft0", "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",
  "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
  "fa6", "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",
  "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"
};

const char* vr_name[] = {
  "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",
  "v8",  "v9",  "v10", "v11", "v12", "v13", "v14", "v15",
  "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
  "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31"
};

const char *cheri_reg_names[] = {
  "cnull", "cra", "csp",  "cgp",  "ctp", "ct0",  "ct1",  "ct2",
  "cs0",   "cs1", "ca0",  "ca1",  "ca2", "ca3",  "ca4",  "ca5",
  "ca6",   "ca7", "cs2",  "cs3",  "cs4", "cs5",  "cs6",  "cs7",
  "cs8",   "cs9", "cs10", "cs11", "ct3", "ct4",  "ct5",  "ct6"
};

const char* csr_name(int which) {
  switch (which) {
    #define DECLARE_CSR(name, number)  case number: return #name;
    #include "encoding.h"
    #undef DECLARE_CSR
  }
  return "unknown-csr";
}

const char* scr_name(int which) {
  switch (which) {
    #define DECLARE_CHERI_SCR(name, number)  case number: return #name;
    #include "encoding.h"
    #undef DECLARE_CHERI_SCR
  }
  return "unknown-scr";
}
