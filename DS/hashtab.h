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

/** file: hashtab.h
    created on: 25 Jun 2024
  
    A Simple Hash Table Implementation
  
    by default it uses uint32_t for indices and hash values
    insert and lookup keys, by index, in O(1) time and memory
    or make a hash table of an arbitrary struct, then
    access their indices via key (see the example program)
  
    Compilation:
      to compile the test program:
      cc -x c -ggdb -Wall -Wextra -Werror \
         -D HASHTAB_IMPLEMENTATION \
         -D HASHTAB_TEST hashtab.h -o test.out
  
      to compile the example program:
      cc -x c -ggdb -Wall -Wextra -Werror \
         -D HASHTAB_IMPLEMENTATION \
         -D HASHTAB_EXAMPLE hashtab.h -o test.out -lreadline
  
      to include in c files:
      (see the example program for more advanced usage)
      ```c
      #define HASHTAB_IMPLEMENTATION
      #include "hashtab.h"
  
      int
      main (void)
      {
        // allocate your data structure
        // see examples
        Type data = ...;
  
        // allocate hashtab
        // where: capacity=10, delta_l=1
        HashTable t = new_hashtab (10, data, 1);
  
        // set hasher and is_equal functions
        // pass them NULL to use defaults
        ht_set_funs (&t, NULL, NULL);
  
        // allocate main memory to store hashes
        // you might use mmap
        idx_t *mem = malloc (ht_sizeof (&t));
        if (0 != ht_init (&t, mem))
          {
            puts ("hashtab initialization failed.");
            return -1;
          }
  
        {
          // do something here
          // use ht_insert, ht_idxof functions
        }
  
        // free the hash table
        ht_free (&t, free (mem));
        return 0;
      }
      ```
 **/
#ifndef HASHTAB__H__
#define HASHTAB__H__
#include <string.h>
#include <stdint.h>
#include <stddef.h>
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
  HT_NO_EMPTYSLOT
};
HASHTABDEFF const char *ht_strerr (enum ht_error_t err);

/**
 *  FNV-1a: a simple hash function
 *  used as the default hasher
 */
HASHTABDEFF hash_t hash_FNV_1a (const char *data, idx_t len);

struct keytab_t {
  DATA_T *key; /* does not have to be string */
  idx_t len; /* length of key */
};
#define NEW_KEY(k,l) (struct keytab_t){.key=k, .len=l}
#define KEYS(str) NEW_KEY (str, strlen (str))

typedef hash_t (*ht_hasher)(const char *data, idx_t len);
typedef int (*ht_isequal)(const DATA_T *restrict k1, idx_t l1,
                          const DATA_T *restrict k2, idx_t l2);

struct hashtab_t {
  idx_t *table;
  idx_t cap;
  idx_t dl; /* delta_l */

  DATA_T *head; /* pointer to data */
  int __data_size; /* length of pointers in head */
  int __key_offset; /* offset of keytab_t in each head[index] */

  ht_isequal isEqual;
  ht_hasher Hasher;
};
typedef struct hashtab_t HashTable;

#define ht_sizeof(ht) ((ht)->cap * sizeof (idx_t))

#define new_hashtab(table_len, data_ptr, delta_l)                       \
  (HashTable){.cap=(idx_t)(table_len), .dl=(idx_t)(delta_l),            \
      .head=(DATA_T*)data_ptr,                                          \
      .__data_size=sizeof (data_ptr[0]),                                \
      .__key_offset=0                                                   \
    }
#define new_hashtab_t(table_len, data_ptr, delta_l, T, memb)            \
  (HashTable){.cap=(idx_t)(table_len), .dl=(idx_t)(delta_l),            \
      .head=(DATA_T*)data_ptr,                                          \
      .__data_size=sizeof (data_ptr[0]),                                \
      .__key_offset=offsetof (T, memb)                                  \
    }

#define ht_set_funs(ht, hasher, isequal) do {                   \
    (ht)->Hasher= hasher;                                       \
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
/* internal macro to get data at index @i */
#define __GET_K(ht, i) \
  (*(char **)((ht)->head + (i)*(ht)->__data_size + (ht)->__key_offset))
/* internal macro to get length of key */
#define __LEN_K(ht, i)                                                  \
  (*(idx_t *)((ht)->head + (i)*(ht)->__data_size                        \
              + (ht)->__key_offset + offsetof (struct keytab_t, len)))

HASHTABDEFF const char *
ht_strerr (enum ht_error_t err)
{
  switch (err)
    {
    case HT_NOT_FOUND:
      return "Not Found";
    case HT_NO_EMPTYSLOT:
      return "No empty slot left";
    case HT_DUPLICATED:
      return "Duplicate Key";

    case HT_FOUND:
      return NULL;
    default:
      return "unknown error code";
    }
}

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

/* internal - default isequal function */
static inline int
__default_isequal(const DATA_T *restrict k1, idx_t l1,
                  const DATA_T *restrict k2, idx_t l2)
{
  if (!k1 || !k2 || l1 != l2)
    return false;
  return 0 == strcmp(k1, k2);
}

HASHTABDEFF int
ht_init (HashTable *ht, hash_t *buf)
{
  if (NULL == ht || NULL == buf
      || NULL == ht->head || ht->cap == 0)
    return -1;

  memset (buf, 0xFF, ht_sizeof (ht));
  ht->table = buf;

  if (NULL == ht->Hasher)
    ht->Hasher = &hash_FNV_1a;
  if (NULL == ht->isEqual)
    ht->isEqual = &__default_isequal;
  if (ht->dl > ht->cap)
    ht->dl = ht->cap;

  return 0;
}

/* internal function */
static inline hash_t
__do_hash (HashTable *t, idx_t i)
{
  if ((idx_t)-1 == i)
    return -1;
  return t->Hasher (__GET_K(t, i), __LEN_K(t, i)) % t->cap;
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
          ht->isEqual (__GET_K(ht, data_idx), __LEN_K(ht, data_idx),
                       __GET_K(ht, *ptr),  __LEN_K(ht, *ptr)))
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
                  if (ht->isEqual (__GET_K(ht, data_idx), __LEN_K(ht, data_idx),
                                   __GET_K(ht, *ptr),  __LEN_K(ht, *ptr)))
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
      if (ht->isEqual (__GET_K(ht, *ptr), __LEN_K(ht, *ptr), key, key_len))
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
                  ht->isEqual (__GET_K(ht, *ptr), __LEN_K(ht, *ptr), key, key_len))
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


/* the test program */
#ifdef HASHTAB_TEST
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

hash_t
simple_hash (const char *data, idx_t len)
{
  UNUSED (len);
  hash_t h = data[0];
  if (h >= 0x61 && h <= 0x7A)
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


int
main (void)
{
  struct keytab_t data[] = {
    KEYS ("Hello"), KEYS ("hello"), KEYS ("World"),
    KEYS ("world"), KEYS ("test"),  KEYS ("Hi"),
    KEYS ("WWW"),   KEYS ("Www"),   KEYS ("WXYZ"),
  };
  
  HashTable t = new_hashtab (26, data, 1);
  ht_set_funs (&t, simple_hash, NULL);

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

    DO_ASSERT ("- testing simple insertion collision... ", {
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
    
    DO_ASSERT ("- testing last possible insertion collision... ", {
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


/* the example program */
#ifdef HASHTAB_EXAMPLE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_WCOUNT 42 /* maximum word count */

typedef struct {
  char *word;
  int count;

  /* to make a hash table */
  struct keytab_t k;
} WordCounter;

static inline int
wcounter_isequal (const DATA_T *restrict k1, idx_t l1,
                  const DATA_T *restrict k2, idx_t l2)
{
  if (!k1 || !k2 || l1 != l2)
    return false;

  /* k1 and k2 are WordCounter->k->key */
  return 0 == strncmp (k1, k2, (l1<l2) ? l1 : l2);
}

#ifdef _USE_DUMMY_HASH
hash_t
dummy_hasher (const char *data, idx_t len)
{
  UNUSED (len);
  return (hash_t) data[0];
}
#endif /* _USE_DUMMY_HASH */

int
main (void)
{
  WordCounter *data = malloc (MAX_WCOUNT * sizeof (WordCounter));
  /**
   *  new hashtab induced from WordCounter type
   *  @k is the name of `struct keytab_t` in WordCounter
   */
  HashTable t = new_hashtab_t (32, data, 1, WordCounter, k);

#ifndef _USE_DUMMY_HASH
  ht_set_funs (&t, NULL, wcounter_isequal);
#else
  ht_set_funs (&t, dummy_hasher, wcounter_isequal);
#endif /* _USE_DUMMY_HASH */

  idx_t *mem = malloc (ht_sizeof (&t));
  if (0 != ht_init (&t, mem))
    {
      puts ("hashtab initialization failed.");
      return -1;
    }

  /* the last added word to the data array */
  int end_idx = 0;
  WordCounter *end = data;

  char *__p;
  puts ("HashTab example program!\n"
        "enter words to be added, press C-d to break\n");
 MAIN_LOOP:
  while ((__p = readline ("Key> ")))
    { 
      if (strlen (__p) == 0)
        {
          free (__p);
          continue;
        }

      int ret;
      idx_t i;
      if ((ret = ht_idxofs (&t, __p, &i)) == HT_NOT_FOUND)
        {
          if (end_idx >= MAX_WCOUNT)
            {
              puts ("End Of Memory!");
              free (__p);
            }
          else
            {
              /* create a new word counter at the end of the array */
              *(end++) = (WordCounter){
                .word = __p,
                .count = 1,
                .k = {.key=__p, .len=strlen (__p)}
              };
              /* insret it to the table */
              if ((ret = ht_insert (&t, end_idx++)) != 0)
                {
                  printf ("Insertion Failed -- %s\n", ht_strerr (ret));
                  end--;
                  end_idx--;
                }
              else
                printf ("Key `%s` was added\n", __p);
            }
        }
      else if (ret == HT_FOUND)
        {
          /* a word counter has been found by this key */
          WordCounter *w = data + i;
          printf ("Key `%s` was incremented, count: %d\n", w->word, ++w->count);
          free (__p);
        }
    }

  if (end_idx)
    puts ("Statistics:");
  else
    puts ("empty table.");
  for (int i = 0; i < end_idx; ++i)
    {
      WordCounter *w = data + i;
      if (w->word)
        printf ("Key: `%s` \t Count: %d\n", w->word, w->count);
    }

  __p = readline ("continue (Y/n)? ");
  if (__p)
    {
      if (__p[0] != 'n' && __p[0] != 'N')
        {
          free(__p);
          goto MAIN_LOOP;
        }
      else
        free (__p);
    }

  /* free allocated memory by readline */
  for (WordCounter *w = data; end_idx != 0; --end_idx, ++w)
    {
      if (w->word)
        free (w->word);
    }
  ht_free (&t, free (mem));
  return 0;
}

#endif /* HASHTAB_EXAMPLE */
