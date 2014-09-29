.export
    slib_ra
    slib_a0
    slib_a1
    slib_v0
    slib_v1
    slib_mul
    slib_div
    slib_not
    slib_and
    slib_or
    slib_xor
    slib_sll
    slib_srl
    slib_sra
.data
    slib_ra: 0
    slib_a0: 0
    slib_a1: 0
    slib_v0: 0
    slib_v1: 0
    zero: 0
    sign: 0
.text
  slib_mul:
    clr slib_v0
    clr sign
    
    bgt slib_a1 zero // if (b > 0) goto bpos;
    
    // return;
    mov fill_ra slib_ra
    zero zero fill_ra:
