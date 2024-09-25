# My Small C Projects


### GNU Recreation
rewriting some of the GUN utils (coreutils)
1. `cat.c`  concatenate files
2. `tr_ng.c`  translate or delete characters (new generation)
3. `yes.c`  print 'y' repeatedly until killed

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


## Libs
### leven.c
Levenshtein Distance  
this file is a dependency for `codeM`


### buffered_io.h
It helps to buffer IO-required calls such as `putc` and `puts` to reduce the number of `write` syscalls, resulting in better performance.

this file is a dependency for `permugen.c`, `tokenizeIt.c` and `tr_ng.c`


## Utils
### permugen.c  
Permutation Generator  
generates word lists for fuzzing based on some given seeds


### tokenizeIt.c
Text Tokenizer  
helps to extract words (tokens) from the given input

could be used together with the permugen program to
generate customized word lists


### xor_encrypt.c
performs a bitwise XOR operation on the entire given file with a fixed value


---
### codeM
Common Iranian ID number (code-e-melli)
also includes:
1. a shell program
2. python C extension module (to use codeM through python)


## Others

### graphics
some graphical programs
