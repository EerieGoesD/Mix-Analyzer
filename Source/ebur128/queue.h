/*
 * Minimal singly-linked tail-queue (STAILQ) macros for platforms without
 * <sys/queue.h> (e.g. MSVC/Windows). These are the standard BSD queue.h
 * macros (public domain, originally from 4.4BSD/FreeBSD), reduced to only
 * the STAILQ operations that libebur128's ebur128.c uses.
 */
#ifndef MIX_ANALYZER_QUEUE_H
#define MIX_ANALYZER_QUEUE_H

#define STAILQ_HEAD(name, type)                                                \
  struct name {                                                                \
    struct type* stqh_first;                                                   \
    struct type** stqh_last;                                                   \
  }

#define STAILQ_ENTRY(type)                                                     \
  struct {                                                                     \
    struct type* stqe_next;                                                    \
  }

#define STAILQ_INIT(head)                                                      \
  do {                                                                         \
    (head)->stqh_first = NULL;                                                 \
    (head)->stqh_last = &(head)->stqh_first;                                   \
  } while (0)

#define STAILQ_INSERT_TAIL(head, elm, field)                                  \
  do {                                                                         \
    (elm)->field.stqe_next = NULL;                                            \
    *(head)->stqh_last = (elm);                                               \
    (head)->stqh_last = &(elm)->field.stqe_next;                              \
  } while (0)

#define STAILQ_REMOVE_HEAD(head, field)                                       \
  do {                                                                         \
    if (((head)->stqh_first = (head)->stqh_first->field.stqe_next) == NULL)   \
      (head)->stqh_last = &(head)->stqh_first;                                 \
  } while (0)

#define STAILQ_FOREACH(var, head, field)                                      \
  for ((var) = ((head)->stqh_first); (var);                                   \
       (var) = ((var)->field.stqe_next))

#define STAILQ_EMPTY(head) ((head)->stqh_first == NULL)
#define STAILQ_FIRST(head) ((head)->stqh_first)

#endif /* MIX_ANALYZER_QUEUE_H */
