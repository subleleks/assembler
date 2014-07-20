assembler
=========

C++ program to convert subleq assembly text file to object file.

```
To compile: make
Usage mode: subleq-asm <assembly_file> <object_file>
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
