/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

  Hashtab is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  Hashtab is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 *  file: hashtab.c
 *  created on: 25 Jun 2024
 *
 *  A Simple Hash Table Implementation
 *
 *  by default it uses uint32_t for indices and hash values
 *
 *  you need to provide these 3 wrapper functions below
 *  based on the data structure that you use
 *  otherwise it uses the defaults (see implementation)
 *
 *  @head points to the head of your data structure,
 *  it's **char to make using arrays easier:
 *
 *    DATA_T *__getter (DATA_T **head, idx_t index);
 *    size_t __lenof (DATA_T **head, idx_t index);
 *    bool __isequal (DATA_T *v1, DATA_T *v2);
 *
 *
 *  here is a simple example with int array:
 *  ```c
 *  DATA_T *
 *  __getter (DATA_T **head, idx_t index)
 *  {
 *    return *head + (index * sizeof (int));
 *  }
 *
 *  size_t
 *  __lenof (DATA_T **head, idx_t index)
 *  {
 *    return sizeof (int);  // int is always 4 bytes
 *  }
 *
 *  bool
 *  __isequal (DATA_T *v1, DATA_T *v2)
 *  {
 *    return *v1 == *v2;
 *  }
 *  ```
 *
 *
 *  Compilation:
 *    to compile the test program:
 *    cc -ggdb -Wall -Wextra -Werror \
 *       -D HASHTAB_IMPLEMENTATION \
 *       -D HASHTAB_TEST hashtab.c -o test.out
 *
 *    to include in c files (based on the above example):
 *    ```c
 *    #define HASHTAB_IMPLEMENTATION
 *    #include "hashtab.c"
 *
 *    // implementation of __getter, __lenof, __isequal
 *    // also you hash function (pass it null to use the default)
 *    size_t simple_hash (const char *data, idx_t len) {...}
 *
 *    int
 *    main (void)
 *    {
 *      int *data[] = {...};
 *
 *      // we insert and search data around the hash of it
 *      // in a interval of length delta_l (here delta_l = 1)
 *      // and as our simple_hash function values are in [0, 255]
 *      // we make a table of length 256
 *      HashTable t = new_hashtab (256, data, 1);
 *      ht_set_funs (&t, simple_hash, __getter, __lenof, __isequal);
 *
 *      idx_t *mem = malloc (ht_sizeof (&t));
 *      // or use nmap to map a file or memory
 *
 *      if (0 != ht_init (&t, mem))
 *        {
 *          puts ("hashtab initialization failed.");
 *          return -1;
 *        }
 *
 *      // do something here
 *      // use ht_insert, ht_indexof functions to insert new data
 *      // or access data by key, both in O(delta_l) time complexity
 *
 *      ht_free (&t, free (mem));
 *      // in case of nmap:  ht_free (&t, munmap (mem, cap_bytes));
 *      return 0;
 *    }
 *    ```
 **/
#ifndef HASHTAB__H__
#define HASHTAB__H__
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef hash_t
#  define hash_t uint32_t
#endif
#ifndef idx_t
#  define idx_t uint32_t
#endif

#ifndef HASHTABDEFF 
#  define HASHTABDEFF static inline
#endif

#ifndef UNUSED
#  define UNUSED(x) (void)(x)
#endif

#ifndef GET_DATA_T
#  define DATA_T char
#endif

enum ht_error_t {
  HT_FOUND = 0,
  HT_NOT_FOUND,
  HT_DUPLICATED,
  HT_INSERTION_FAILED,
  HT_NO_EMPTYSLOT
};

typedef hash_t (*ht_hasher)(const char *data, idx_t len);
typedef DATA_T *(*ht_get)(DATA_T **head, idx_t index);
typedef size_t (*ht_len)(DATA_T **head, idx_t index);
typedef bool (*ht_isequal)(DATA_T *v1, DATA_T *v2);

/**
 *  FNV-1a: a simple hash function
 *  used as the default hasher
 */
HASHTABDEFF hash_t hash_FNV_1a (const char *data, idx_t len);

struct hashtab_t {
  idx_t *table;
  idx_t cap;
  idx_t dl; /* delta_l */

  DATA_T **data;
  ht_get Getter;
  ht_len Lenof;
  ht_isequal isEqual;
  ht_hasher Hasher;
};
typedef struct hashtab_t HashTable;
/* 8 * cap  in the default configuration */
#define ht_sizeof(ht) ((ht)->cap * sizeof (idx_t))

#define new_hashtab(table_len, data_ptr, delta_l)                       \
  (HashTable){.cap=(idx_t)(table_len), .dl=(idx_t)(delta_l),            \
      .data=(DATA_T**)data_ptr}

#define ht_set_funs(ht, hasher, getter, lenof, isequal) do {    \
    (ht)->Hasher= hasher;                                       \
    (ht)->Getter = getter;                                      \
    (ht)->Lenof = lenof;                                        \
    (ht)->isEqual = isequal;                                    \
  } while (0)

HASHTABDEFF int ht_init (HashTable *ht, idx_t *buf);

#ifdef HAVE_MALLOC
#  define ht_init2(ht) ht_init (ht, (idx_t *)malloc (ht_sizeof (ht)))
#endif

HASHTABDEFF int ht_insert (HashTable *ht, idx_t data_idx);

HASHTABDEFF int
ht_idxof (HashTable *ht, char *key, size_t key_len, idx_t *result);

#define ht_idxofs(ht, key, result) \
  ht_idxof (ht, key, strlen (key), result)


#define ht_free(ht, free_fun) do {                \
  size_t cap_bytes = ht_sizeof (ht);              \
  void *mem = (ht)->table;                        \
  if (mem && cap_bytes > 0) {                     \
    free_fun;                                     \
  }} while (0)

#endif /* HAHSTAB__H__ */

/* hash table implementation */
#ifdef HASHTAB_IMPLEMENTATION

HASHTABDEFF hash_t
hash_FNV_1a (const char *data, idx_t len)
{
  const hash_t FNV_PRIME = 0x01000193;
  hash_t hash = 0x811c9dc5;

  for (; len > 0; --len, ++data)
    {
      hash = (hash ^ *data) * FNV_PRIME;
    }

  return hash;
}

/* internal - default getter, lenof, isequal */
static inline DATA_T *
__default_getter (DATA_T **head, idx_t index)
{
  return head[index];
}

static inline size_t
__default_lenof (DATA_T **head, idx_t index)
{
  return strlen (head[index]);
}

static inline bool
__default_isequal (DATA_T *v1, DATA_T *v2)
{
  if (!v1 || !v2)
    return false;
  return 0 == strcmp(v1, v2);
}

HASHTABDEFF int
ht_init (HashTable *ht, hash_t *buf)
{
  if (NULL == ht || NULL == buf
      || NULL == ht->data || ht->cap == 0)
    return -1;

  memset (buf, 0xFF, ht_sizeof (ht));
  ht->table = buf;

  if (NULL == ht->Hasher)
    ht->Hasher = &hash_FNV_1a;
  if (NULL == ht->Getter)
    ht->Getter = &__default_getter;
  if (NULL == ht->Lenof)
    ht->Lenof = &__default_lenof;
  if (NULL == ht->isEqual)
    ht->isEqual = &__default_isequal;

  return 0;
}

/* internal function */
static inline hash_t
__do_hash (HashTable *t, idx_t i)
{
  if ((idx_t)-1 == i)
    return -1;

  return t->Hasher (t->Getter (t->data, i),
                    t->Lenof (t->data, i));
}

HASHTABDEFF int
ht_insert (HashTable *ht, idx_t data_idx)
{
  hash_t hash = __do_hash (ht, data_idx) % ht->cap;
  
  idx_t *ptr = ht->table + hash;
  if ((idx_t)-1 == *ptr)
    {
      *ptr = data_idx;
      return HT_FOUND;
    }
  else
    {
      /* hash of occupied slot */
      hash_t occ_h = __do_hash (ht, *ptr);
      if (occ_h == hash &&
          ht->isEqual (ht->Getter (ht->data, data_idx),
                         ht->Getter (ht->data, ht->table[hash])))
        {
          /* duplicated data */
          return HT_DUPLICATED;
        }
      else
        {
          /* hash collision, not duplicated */
          if (ht->dl > 0)
            {
              /* find the first empty slot in length delta_l */
              for (int i = -1 * ht->dl; i <= (int)ht->dl; ++i)
                {
                  ptr = ht->table + ((hash + i + ht->cap) % ht->cap);
                  if ((idx_t)-1 == *ptr)
                    {
                      *ptr = data_idx;
                      return HT_FOUND;
                    }
                  if (ht->isEqual (ht->Getter (ht->data, data_idx),
                                     ht->Getter (ht->data, *ptr)))
                    return HT_DUPLICATED;
                }
              return HT_NO_EMPTYSLOT;
            }
        }
    }
  
  return -1; /* unreachable */
}

HASHTABDEFF int
ht_idxof (HashTable *ht, char *key, size_t key_len, idx_t *result)
{
  hash_t hash = ht->Hasher (key, key_len) % ht->cap;
  idx_t *ptr = ht->table + hash;

  if ((idx_t)-1 == *ptr)
    {
      return HT_NOT_FOUND;
    }
  else
    {
      if (ht->isEqual (ht->Getter (ht->data, *ptr), key))
        {
          *result = *ptr;
          return HT_FOUND;
        }
      else if (ht->dl > 0)
        {
          for (int i = -1 * ht->dl; i <= (int)ht->dl; ++i)
            {
              ptr = ht->table + ((hash + i + ht->cap) % ht->cap);
              if ((idx_t)-1 != *ptr &&
                  ht->isEqual (ht->Getter (ht->data, *ptr), key))
                {
                  *result = *ptr;
                  return HT_FOUND;
                }
            }
        }
    }
  return HT_NOT_FOUND;
}

#endif /* HASHTAB_IMPLEMENTATION */


#ifdef HASHTAB_TEST
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

hash_t
simple_hash (const char *data, idx_t len)
{
  UNUSED (len);
  hash_t h = data[0];
  if (h >= 0x61 && h <= 0x3A)
    return h - 0x61; /* map a-z -> 0-26 */
  if (h >= 0x41 && h <= 0x5A)
    return h - 0x41; /* map A-Z -> 0-26 */
  return 0;
}

#define DO_ASSERT(msg, asserts) do {            \
    printf ("%s", msg);                         \
    asserts;                                    \
    puts ("PASS");                              \
  } while (0)

/**
 *  we can only use default definitions of GET_DATA_xx here, because
 *  they are already defined when we reach this point in compilation
 *  see documentation for more advanced usage
 *
 *  we make a hash table of length 26 (a-z) with the `simple_hash`
 *  function which maps arbitrary data into {0, ..., 26}
 */
int
main (void)
{
  static char *data[] = {
    "Hello", "hello", "World", "world", "test", "Hi", "WWW", "Www", "WXYZ"
  };
  
  HashTable t = new_hashtab (26, data, 1);
  ht_set_funs (&t, simple_hash, NULL, NULL, NULL);

  idx_t *mem = malloc (ht_sizeof (&t));
  
  if (0 != ht_init (&t, mem))
    {
      puts ("hashtab initialization failed.");
      return -1;
    }

  {
    idx_t i;

    DO_ASSERT ("- testing simple insertion... ", {
        assert (0 == ht_insert (&t, 2));
        assert (0 == ht_idxofs (&t, "World", &i));
        assert (2 == i);
      });

    DO_ASSERT ("- testing simple collision... ", {
        // Hash(WWW) = Hash(World)
        assert (0 == ht_insert (&t, 6));
        assert (0 == ht_idxofs (&t, "WWW", &i));
        assert (6 == i);
      });

    DO_ASSERT ("- testing duplicate key insertion... ", {
        /* this must fail */
        assert (HT_DUPLICATED == ht_insert (&t, 6));
        assert (0 == ht_idxofs (&t, "WWW", &i));
        assert (6 == i);
      });
    
    DO_ASSERT ("- testing last possible collision insertion... ", {
        /**
         *  we inserted `World`   -> at 22
         *  then inserted `WWW`   -> 22 is full      => at 21
         *  now we insert `Www`   -> 21, 22 are full => at 23
         */
        assert (0 == ht_insert (&t, 7));
        assert (0 == ht_idxofs (&t, "Www", &i));
        assert (7 == i);
      });
    
   DO_ASSERT ("- testing no empty slot found... ", {
       /**
        *  we try to insert "WXYZ" (Hash(WXYZ) = 22)
        *  as delta_l is 1 and slots 21,22,23 are full
        *  this must fail with error: HT_NO_EMPTYSLOT
        */
       assert (HT_NO_EMPTYSLOT == ht_insert (&t, 8));
     });
  }
  
  ht_free (&t, free (mem));
  return 0;
}

#endif /* HASHTAB_TEST */
