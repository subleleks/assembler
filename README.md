# assembler

C++ program that converts SUBLEQ Assembly text file to an object file format.

It's part of a series of programs we're making:

1. [Kernel](https://github.com/subleleks/kernel)
2. [Compiler](https://github.com/subleleks/compiler)
3. [Assembler](https://github.com/subleleks/assembler)
4. [Linker](https://github.com/subleleks/linker)
5. [Simulator](https://github.com/subleleks/simulator)
6. [Hardware](https://github.com/subleleks/hardware)

## Summary

To build it, simply run `make`.

**Usage**:

    subleq-asm <assembly_file> <object_file>

## SUBLEQ Assembly

This is a special kind of Assembly language; it has only _one_ instruction,
called SUBLEQ.

### The SUBLEQ Instruction

It has the following format:

```asm
SUBLEQ A B C
```

Here's what it does, in a kind of pseudo-code:

```c
SUBLEQ(address A, address B, address C)
{
    VALUE(B) = (VALUE(B) - VALUE(A))

    if (VALUE(B) <= 0)
        goto C
    else
        GOTO NEXT_INSTRUCTION
}
```

All three operands (_A_, _B_ and _C_) are _memory addresses_. `VALUE(A)` means
the value on memory address `A`.

### The File format

The whole file has the following format:

```asm
// Comments to end-of-line

.export
    // Exported symbols

.data
    // Specify data on memory

.text
    // Instructions
  label:
    // Mode instructions
```

Now, some rules for the `.text` part:

- Instructions come on the format `SUBLEQ A B C`
- `A`, `B` and `C` can be numbers (memory addresses starting from zero) or
  labels for things on the `.data` part

```asm
.data
    numberTwo: 2
	numberMinusThree: -3
	minusOne: -1

.text
    SUBLEQ 3 2 1
    // Which is the same as
	SUBLEQ minusOne numberMinusThree numberTwo
```

- You can suppress `SUBLEQ`, merely writing `A B C`

```asm
// Same as above example
.text
    3 2 1
	minusOne numberMinusThree numberTwo
```

- If you don't specify `C` it is assumed to be the next instruction's
  address.

```asm
.text
    // So this means it will always go to the
	// next instruction
    3 2
```

- Finally, if you don't provide `B` we assume it's the same as `A`.

```asm
.text
    // So this...
	3
	// ...actually means this...
    3 3
    // Which also has the next address implied
```

### Summary

So, in the end, these are all valid statements:

```asm
.text
  label:
    INSTRUCTION A B C
	            A B C
				A B
				A
```

### Example

The following is an example of a SUBLEQ Assembly file:

```asm
.export
    label
.data
    A 5
    B 123
.text
  label:
    A B label
    A 0xFF C
```

For more, check out our [assembly-examples][examples] repository.

## Object Code

This program translates a well-formed source in previous Assembly language to an
object code.

It is a binary file with the following format:

```
What                      Raw size
---------------------------------------------
Count of exported symbols    (uint32_t)
All exported symbols:
    Symbol name              (null-ended char[])
	Symbol address           (uint32_t)

Count of pending references  (uint32_t)
All pending references:
    Symbol name              (null-ended char[])
	Number of references to  (uint32_t)
    this symbol
	All references:
	    Reference            (uint32_t)
Number of relative addresses (uint32_t)
All relative addresses:
    Address                  (uint32_t)
Assembled code size          (uint32_t)
Assembled code:
    s
```

[examples]: https://github.com/subleleks/assembly-examples

