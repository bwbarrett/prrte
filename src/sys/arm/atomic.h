/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2010      IBM Corporation.  All rights reserved.
 * Copyright (c) 2010      ARM ltd.  All rights reserved.
 * Copyright (c) 2017-2018 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2019      Intel, Inc.  All rights reserved.
 * Copyright (c) 2020      Cisco Systems, Inc.  All rights reserved
 * Copyright (c) 2021      Nanook Consulting.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/*
 * ARMv5 and earlier lack robust atomic operations and therefore this file uses
 * Linux kernel support where needed.  The kernel also provides memory barriers
 * and this file uses them for ARMv5 and earlier processors, which lack the
 * memory barrier instruction.  These kernel functions are available on kernel
 * versions 2.6.15 and greater; using them will result in undefined behavior on
 * older kernels.
 * See Documentation/arm/kernel_user_helpers.txt in the kernel tree for details
 */

#ifndef PRTE_SYS_ARCH_ATOMIC_H
#define PRTE_SYS_ARCH_ATOMIC_H 1

#if (PRTE_ASM_ARM_VERSION >= 7)

#    define PRTE_HAVE_ATOMIC_MEM_BARRIER 1
/* use the DMB instruction if available... */

#    define MB()  __asm__ __volatile__("dmb" : : : "memory")
#    define RMB() __asm__ __volatile__("dmb" : : : "memory")
#    define WMB() __asm__ __volatile__("dmb" : : : "memory")

#elif (PRTE_ASM_ARM_VERSION == 6)

#    define PRTE_HAVE_ATOMIC_MEM_BARRIER 1
/* ...or the v6-specific equivalent... */

#    define MB()  __asm__ __volatile__("mcr p15, 0, r0, c7, c10, 5" : : : "memory")
#    define RMB() MB()
#    define WMB() MB()

#else

#    define PRTE_HAVE_ATOMIC_MEM_BARRIER 1
/* ...otherwise use the Linux kernel-provided barrier */

#    define MB()  (*((void (*)(void))(0xffff0fa0)))()
#    define RMB() MB()
#    define WMB() MB()

#endif

/**********************************************************************
 *
 * Memory Barriers
 *
 *********************************************************************/

#if (PRTE_HAVE_ATOMIC_MEM_BARRIER == 1)

static inline void prte_atomic_mb(void)
{
    MB();
}

static inline void prte_atomic_rmb(void)
{
    RMB();
}

static inline void prte_atomic_wmb(void)
{
    WMB();
}

static inline void prte_atomic_isync(void)
{
}

#endif

/**********************************************************************
 *
 * Atomic math operations
 *
 *********************************************************************/

#if (PRTE_GCC_INLINE_ASSEMBLY && (PRTE_ASM_ARM_VERSION >= 6))

#    define PRTE_HAVE_ATOMIC_COMPARE_EXCHANGE_32 1
#    define PRTE_HAVE_ATOMIC_MATH_32             1
static inline bool prte_atomic_compare_exchange_strong_32(prte_atomic_int32_t *addr,
                                                          int32_t *oldval, int32_t newval)
{
    int32_t prev, tmp;
    bool ret;

    __asm__ __volatile__("1:  ldrex   %0, [%2]        \n"
                         "    cmp     %0, %3          \n"
                         "    bne     2f              \n"
                         "    strex   %1, %4, [%2]    \n"
                         "    cmp     %1, #0          \n"
                         "    bne     1b              \n"
                         "2:                          \n"

                         : "=&r"(prev), "=&r"(tmp)
                         : "r"(addr), "r"(*oldval), "r"(newval)
                         : "cc", "memory");

    ret = (prev == *oldval);
    *oldval = prev;
    return ret;
}

/* these two functions aren't inlined in the non-gcc case because then
   there would be two function calls (since neither cmpset_32 nor
   atomic_?mb can be inlined).  Instead, we "inline" them by hand in
   the assembly, meaning there is one function call overhead instead
   of two */
static inline bool prte_atomic_compare_exchange_strong_acq_32(prte_atomic_int32_t *addr,
                                                              int32_t *oldval, int32_t newval)
{
    bool rc;

    rc = prte_atomic_compare_exchange_strong_32(addr, oldval, newval);
    prte_atomic_rmb();

    return rc;
}

static inline bool prte_atomic_compare_exchange_strong_rel_32(prte_atomic_int32_t *addr,
                                                              int32_t *oldval, int32_t newval)
{
    prte_atomic_wmb();
    return prte_atomic_compare_exchange_strong_32(addr, oldval, newval);
}

#    if (PRTE_ASM_SUPPORT_64BIT == 1)

#        define PRTE_HAVE_ATOMIC_COMPARE_EXCHANGE_64 1
static inline bool prte_atomic_compare_exchange_strong_64(prte_atomic_int64_t *addr,
                                                          int64_t *oldval, int64_t newval)
{
    int64_t prev;
    int tmp;
    bool ret;

    __asm__ __volatile__("1:  ldrexd  %0, %H0, [%2]           \n"
                         "    cmp     %0, %3                  \n"
                         "    it      eq                      \n"
                         "    cmpeq   %H0, %H3                \n"
                         "    bne     2f                      \n"
                         "    strexd  %1, %4, %H4, [%2]       \n"
                         "    cmp     %1, #0                  \n"
                         "    bne     1b                      \n"
                         "2:                                    \n"

                         : "=&r"(prev), "=&r"(tmp)
                         : "r"(addr), "r"(*oldval), "r"(newval)
                         : "cc", "memory");

    ret = (prev == *oldval);
    *oldval = prev;
    return ret;
}

/* these two functions aren't inlined in the non-gcc case because then
   there would be two function calls (since neither cmpset_64 nor
   atomic_?mb can be inlined).  Instead, we "inline" them by hand in
   the assembly, meaning there is one function call overhead instead
   of two */
static inline bool prte_atomic_compare_exchange_strong_acq_64(prte_atomic_int64_t *addr,
                                                              int64_t *oldval, int64_t newval)
{
    bool rc;

    rc = prte_atomic_compare_exchange_strong_64(addr, oldval, newval);
    prte_atomic_rmb();

    return rc;
}

static inline bool prte_atomic_compare_exchange_strong_rel_64(prte_atomic_int64_t *addr,
                                                              int64_t *oldval, int64_t newval)
{
    prte_atomic_wmb();
    return prte_atomic_compare_exchange_strong_64(addr, oldval, newval);
}

#    endif

#    define PRTE_HAVE_ATOMIC_ADD_32 1
static inline int32_t prte_atomic_fetch_add_32(prte_atomic_int32_t *v, int inc)
{
    int32_t t, old;
    int tmp;

    __asm__ __volatile__("1:  ldrex   %1, [%3]        \n"
                         "    add     %0, %1, %4      \n"
                         "    strex   %2, %0, [%3]    \n"
                         "    cmp     %2, #0          \n"
                         "    bne     1b              \n"

                         : "=&r"(t), "=&r"(old), "=&r"(tmp)
                         : "r"(v), "r"(inc)
                         : "cc", "memory");

    return old;
}

#    define PRTE_HAVE_ATOMIC_SUB_32 1
static inline int32_t prte_atomic_fetch_sub_32(prte_atomic_int32_t *v, int dec)
{
    int32_t t, old;
    int tmp;

    __asm__ __volatile__("1:  ldrex   %1, [%3]        \n"
                         "    sub     %0, %1, %4      \n"
                         "    strex   %2, %0, [%3]    \n"
                         "    cmp     %2, #0          \n"
                         "    bne     1b              \n"

                         : "=&r"(t), "=&r"(old), "=&r"(tmp)
                         : "r"(v), "r"(dec)
                         : "cc", "memory");

    return t;
}

#endif

#endif /* ! PRTE_SYS_ARCH_ATOMIC_H */
