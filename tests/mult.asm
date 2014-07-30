.export
.data
    zero: 0
    a: 0
    b: -1
    c: -7
    tmp: 0
    tmpb: 0
    tmpc: 0
    sig: 0
    one: 1
    minusOne: -1
    max: 0
    inc: 0
.text
  start:

  mult:
    a a # a = 0;
    sig sig # sig = 0;

    # if (-b <= 0) goto bpos;
    tmp tmp
    b tmp bpos

    minusOne sig # sig++;

    # tmpb = -b;
    tmpb tmpb
    b tmpb

    zero zero checkc # goto check;
  bpos:
    # tmpb = b;
    tmpb tmpb
    b zero
    zero tmpb
    zero zero
  checkc:
    # if (-c <= 0) goto cpos;
    tmp tmp
    c tmp cpos

    minusOne sig # sig++;

    # tmpc = -c;
    tmpc tmpc
    c tmpc

    zero zero multMax # goto multMax;
  cpos:
    # tmpc = c;
    tmpc tmpc
    c zero
    zero tmpc
    zero zero
  multMax:
    # if (tmpb - tmpc <= zero) goto loopB;
    tmp tmp
    tmpb zero
    zero tmp
    zero zero
    tmpc tmp loopB

    zero zero loopC # goto loopC;
  loopB:
    # max = tmpb;
    max max
    tmpb zero
    zero max
    zero zero

    # inc = -tmpc;
    inc inc
    tmpc inc multLoop # goto multLoop;
  loopC:
    # max = tmpc;
    max max
    tmpc zero
    zero max
    zero zero

    # inc = -tmpb;
    inc inc
    tmpb inc
  multLoop:
    zero max multSig # if (max <= 0) goto multSig;
    inc a # a = a - (-tmpc or -tmpb);
    one max # max--;
    zero zero multLoop # goto multLoop;
  multSig:
    one sig checkgeq # sig--; if (sig <= 0) goto checkgeq;
    zero zero multDone # goto multDone;
  checkgeq:
    # if (-sig <= 0) goto multNeg;
    tmp tmp
    sig tmp multNeg
    zero zero multDone # goto multDone;
  multNeg:
    # tmp = a;
    tmp tmp
    a zero
    zero tmp
    zero zero

    # a = -tmp;
    a a
    tmp a
  multDone:
    zero zero multDone
