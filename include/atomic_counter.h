/*
** atomic_counter.h
** 
** Made by Jens Gustedt
** Login   <gustedt@damogran.loria.fr>
** 
** Started on  Sat Jul 10 12:08:15 2010 Jens Gustedt
** Last update Sat Jul 10 12:08:15 2010 Jens Gustedt
*/

#ifndef   	ATOMIC_COUNTER_H_
# define   	ATOMIC_COUNTER_H_

#include "orwl_enum.h"
#include "orwl_thread.h"
#include "orwl_posix_default.h"

DECLARE_STRUCT(atomic_counter);

/**
 ** @brief A structure that holds a thread safe counter.
 **
 ** The value of this counter will always be greater or equal to zero.
 ** @see atomic_counter_wait for a routing that allows to wait that
 ** the counter is zero again.
 **
 ** @warning Calls to ::atomic_counter_inc and ::atomic_counter_dec
 ** must always match up during the execution of the code.
 **/
struct atomic_counter {
  sem_t atomic_pos;
  sem_t atomic_neg;
  sem_t atomic_mut;
};

DECLARE_ONCE(atomic_counter);

PROTOTYPE(atomic_counter*, atomic_counter_init, atomic_counter*, int, unsigned);
FSYMB_DOCUMENTATION(atomic_counter_init)
#define atomic_counter_init(...) CALL_WITH_DEFAULTS(atomic_counter_init, 3, __VA_ARGS__)
DECLARE_DEFARG(atomic_counter_init, , PTHREAD_PROCESS_PRIVATE, 0u);

inline
PROTOTYPE(void, atomic_counter_inc, atomic_counter*);

inline
PROTOTYPE(void, atomic_counter_dec, atomic_counter*);

inline
PROTOTYPE(unsigned, atomic_counter_getvalue, atomic_counter*);

inline
PROTOTYPE(void, atomic_counter_wait, atomic_counter*);

/**
 ** @brief Get the value of the counter.
 **
 ** @warning The value that is returned is not necessarily valid at
 ** the time of return. It may have changed already.
 **/
inline
unsigned atomic_counter_getvalue(atomic_counter* p) {
  unsigned atomic_pos = 1;
  unsigned atomic_neg = 1;
  INVARIANT(sem_assert(&p->atomic_neg) < 2u)
    INVARIANT(sem_assert(&p->atomic_mut) < 2u) {
    while (branch_expect(atomic_pos == atomic_neg, false)) {
      SEM_CRITICAL(p->atomic_mut) {
        sem_getvalue0(&p->atomic_pos, &atomic_pos);
        sem_getvalue0(&p->atomic_neg, &atomic_neg);
      }
    }
  }
  return (atomic_pos + 1) - atomic_neg;
}

/**
 ** @brief Increment the data of @a p atomically.
 **
 ** This should be relatively efficient in the general case. In the
 ** particular case that the count was zero before, execution may slow
 ** down a bit.
 **
 ** @warning Calls to ::atomic_counter_inc and ::atomic_counter_dec
 ** must always mach up during the execution of the code.
 **/
inline
void atomic_counter_inc(atomic_counter* p) {
  INVARIANT(sem_assert(&p->atomic_neg) < 2u)
    INVARIANT(sem_assert(&p->atomic_mut) < 2u) {
    sem_post(&p->atomic_pos);
    if (branch_expect(sem_trywait_nointr(&p->atomic_neg) != 0, true)) {
      if (errno) {
        assert(errno == EAGAIN);
        errno = 0;
      }
    } else {
      assert(!sem_assert(&p->atomic_neg));
      // We just got atomic_neg so we are the first
      SEM_CRITICAL(p->atomic_mut) {
        if (sem_trywait_nointr(&p->atomic_pos)) {
          assert(EAGAIN || EINVAL);
        }
      }
    }
  }
}

/**
 ** @brief Decrement the data of @a p atomically.
 **
 ** This should be relatively efficient in the general case. In the
 ** particular case that this is the last ::atomic_counter_dec before
 ** the counter falls down to zero again, execution may slow down a
 ** bit.
 **
 ** @warning Calls to ::atomic_counter_inc and ::atomic_counter_dec
 ** must always mach up during the execution of the code.
 **/
inline
void atomic_counter_dec(atomic_counter* p) {
  INVARIANT(sem_assert(&p->atomic_neg) < 2u)
    INVARIANT(sem_assert(&p->atomic_mut) < 2u) {
    if (branch_expect(sem_trywait_nointr(&p->atomic_pos) != 0, false)) {
      assert(errno == EAGAIN);
      errno = 0;
      SEM_CRITICAL(p->atomic_mut) {
        if (sem_trywait_nointr(&p->atomic_pos)) {
          assert(errno == EAGAIN);
          errno = 0;
          assert(!sem_assert(&p->atomic_neg));
          sem_post(&p->atomic_neg);
        }
      }
    }
  }
}

/**
 ** @brief Wait for the counter to fall back to zero.
 **/
inline
void atomic_counter_wait(atomic_counter* p) {
  for (bool unfinished = true; unfinished;) {
    INVARIANT(sem_assert(&p->atomic_neg) < 2u)
      INVARIANT(sem_assert(&p->atomic_mut) < 2u) {
      sem_wait_nointr(&p->atomic_neg);
      SEM_CRITICAL(p->atomic_mut) {
        unfinished = (sem_trywait_nointr(&p->atomic_pos) == 0);
        if (!unfinished) {
          assert(errno == EAGAIN);
          errno = 0;
          assert(!sem_assert(&p->atomic_neg));
          sem_post(&p->atomic_neg);
        }
      }
    }
  }
}

/**
 ** @brief Account the @c atomic_counter @a COUNT during execution of a
 ** dependent block or statement.
 **/
DOCUMENT_BLOCK
#define ACCOUNT(COUNT)                          \
SAVE_BLOCK(                                     \
           atomic_counter*,                     \
           _count,                              \
           &(COUNT),                            \
           atomic_counter_inc(_count),          \
           atomic_counter_dec(_count))

// help doxy

#endif 	    /* !ATOMIC_COUNTER_H_ */