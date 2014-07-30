.export
.data
    zero: 0
    returnAddress: 0
    returnValue: 0
    minusOne: -1
    backptr: .ptr back
.text
  start:
    # foo();
    returnAddress returnAddress
    backptr zero
    zero returnAddress
    zero zero
    zero zero foo
  back: # exit
    zero zero back

  foo:
    # return 1;
    returnValue returnValue
    minusOne returnValue
    returnJump returnJump
    returnAddress zero
    zero returnJump
    zero zero
    zero zero returnJump:
