# My Small C Projects


### GNU Recreation
rewriting some of the GUN utils (coreutils)
1. `cat.c`
2. `tr_ng.c`
3. `yes.c`

---

### Templates
some templates to help write C programs
1. `defer.c`  Golang defer in C
2. `Vheader.c`  header (metadata) before pointers

---

### DS
Data structure and Memory management
1. `arena.c`  Arena implementation
2. `dynamic_array.c`  Dynamic Array
3. `hashtab.c`  Hash Table
4. `linked_list.c`  Linked Lists
5. `ptable.c`  Pointer Table with arbitrary remove element
6. `ring_buffer.c`  Ring Buffer
7. `tape_mem.c`  Tape like memory allocator

---

### permugen.c
Permutation Generator

to generate word lists for fuzzing based on some given seeds


### buffered_io.h
Helps to buffer the IO calls like `putc`, `puts`, etc.

to reduce number of `write` syscall so having better performance.

this file is a dependency for `permugen.c`

---

### leven.c
Levenshtein Distance algorithm

this file is a dependency for the `codeM` project


### codeM
Common Iranian ID number (code-e-melli)
also includes:
1. a shell program
2. python C extension module (to use codeM through python)

---

### graphics
graphical programs


### xor_encrypt.c
to XOR the entire given file with a fixed value
