.export
.data
    zero: 0
    one:  1
    two:  2
    minusOne: -1

    n1:  1
    n2:  0
    n:   8 # will result in fib == 13
    fib: 0
    i:   2
    tmp: 0
.text
  start: # stopping if (n <= 1)
    # tmp = n;
    tmp tmp
    n zero
    zero tmp
    zero zero

    one tmp fib1 # if (tmp <= one) goto fib1;
    zero zero check2 # goto check2;
  fib1:
    fib fib exit # fib = 0; goto exit;

  check2: # stopping if (n == 2)
    # tmp = n;
    tmp tmp
    n zero
    zero tmp
    zero zero

    two tmp fib2 # if (tmp <= two) goto fib2;
    zero zero calcFib # goto calcFib;
  fib2:
    # fib = one;
    fib fib
    one zero
    zero fib
    zero zero

    zero zero exit # goto exit;

  calcFib:
    # tmp = n;
    tmp tmp
    n zero
    zero tmp
    zero zero

    i tmp exit # if (tmp <= i) goto exit;

    # fib = n1;
    fib fib
    n1 zero
    zero fib
    zero zero

    # fib = fib + n2;
    n2 zero
    zero fib
    zero zero

    # n2 = n1;
    n2 n2
    n1 zero
    zero n2
    zero zero

    # n1 = fib;
    n1 n1
    fib zero
    zero n1
    zero zero

    minusOne i # i++;
    zero zero calcFib # goto calcFib;

  exit:
    zero zero exit
