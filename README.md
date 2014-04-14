assembler
=========

C++ program to convert subleq assembly text file to object file.

```
To compile: make
Usage mode: subleq-asm <subleq_assembly_file> <binary_output>
```

Input example:
```asm
.export
    ln
.text
  ln:
    A B ln
    A B C
.data
    A 5
    B 123
```
