// See LICENSE_CHERI for license details.

cheri_reg_t temp = CS1;
temp.offset = CS1.offset + RS2;
WRITE_CD(temp);
