/**
 *   file: linked_list.c
 *   created at: 17 Oct 2023
 *
 *   implements linked list (in Linux naming format)
 *
 *   some of the functionalities are avilable in
 *   both macro and function implementations
 *
 *
 *   compilation:
 *     to compile executable test program:
 *        cc -Wall -Wextra -o test -D LINK_TEST
 *                 -D LINK_IMPLEMENTATION linked_list.c
 *
 *     define LINK_ONLY_MACRO and remove LINK_IMPLEMENTATION
 *     to use only macro's instead of function calls.
 *
 *     to include in other c files:
 *     `
 *        #define LINK_IMPLEMENTATION
 *        #include "linked_list.c"
 *     `
 *     remove LINK_IMPLEMENTATION macro to get only headers.
 *     using implementation macro in some files and
 *     LINK_ONLY_MACRO in others, will cause compile error.
 */
#ifndef LINK__H__
#define LINK__H__

#ifndef LINKDEF
#define LINKDEF static inline
#endif

struct list_head{
  struct list_head *next;
  struct list_head *prev; 
};


/**
 *  initialize a list_head head
 */
#define INIT_LIST_HEAD(head)                            \
  head = (struct list_head){NULL, NULL};                \
  head.next = &head; head.prev=&head;

/**
 *  usage:  list_head *head = list_init_head()
 *  initialized head with this macro, should only be used
 *  by `link_add_first` macro for the first addition,
 *  otherwise it crashes your program
 */
#define LIST_INIT_HEAD_UNSAFE()                         \
  &((list_head){.next=NULL, .prev=NULL})

/* give offset of @member in struct @T (in bytes) */
#define offset_of(T, member)                            \
  (size_t)&( ((T*)0)->member )

/**
 *  get the struct @T for this entry @ptr
 *  where: `ptr = T->member`
 */
#define container_of(ptr, T, member)                    \
  ({const typeof( ((T*)0)->member ) *__ptr = (ptr);     \
    (T*)( (char*)__ptr - offset_of (T, member)); })

/**
 *  iterate over list of given type
 *  @pos -- to use as loop cursor
 *  @head - list head
 *  @member the name of the list_s within the struct
 */
#define list_for_each(pos, head, member)                \
  for (pos = container_of ((head)->next,                \
                           typeof(*pos), member);       \
       &((pos)->member) != head  &&                     \
         ((pos)->member).next != NULL;                  \
       pos = container_of (((pos)->member).next,        \
                           typeof(*pos), member))

/**
 *   it's the same as the list_for_each macro
 *   but it's *Not* safe for a broken linked list, when
 *   `pos->next` could be a NULL pointer for some position `pos`
 */
#define list_for_each_unsafe(pos, head, member)         \
  for (pos = container_of ((head)->next,                \
                           typeof(*pos), member);       \
       &((pos)->member) != head;                        \
       pos = container_of (((pos)->member).next,        \
                           typeof(*pos), member))

/* delete beginnings and endings */
#define link_del_first(head) link_del(head, (head)->next);
#define link_del_last(head) link_del(head, (head)->prev);

/**
 *  use for the first add to the link list
 *  @head is the head of the linked list
 *  @sl is a pointer to your struct's list_head
 */
#define link_add_first(head, sl)                \
  do { (head)->next = (sl);                     \
    (head)->prev = (sl);                        \
    (sl)->next = (head);                        \
    (sl)->prev = (head); } while(0)


/* when you just need macro's and don't want to use functions! */
# ifdef LINK_ONLY_MACRO
/* similar to the function with the same name */
#define link_add_end(head, sl) do{              \
    (sl)->next=(head);                          \
    (sl)->prev=(head)->prev;                    \
    ((head)->prev)->next = (sl);                \
    (head)->prev = (sl); } while(0);

/* similar to the function with the same name */
#define link_add_head(head, sl) do {            \
    (sl)->next=(head)->next;                    \
    (sl)->prev=(head);                          \
    ((head)->next)->prev = sl;                  \
    (head)->next = sl; } while(0);

#define link_add_after(pos, new) do{            \
    (new)->next = (pos)->next;                  \
    (new)->prev = pos;                          \
    ((pos)->next)->prev = new;                  \
    (pos)->next = new;} while(0);

#define link_add_before(pos, new) do{           \
    (new)->next = pos;                          \
    (new)->prev = (pos)->prev;                  \
    ((pos)->prev)->next = new;                  \
    (pos)->prev = new; } while(0);

#define link_del(head, sl)                      \
  if ((head) != (sl)) {                         \
    ((sl)->next)->prev = (sl)->prev;            \
    ((sl)->prev)->next = (sl)->next; }

# else
/**
 *  @head is the head of the list
 *  @new is a pointer to your struct's links_s
 */
LINKDEF void link_add_end(struct list_head *head, struct list_head *new);
LINKDEF void link_add_head(struct list_head *head, struct list_head *new);
/**
 *  @pos pointer to desired position
 *  @new is a pointer to your struct's links_s
 */
LINKDEF void link_add_after(struct list_head *pos, struct list_head *new);
LINKDEF void link_add_before(struct list_head *pos, struct list_head *new);
/**
 *  @entry is a pointer to your struct's list_head
 */
LINKDEF int link_del(struct list_head *head, struct list_head *entry);
# endif
#endif


/* function implementations */
#ifdef LINK_IMPLEMENTATION

LINKDEF void
link_add_head(struct list_head *head,
              struct list_head *new)
{
  new->next = head->next;
  new->prev = head;
  (head->next)->prev = new;
  head->next = new;
}

LINKDEF void
link_add_end(struct list_head *head,
             struct list_head *new)
{
  new->next = head;
  new->prev = head->prev;
  (head->prev)->next = new;
  head->prev = new;
}

LINKDEF void
link_add_after(struct list_head *pos,
               struct list_head *new)
{
  new->next = pos->next;
  new->prev = pos;
  (pos->next)->prev = new;
  pos->next = new;
}

LINKDEF void
link_add_before(struct list_head *pos,
                struct list_head *new)
{
  new->next = pos;
  new->prev = pos->prev;
  (pos->prev)->next = new;
  pos->prev = new;
}

LINKDEF int
link_del(struct list_head *head,
         struct list_head *entry)
{
  if (head == entry) return -1;

  (entry->prev)->next = entry->next;
  (entry->next)->prev = entry->prev;
  
  return 0;
}
#endif



/* the test executable program */
#ifdef LINK_TEST
#include <stdio.h>


typedef struct data{
  int la, ha;
  char p;
  struct list_head lnk;
}data;

LINKDEF void
print_data(struct data *d)
{
  printf("@%p -- {.p=%c .a=%d%d .l=%p}\n",
         d, d->p, d->la, d->ha, &(d->lnk));
}


int
main(void)
{
  data *d;
  data d1,d2,d3,d4;
  struct list_head head; 

  d1 = (data) { 1, 2, .p='a', .lnk=(struct list_head){} };
  d2 = (data) { 2, 3, .p='b', .lnk=(struct list_head){} };
  d3 = (data) { 3, 4, .p='c', .lnk=(struct list_head){} };
  d4 = (data) { 4, 5, .p='d', .lnk=(struct list_head){} };

  /* initialize the linked list */
  INIT_LIST_HEAD (head);

  
  puts("/* addition test ********************************/");
  /* final order: `d`, `a`, `b`, `c` */
  link_add_head (&head, &(d1.lnk));  // add a
  link_add_head (&head, &(d4.lnk));  // add d
  link_add_end (&head, &(d2.lnk));   // add b
  link_add_end (&head, &(d3.lnk));   // add c

  list_for_each (d, &head, lnk)
    {
      print_data (d);
    }

  
  puts("\n/* link addr test *******************************/");
  list_for_each (d, &head, lnk)
    {
      printf ("%c.l: {.prev=%p  .next=%p}\n",
              d->p, (d->lnk).prev, (d->lnk).next);
    }

  
  puts("\n/* deletion test ********************************/");
  /* final result: only has `b` */
  link_del_first (&head);       // del d
  link_del_last (&head);        // del c
  link_del (&head, &(d1.lnk));  // del a

  list_for_each (d, &head, lnk)
    {
      print_data (d);
    }
  
  
  puts("\n/* addition after free test *********************/");
  /* this will make the list empty */
  link_del_first (&head);
  link_add_end (&head, &(d3.lnk));   // add c
  
  list_for_each (d, &head, lnk)
    {
      print_data (d);
    }


  puts("\n/* add before&after test ************************/");
  /* get a pointer to the last element of the list */
  struct list_head *p = &(d3.lnk);

  /* final order: `a`, `b`, `c`, `d` */
  link_add_before (p, &(d2.lnk));        // add b before c
  link_add_after (p, &(d4.lnk));         // add d after c 
  link_add_before (p->prev, &(d1.lnk));  // add a before b==p->prev

  list_for_each (d, &head, lnk)
    {
      print_data (d);
    }

  return 0;
}
#endif
