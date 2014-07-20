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
    label
.data
    A 5
    B 123
.text
  label:
    A B label
    A 0xFF C
```
