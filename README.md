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
### dyna.h
Generic dynamic array implementation


### leven.c
Levenshtein Distance  


### mini-lexer.c
A minimal, chunk based, language independent lexer.


### buffered_io.h
It helps to buffer IO-required calls such as `putc` and `puts` to reduce the number of `write` syscalls, resulting in better performance.

Additionally, it's python C extension, `buffered_io_py.c` is available.


### unescape.h
Interpreters backslash characters in the given input, both in-place and out-of-place



## Utils
### permugen.c  
Permutation Generator  
generates word lists for fuzzing based on some given seeds


### key_extractor.c
Text Tokenizer  
It helps to extract keywords (tokens) from input text (based on mini-lexer.c)

It can be used together with permugen to generate customized word lists


### moreless.c
Automatically runs less command  
`moreless` is a pre-loading wrapper program (like proxychains),
It displays the stdout of all programs within a `less` process.


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
