/* $Revision: 14354 $ $Date: 2013-12-17 13:58:18 -0800 (Tue, 17 Dec 2013) $
 * Copyright (c) Bullseye Testing Technology
 * This source file contains confidential proprietary information.
 *
 * SYNOPSIS
 *   typedef ... cov_atomic_t;
 *   cov_atomic_t lock = cov_atomic_initializer;
 *   bool cov_atomic_tryLock(cov_atomic_t*)
 *   void cov_atomic_unlock(cov_atomic_t*)
 *
 * DESCRIPTION
 *   This header declares an atomic locking capability.
 *   Type cov_atomic_t declares a lock variable.
 *   Expression cov_atomic_initializer is of type cov_atomic_t and
 *   initializes a lock variable to the unlocked state.
 *   Function cov_atomic_tryLock attempts to obtain the lock and returns
 *   true if the lock was obtained.
 *   If the lock is not available, the function returns false immediately.
 *   Function cov_atomic_unlock sets the lock variable to the unlocked state.
 */

#if __TI_COMPILER_VERSION__
	/* Work around bug in TI compiler, observed in TI ARM C/C++ Compiler v5.0.1
	//   "warning #427-D: dollar sign ("$") used in identifier"
	*/
	#pragma diag_suppress 427
#endif

#if (_MSC_VER || defined(__LCC__) || __MWERKS__) && _M_IX86
	/* Avoid using InterlockedExchange for embedded systems */
	/* Compiler barrier not needed for Microsoft, volatile objects have barrier semantics */
	typedef volatile long cov_atomic_t;
	#define cov_atomic_initializer 1

	/*lint -e{530,715,818} */
	static __inline int __stdcall cov_atomic_tryLock(cov_atomic_t* p)
	{
		int newValue;
		__asm {
			sub eax,eax
			mov ebx,p
			xchg eax,[ebx]
			mov newValue,eax
		};
		return newValue;
	}

	static __inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
/*---------------------------------------------------------------------------*/
#elif _XBOX
	typedef volatile long cov_atomic_t;
	#define cov_atomic_initializer { 1 }

	static __inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		return InterlockedCompareExchangeAcquire(p, 0, 1);
	}

	static __inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		InterlockedCompareExchangeRelease(p, 1, 0);
	}
/*---------------------------------------------------------------------------*/
#elif _WIN32 && !__GNUC__
	typedef volatile long cov_atomic_t;
	#define cov_atomic_initializer 1

	static __inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		#if UNDER_CE && WINVER <= 0x500
			return InterlockedExchange((long*)p, 0);
		#else
			return InterlockedExchange(p, 0);
		#endif
	}

	static __inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
/*---------------------------------------------------------------------------*/
#elif (__amd64 || __i386 || __i386__ || __x86_64) && __GNUC__
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int newValue = 0;
		__asm__ volatile ("\n"
			#if __amd64 || __x86_64
				"mfence\n"
			#endif
			"xchgl %0,%1\n"
			: "=r" (newValue) : "m" (*p), "0" (newValue) : "memory");
		return newValue;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
		__asm__ volatile ("" ::: "memory");
	}
/*---------------------------------------------------------------------------*/
#elif __hppa__ && __GNUC__
	/* See comments for __hpux */
	typedef volatile struct { int i0, i1, i2, i3; } cov_atomic_t;
	#define cov_atomic_initializer { 1, 1, 1, 1 }

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		const unsigned long mask = 0xf;
		int x;
		volatile char* p1 =
			(volatile char*)(((long)p + mask) & ~mask);
		/* Make the cache line "dirty" to improve performance */
		*p1 = '\0';
		__asm__ volatile("ldcw 0(%1),%0" : "=r" (x) : "r" (p1));
		return x;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		p->i0 = 1;
		p->i1 = 1;
		p->i2 = 1;
		p->i3 = 1;
		__asm__ volatile ("" ::: "memory");
	}
/*---------------------------------------------------------------------------*/
#elif __hppa && __hpux /* __HP_cc symbol not available on 10.20 */
	/* Based on:
	 * Implementing Spinlocks on the Intel Itanium Architecture and PA-RISC
	 * June 30, 2003 Version 1.0
	 * http://h21007.www2.hp.com/portal/download/files/unprot/itanium/spinlocks.pdf
	 */

	/* No PA-RISC processor implements reordering of loads and stores, so no memory barrier needed */
	/* ldcws operand must be 16-byte aligned, it is easiest to just reserve
	 * enough space to make sure a 16-byte boundary is in there, let the
	 * locking code figure out where it is.  This assumes an int is 4-byte
	 * aligned.
	 */
	typedef volatile struct { int i0, i1, i2, i3; } cov_atomic_t;
	#define cov_atomic_initializer { 1, 1, 1, 1 }

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		const unsigned long mask = 0xf;
		register int x;
		register volatile char* p1 =
			(volatile char*)(((long)p + mask) & ~mask);
		/* Make the cache line "dirty" to improve performance */
		*p1 = '\0';
		_asm("LDCWS", 0, 0, p1, x);
		return x;
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		p->i0 = 1;
		p->i1 = 1;
		p->i2 = 1;
		p->i3 = 1;
	}
/*---------------------------------------------------------------------------*/
#elif __ia64 && __HP_cc
	/* Based on:
	 * Implementing Spinlocks on the Intel Itanium Architecture and PA-RISC
	 * June 30, 2003 Version 1.0
	 * http://h21007.www2.hp.com/portal/download/files/unprot/itanium/spinlocks.pdf
	 *
	 * external reference specification
	 * inline assembly for Itanium-based HP-UX
	 * November 2005
	 */
	#include <machine/sys/inline.h>

	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		_Asm_mf();
		return _Asm_xchg(_SZ_W, p, 0, _LDHINT_NONE);
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
		_Asm_sched_fence();
	}
/*---------------------------------------------------------------------------*/
#elif __ia64 && __GNUC__
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int newValue = 0;
		int result;
		__asm__ volatile ("\n"
			"mf\n"
			"xchg4 %0=[%1],%2\n"
			: "=r" (result) : "r" (p), "r" (newValue) : "memory");
		return result;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
		__asm__ volatile ("" ::: "memory");
	}
/*---------------------------------------------------------------------------*/
#elif __SUNPRO_C || __SUNPRO_CC
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	/* Remainder of implementation in atomic-*-solaris.s */
	int cov_atomic_tryLock(cov_atomic_t* p);

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
/*---------------------------------------------------------------------------*/
#elif __sparc__ && __GNUC__
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int x = 0;
		__asm__ volatile ("\n"
			"membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore\n"
			"swap [%2],%0\n"
			: "=&r" (x) : "0" (x), "r" (p));
		return x;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
		__asm__ volatile ("" ::: "memory");
	}
/*---------------------------------------------------------------------------*/
#elif _POWER && _AIX
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	/* Remainder of implementation in atomic-ppc-aix.s */
	int cov_atomic_tryLock(cov_atomic_t* p);

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
/*---------------------------------------------------------------------------*/
#elif (_POWER || __PPC || __ppc__ || __POWERPC__ || __powerpc__) && __GNUC__
	/* See atomic-ppc-aix.s for references */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		__asm__ volatile ("\n"
			"	sync\n"
			"1:	lwarx %0,0,%2\n"
			"	stwcx. %3,0,%2\n"
			"	bne- 1b\n"
			: "=&r" (result), "=m" (*p)
			: "r" (p), "r" (0), "m" (*p)
			: "cc", "memory");
		return result;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
		__asm__ volatile ("" ::: "memory");
	}
/*---------------------------------------------------------------------------*/
#elif __POWERPC__ && __CWCC__
	/* Freescale C/C++ Compiler for Embedded PowerPC
	 * Last reviewed: Dec 2009 with version 4.2.0.134
	 */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static inline int cov_atomic_tryLock(register cov_atomic_t* p)
	{
		register int result;
		register int zero = 0;
		asm {
			sync
			L1:
			lwarx result,0,p
			stwcx. zero,0,p
			bne- L1
		}
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
/*---------------------------------------------------------------------------*/
#elif __ARMCC_VERSION
	/* ARM RealView Compilation Tools
	 * Last reviewed: Oct 2009 with version 4.0
	 * Inline assembly (__asm { }) not supported with Thumb and Thumb-2
	 * Embedded assembly (__asm function) not supported in version 1.2
	 */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	/* If Thumb mode (not Thumb-2) */
	#if __thumb__ && __TARGET_ARCH_THUMB < 4
		#error Thumb not supported, compile this file in ARM mode
	#endif

	/* If ARMv6 or later, or Thumb-2 or later */
	#if __TARGET_ARCH_ARM >= 6 || __TARGET_ARCH_THUMB >= 4
		static __asm int cov_atomic_tryLock(cov_atomic_t* p)
		{
			mov r1,#0
			#if __TARGET_ARCH_ARM == 6
				mcr p15,0,r1,c7,c10,5
			#else
				dmb
			#endif
			/* Label must be in column 0 */
1
			ldrex r2,[r0]
			strex r3,r1,[r0]
			cmp r3,#0
			bne %B1
			mov r0,r2
			bx lr
		}

		static __inline void cov_atomic_unlock(cov_atomic_t* p)
		{
			*p = 1;
		}
	#else
		static __inline int cov_atomic_tryLock(cov_atomic_t* p)
		{
			int result;
			__asm {
				swp result,0,[p]
			}
			return result;
		}

		static __inline void cov_atomic_unlock(cov_atomic_t* p)
		{
			/* swp may bypass the cache, so it must be used to unlock as well */
			int tmp;
			__asm {
				swp tmp,1,[p]
			}
		}
	#endif
/*---------------------------------------------------------------------------*/
#elif __arm__ && __GNUC__
	/* Processor macros determined from gcc-4.4.1/gcc/config/arm/arm.c */
	/* ARM1136JF-S and ARM1136J-S Technical Reference Manual: Example of LDREX and STREX usage */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	#define ARMv2345 __ARM_ARCH_2__ || __ARM_ARCH_3__ || __ARM_ARCH_3M__ || __ARM_ARCH_4__ || __ARM_ARCH_4T__ || \
		__ARM_ARCH_5__ || __ARM_ARCH_5E__ || __ARM_ARCH_5T__ || __ARM_ARCH_5TE__ || __ARM_ARCH_5TEJ__

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		#if __thumb__ && !__thumb2__
			#error Thumb not supported, compile this file in ARM mode
		#elif ARMv2345
			__asm__ volatile("swp %0,%1,[%2]" : "=&r" (result) : "r" (0), "r" (p));
		#else
			int tmp;
			__asm__ volatile(
				#if __ARM_ARCH_6J__ || __ARM_ARCH_6K__ || __ARM_ARCH_6M__ || __ARM_ARCH_6T2__ || \
					__ARM_ARCH_6ZK__ || __ARM_ARCH_6Z__ || __ARM_ARCH_6__
					/* ARM Architecture Reference Manual Issue I: B6.6.5 Register 7: cache management functions */
					"	mcr p15,0,%3,c7,c10,5\n"
				#else
					"	dmb\n"
				#endif
				"1:	ldrex %0,[%2]\n"
				#if __ARM_ARCH_7__ || __ARM_ARCH_7M__
					/* binutils <= 2.17 wrongly rejects strex,
					//   fixed in rev 1.328 of gas/config/tc-arm.c, 31 May 2007
					*/
					"	.arch armv6t2\n"
				#endif
				"	strex %1,%3,[%2]\n"
				#if __ARM_ARCH_7__
					"	.arch armv7\n"
				#elif __ARM_ARCH_7M__
					"	.arch armv7m\n"
				#endif
				"	cmp %1,#0\n"
				"	bne 1b\n"
				: "=&r" (result), "=&r" (tmp)
				: "r" (p), "r" (0)
				: "cc");
		#endif
		return result;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		#if ARMv2345
			int tmp;
			/* swp may bypass the cache, so it must be used to unlock as well */
			__asm__ volatile("swp %0,%1,[%2]\n" : "=&r" (tmp) : "r" (1), "r" (p));
		#else
			*p = 1;
		#endif
	}
/*---------------------------------------------------------------------------*/
#elif __ICCARM__
	/* IAR Systems Embedded Workbench
	 * ARM IAR C/C++: Development Guide: Compiling and linking
	 * Last reviewed: Oct 2009 with version 5.4
	 */
	#include <intrinsics.h>
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	/* If pre-v7 thumb mode */
	#if __CORE__ <= 6 && __CPU_MODE__ == 1
		#pragma type_attribute=__arm __interwork
	#endif
	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		#if __CORE__ >= 6 && __CPU_MODE__ == 1
			// Work around problem with simulator not recognizing strex and ldrex instructions
			result = *p;
			*p = 0;
		#elif __CORE__ >= 6
			#if __CORE__ == 6
				__MCR(15, 0, 0, 7, 10, 5);
			#else
				__DMB();
			#endif
			do {
				result = (int)__LDREX((unsigned long*)p);
			} while (__STREX(0, (unsigned long*)p) != 0);
		#else
			result = (int)__SWP(0, (unsigned long*)p);
		#endif
		return result;
	}

	#if __CORE__ <= 6 && __CPU_MODE__ == 1
		#pragma type_attribute=__arm __interwork
	#endif
	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		#if __CORE__ >= 6
			*p = 1;
		#else
			/* swp may bypass the cache, so it must be used to unlock as well */
			__SWP(1, (unsigned long*)p);
		#endif
	}
/*---------------------------------------------------------------------------*/
#elif __ICCAVR__
	/* IAR Systems Embedded Workbench for Atmel AVR
	 * IAR C/C++ Compiler Reference Guide for Atmel Corporation AVR Microcontroller Family
	 * Last reviewed: Jul 2012 with version 6.12
	 */
	#include <intrinsics.h>
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		unsigned char save = __save_interrupt();
		__disable_interrupt();
		result = *p;
		*p = !cov_atomic_initializer;
		__restore_interrupt(save);
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __ICCM32C__
	/* IAR Systems Embedded Workbench for M32C
	// M32C IAR C/C++ Compiler Reference Guide for Renesas M32C and M16C/8x Series of CPU Cores 2004
	// Renesas M32C/80 Series Software Manual Rev 1.00 2006.03
	*/

	#include <intrinsics.h>
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		const unsigned char save = __read_ipl();
		__write_ipl(7);
		result = *p;
		*p = !cov_atomic_initializer;
		switch (save) {
		case 0: __write_ipl(0); break;
		case 1: __write_ipl(1); break;
		case 2: __write_ipl(2); break;
		case 3: __write_ipl(3); break;
		case 4: __write_ipl(4); break;
		case 5: __write_ipl(5); break;
		case 6: __write_ipl(6); break;
		default: __write_ipl(7); break;
		}
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __ICCSH__
	/* IAR Systems Embedded Workbench for SuperH
	 * IAR C/C++ Development Guide Compiling and Linking for the Renesas SH Microcomputer Family
	 * Last reviewed: Mar 2010 with version 6.0
	 */
	#include <intrinsics.h>

	typedef char cov_atomic_t;
	#define cov_atomic_initializer 0

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		return _builtin_tas(p);
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __mips && __GNUC__
	/* MIPS/MIPS64 */
	#if __mips < 2
		#error Use instruction set -march=mips2 or higher
	#endif
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		int tmp;
		__asm__ volatile (
			"	.set noreorder\n"
			"	sync\n"
			"1:	ll %0,%3\n"
			"	li %2,0\n"
			"	sc %2,%1\n"
			"	beqz %2,1b\n"
			"	nop\n"          /* delay slot */
			"	.set reorder\n"
			: "=&r" (result), "=m" (*p), "=&r" (tmp)
			: "m" (*p)
			: "cc");
		return result;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
		__asm__ volatile ("" ::: "memory");
	}
/*---------------------------------------------------------------------------*/
#elif __mips && __sgi /* IRIX compiler */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	/* Remainder of implementation in atomic-mips-irix.s */
	int cov_atomic_tryLock(cov_atomic_t* p);

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
/*---------------------------------------------------------------------------*/
#elif __DCC__ && __386
	/* Wind River/Diab pentium
	 * Last reviewed: Oct 2009 with version 5.7.0
	 */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	__asm volatile int cov_atomic_tryLock(cov_atomic_t* p)
	{
% mem p
! "ax", "cx"
		subl %eax,%eax
		movl p,%ecx
		xchgl (%ecx),%eax
}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
/*---------------------------------------------------------------------------*/
#elif __DCC__ && __arm
	/* Wind River/Diab pentium
	 * Last reviewed: Oct 2009 with version 5.7.0
	 */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	#if !defined(Libcov_armv)
		#error Add compiler option -DLibcov_armv=<n>, where <n> is the architecture version (for example 7 with ARMv7)
	#endif
	#if __THUMB__ && !__THUMB2__
		#error Thumb not supported, compile this file in ARM mode
	#endif

	__asm volatile int cov_atomic_tryLock(cov_atomic_t* p)
	{
% reg p; lab L
! "r0", "r1", "r2"
		mov r1,#0
		#if Libcov_armv < 6 && !__THUMB2__
			swp r0,r1,[p]
		#else
			/* Wind River assembler incorrectly does not recognize dmb with -tARMV7 */
			#if __THUMB2__ /* || Libcov_armv >= 7 */
				dmb
			#else
				mcr p15,0,r1,c7,c10,5
			#endif
			L:
				ldrex r0,[p]
				strex r2,r1,[p]
				cmp r2,#0
				bne L
		#endif

}

	__asm volatile void cov_atomic_unlock(cov_atomic_t* p)
	{
% reg p
! "r0", "r1"
		mov r1,#1
		#if Libcov_armv < 6 && !__THUMB2__
			/* swp may bypass the cache, so it must be used to unlock as well */
			swp r0,r1,[p]
		#else
			str r1,[p,#0]
		#endif
}
/*---------------------------------------------------------------------------*/
#elif __DCC__ && (__coldfire || __m68k)
	/* Wind River/Diab ColdFire and m68k
	 * ColdFire Family Programmer's Reference Manual Rev. 3
	 * Last reviewed: Oct 2009 with version 5.7.0
	 */
	typedef volatile char cov_atomic_t;
	#define cov_atomic_initializer 0

	/* If you see the error below, add -DLibcov_isa_a to your compile command
	 *   "error: not part of selected instruction set"
	 * The tas.b instruction became available in ISA B.
	 */
	#if Libcov_isa_a
		int cov_atomic_tryLock(cov_atomic_t* p)
		{
			const int result = !*p;
			*p = 1;
			return result;
		}
	#else
		__asm volatile int cov_atomic_tryLock(cov_atomic_t* p)
		{
% reg p
! "d0"
			clr.l d0
			tas.b (p)
			seq.b d0
% mem p
! "a0", "d0"
			clr.l d0
			move.l p,a0
			tas.b (a0)
			seq.b d0
}
	#endif

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __DCC__ && __m32r
	/* Wind River/Diab M32R
	 * M32R family Software Manual
	 * http://documentation.renesas.com/eng/products/mpumcu/e32rsm.pdf
	 * Based on Linux 2.6.19 system.h functions __xchg, local_irq_restore, local_irq_save
	 * See also http://tool-support.renesas.com/eng/toolnews/n990501/tn2.htm
	 *   Problem in Using System Call "twai_flg"
	 * Last reviewed: Oct 2009 with version 5.7.0
	 */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	__asm volatile int cov_atomic_tryLock(cov_atomic_t* p)
	{
% reg p
! "r0","r1","r2"
		mvfc r1,psw        /* Save processor status word, r1 <- psw */
		and3 r2,r1,#0xffbf /* Clear interrupt enable bit, r2 <- r1 & 0xffbf */
		mvtc r2,psw        /* Store PSW, r2 -> psw */
		ld24 r2,#0         /* Clear r2 */
		lock r0,@p         /* Load *p for return value, r0 <- *p */
		unlock r2,@p       /* Store zero to *p, r2 -> *p */
		mvtc r1,psw        /* Restore PSW, r1 -> psw */
}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __DCC__ && __mips
	/* Wind River/Diab MIPS
	 * See atomic-mips-irix.s
	 * Last reviewed: Oct 2009 with version 5.7.0
	 */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	__asm volatile int cov_atomic_tryLock(cov_atomic_t* p)
	{
% reg p; lab L1
! "$2", "$3"
		sync
		L1:
		ll $2,(p)
		li $3,0
		sc $3,(p)
		beqz $3,L1
		nop
}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
/*---------------------------------------------------------------------------*/
#elif __DCC__ && __ppc
	/* Wind River/Diab PowerPC
	 * References:
	 *   Power Instruction Set Architecture Version 2.06
	 *     4.4.3 Memory Barrier Instructions
	 *     Appendix B. Programming Examples for Sharing Storage
	 * Last reviewed: Oct 2009 with version 5.7.0
	 * The branch prediction hint "-", as in bne-, is not accepted for some CPU targets
	 */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	__asm volatile int cov_atomic_tryLock(cov_atomic_t* p)
	{
% reg p; lab L1
#if __VERSION_NUMBER__ >= 5000
! "r4", "r5"
#endif
		li 4,0
		#if __PPC_VLE__
			mbar 0
		#else
			sync
		#endif
		L1:
		lwarx 5,0,p
		stwcx. 4,0,p
		bne L1
		mr 3,5
}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
/*---------------------------------------------------------------------------*/
#elif __DCC__ && __sh
	/* Wind River/Diab SuperH
	 * Last reviewed: Oct 2009 with version 5.7.0
	 */
	typedef volatile char cov_atomic_t;
	#define cov_atomic_initializer 0

	__asm volatile int cov_atomic_tryLock(cov_atomic_t* p)
	{
% reg p
! "r0"
		tas.b @p
		movt r0
}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __vxworks
	#include <vxLib.h>

	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 0

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		return vxTas((void*)p);
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 0;
	}
/*---------------------------------------------------------------------------*/
#elif __RENESAS__ && defined(M16C)
	/* Renesas C/C++ Compiler Package for M16C Series and R8C Family V.6.00
	*/
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		_asm("	push.w a0");
		_asm("	mov.w r1,a0");
		_asm("	xchg.w r0,[a0]");
		_asm("	pop.w a0");
	}

	_inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
		_asm("");
	}
/*---------------------------------------------------------------------------*/
#elif defined(__RENESAS__) && __RX
	/* References:
	//   RX Family Users Manual: Software RENESAS 32-Bit MCU
	//   RX Family C/C++ Compiler, Assembler, Optimizing Linkage Editor Compiler Package V.1.01 Users Manual
	*/
	#include <machine.h>
	typedef signed long cov_atomic_t;
	#define cov_atomic_initializer 1

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		signed long tmp = 0;
		xchg(&tmp, p);
		return tmp;
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __M32R__
	/* C/C++ Compiler Package for M32R Family V.5.00, Rev.1.00 Jul 15, 2005
	// M32R family Software Manual, Revised publication, 1998.07
	*/
	typedef int cov_atomic_t;
	#define cov_atomic_initializer 1

	#pragma keyword asm on
	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		asm(
			/* Save PSW */
			" mvfc r1,psw\n"
			/* Clear IE bit of PSW */
			" and3 r0,r1,#0xffbf\n"
			" mvtc r0,psw\n"
			/* Exchange */
			" ldi r2,#0\n"
			" lock r0,@r4\n"
			" unlock r2,@r4\n"
			/* Restore PSW */
			" mvtc r1,psw\n"
			/* Return R0 */
			" jmp r14\n"
			);
		return 0;
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif defined(__RENESAS__) && defined(_SH)
	/* References:
	//   SuperH RISC engine C/C++ Compiler, Assembler, Optimizing Linkage Editor Compiler Package V.9.01 User's Manual
	//   SH-1/SH-2/SH-DSP Software Manual, Rev. 5.00
	*/
	#include <umachine.h>
	typedef volatile char cov_atomic_t;
	#define cov_atomic_initializer 0

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		/* This expands to:
		//   TAS.B @R4
		//   MOVT R0
		*/
		return tas(p);
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 0;
	}
/*---------------------------------------------------------------------------*/
#elif __CTC__ || __CPTC__
	/* TASKING TriCore 3.5 */
	typedef unsigned cov_atomic_t;
	#define cov_atomic_initializer 1

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		return __swap(p, 0);
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif _ARC && _ARCVER >= 0x31
	/* ARC MetaWare, 700 processor series */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	_Asm int cov_atomic_tryLock2(cov_atomic_t* p)
	{
%reg p
		mov %r10,p
		mov %r0,0
		ex %r0,[%r10]
%error
	}

	static _Inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		return cov_atomic_tryLock2(p);
	}

	static _Inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif _ARC && _ARCVER >= 5
	/* ARC MetaWare A4, A5, ARC 600 */
	/* ex instruction is only available in ARC 700 series */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static _Inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		_disable();
		const int result = *p;
		*p = 0;
		_enable();
		return result;
	}

	static _Inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __ARC700__ && __GNUC__
	/* GCC for ARC http://sourceforge.net/projects/gcc-arc/ */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result = 0;
		__asm__ volatile("ex %0,[%1]" : "=r" (result) : "r" (p), "0" (result) : "memory");
		return result;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __TCS_VERSION__
	/* NXP TriMedia Compilation System */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		#pragma TCS_atomic
		const int result = *p;
		*p = 0;
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __TI_COMPILER_VERSION__ && _TMS320C5XX
	/* Texas Instruments cl500
	//   TMS320C54x Optimizing C/C++ Compiler Users Guide
	//   TMS320C54x DSP Reference Set Volume 2: Mnemonic Instruction Set
	*/
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		/* Disable interrupts */
		asm("	ldm st1,b");
		asm("	ssbx intm");
		/* Swap */
		result = *p;
		*p = 0;
		/* Enable interrupts */
		asm("	stlm b,st1");
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __TI_COMPILER_VERSION__ && (__TMS320C55X__ || _TMS320C6X)
	/* Texas Instruments cl6x
	//   TMS320C6000 Optimizing Compiler v 7.2 User's Guide
	// Texas Instruments cl55
	//   ccs/ccsv5/tools/compiler/c5500/include/c55x.h
	*/
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		const unsigned csr = _disable_interrupts();
		const int result = *p;
		*p = 0;
		_restore_interrupts(csr);
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif (__TI_COMPILER_VERSION__ && __MSP430__) || __ICC430__
	/* Texas Instruments cl430
	//   MSP430 Optimizing C/C++ Compiler v 3.1
	//   CC430 Family User's Guide
	//
	// IAR Systems Embedded Workbench
	//   IAR C/C++ Compiler Reference Guide for Texas Instruments MSP430 Microcontroller Family
	//   Last reviewed: Jan 2012 with version 5.40.2.40380
	*/
	#if __ICC430__
		#include <intrinsics.h>
		#define _disable_interrupts __disable_interrupt
		#define _get_interrupt_state __get_interrupt_state
		#define _set_interrupt_state __set_interrupt_state
	#endif
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		const unsigned sr = _get_interrupt_state();
		_disable_interrupts();
		result = *p;
		*p = 0;
		_set_interrupt_state(sr);
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __TI_COMPILER_VERSION__ && __TMS470__
	/* Texas Instruments ARM
	// ARM Optimizing C/C++ Compiler v4.9 User's Guide August 2011
	*/
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	/* The compiler does not provide access to local variables.
	 * Since this function returns a value, the entire function is in an asm statement.
	 * Unfortunately, this makes it not inline */
	int cov_atomic_tryLock(cov_atomic_t* p);
	#if __TI_TMS470_V4__ || __TI_TMS470_V5__
		#if __16bis__
			#error Thumb not supported, compile this file in ARM mode
		#endif
		asm(
			"_cov_atomic_tryLock:\n"
			"cov_atomic_tryLock:\n"
			"$cov_atomic_tryLock:\n"
			" mov a2,#0\n"      /* a2 <- 0 */
			" mov v9,a1\n"      /* v9 <- p */
			" swp a1,a2,[v9]\n" /* a1 <- @v9 */
								/* a2 -> @v9 */
			" bx lr\n"          /* return a1 */
			);
	#else
		asm(
			"_cov_atomic_tryLock:\n"
			"cov_atomic_tryLock:\n"
			" mov r1,#0\n"
			#if __TI_TMS470_V6__
				" mcr p15,0,r1,c7,c10,5\n"
			#else
				" dmb\n"
			#endif
			"L1:\n"
			" ldrex r2,[r0]\n"
			" strex r3,r1,[r0]\n"
			" cmp r3,#0\n"
			" bne L1\n"
			" mov r0,r2\n"
			" bx lr\n"
			);
	#endif

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __sh__ && __GNUC__
	/* SH-1/SH-2/SH-DSP Software Manual, Renesas 32-Bit RISC Microcomputer SuperH RISC engine Family */
	/* http://documentation.renesas.com/eng/products/mpumcu/rej09b0171_superh.pdf */
	typedef volatile char cov_atomic_t;
	#define cov_atomic_initializer 0

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		__asm__ volatile (
			"tas.b @%1\n\t"
			"movt %0"
			: "=r" (result) : "r" (p) : "cc", "memory");
		return result;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
		__asm__ volatile ("" ::: "memory");
	}
/*---------------------------------------------------------------------------*/
#elif __ghs__ && __ARM__
	/* Green Hills Software ARM
	// MULTI: Building Applications for Embedded ARM December 6, 2011
	*/
	#if !__THUMB2_AWARE
		#error This implementation does not support older than ARMv7
	#endif

	#define __ARM_DSP 1 /* Work around apparent bug in arm_ghs.h */
	#include <arm_ghs.h>

	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		asm(" dmb");
		do {
			result = __LDREX((int*)p);
		} while (__STREX(!cov_atomic_initializer, (int*)p) != 0);
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __ghs__ && __ppc
	/* Green Hills Software PowerPC */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	__asm int cov_atomic_tryLock(cov_atomic_t* p)
	{
%reg p lab L1
		li r4,0
		sync
		L1:
		lwarx r5,0,p
		stwcx. r4,0,p
		bne- L1
		mr r3,r5
%error
}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
#elif __ghs__ && __V800__
	/* Green Hills Software NEC V850
	 * V850 FAMILY 32-bit Single-Chip Microcontroller Architecture Users Manual 7th edition
	 * MULTI: Building Applications for Embedded V800, December 28, 2011
	 */
	#include <v800_ghs.h>
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer { 1 }

	static __inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		const unsigned psw = __GETSR();
		const int result = *p;
		*p = 0;
		__SETSR(psw);
		return result;
	}

	static __inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
#elif __ADSPBLACKFIN__
	/* Analog Devices Visual DSP++ */
	#include <ccblkfn.h>
	#define cov_atomic_t testset_t
	#define cov_atomic_initializer 0
	#define cov_atomic_tryLock adi_try_lock
	#define cov_atomic_unlock adi_release_lock
#elif __ADSP21000__
	/* Analog Devices SHARC
	// VisualDSP++ 5.0 C/C++ Compiler Manual for SHARC Processors Revision 1.5, January 2011
	*/
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer { 1 }

	#include <sysreg.h>
	#include <platform_include.h>

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		/* Disable interrupts */
		const int enabled = sysreg_bit_tst(sysreg_MODE1, IRPTEN);
		sysreg_bit_clr(sysreg_MODE1, IRPTEN);
		result = *p;
		*p = 0;
		/* Restore interrupts */
		if (enabled) {
			sysreg_bit_set(sysreg_MODE1, IRPTEN);
		}
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
#elif __v850__
	/* NEC v850 Compiler-Assembler 3.10 */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer { 1 }

	/* Perform the tryLock operation with an intermediate function because local variables cannot be named from within an __asm declaration.
	 * Using this function, we know to store the result at the address in the second argument.
	 */
	static void cov_atomic_tryLock_asm(cov_atomic_t* p, int* result)
	{
		__asm(".option nooptimize");
		__asm("	stsr 5,r10");		/* Save the PSW in r10. We only care about the Interrupt Disable bit */
		__asm("	di");				/* Disable interrupts */
		__asm("	ld.w [r6], r11");	/* *p -> r11 */
		__asm("	st.w r11, [r7]");	/* *result <- r11 */
		__asm("	st.w r0, [r6]");	/* 0 -> *p */
		__asm("	ldsr r10,5");		/* Restore the Interrupt Disable bit */
		__asm(".option optimize");
	}

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		cov_atomic_tryLock_asm(p, &result);
		return result;
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
#elif _ENTERPRISE_C_
	/* Freescale StarCore
	 * C Compiler User Guide (version 22.11)
	 * SC140 DSP Core Reference Manual Revision 4.1, September 2005
	 * MSC8144E Reference Manual Rev 3, July 2009
	 */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer { 0 }
	asm static int cov_atomic_tryLock(cov_atomic_t* p)
	{
	asm_header
	.arg
		_p in $r0;
	return in $d0;
	asm_body
		#if _SC3000_
			syncio
		#else
			; Work around "A.2: AGU register contents are not available for an additional cycle"
			nop
		#endif
		bmtset #1,(r0)
		move.l sr,d0
		and #2,d0,d0
		not d0,d0
	asm_end
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 0;
	}
#elif __XTENSA__
	/* Tensilica Xtensa
	 * Implementing a Memory-Based Mutex and Barrier Synchronization Library 2007
	 * Xtensa Instruction Set Architecture (ISA) Reference Manual 2007
	 * Memory barrier instruction MEMW automatically generated for volatile objects
	 * Verified with xt-xcc 9.0.1
	 */
	#include "xtensa/config/core-isa.h"
	#if XCHAL_HAVE_S32C1I
		#include "xtensa/tie/xt_sync.h"
	#else
		#include "xtensa/tie/xt_core.h"
	#endif
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result = 0;
		#if XCHAL_HAVE_S32C1I
			XT_WSR_SCOMPARE1(1);
			XT_S32C1I(result, p, 0);
		/* If just one privilege level, then RSIL is available */
		#elif XCHAL_MMU_RINGS == 1
			/* Disable interrupts */
			const int interruptLevel = XT_RSIL(15);
			/* Try the lock */
			result = *p;
			*p = 0;
			/* Enable interrupts */
			XT_WSR_PS(interruptLevel);
		#else
			#error No implementation for this processor configuration
		#endif
		return result;
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
#elif __CWCC__ && __COLDFIRE__
	/* Freescale ColdFire
	 * CodeWarrior Development Studio ColdFire Architectures Edition Build Tools Reference v7.x
	 * ColdFire Family Programmer's Reference Manual
	 */
	typedef volatile char cov_atomic_t;
	#define cov_atomic_initializer 0

	int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		/* If the TAS.B instruction is available (ISA_B or later) */
		/*   We determined this conditional test empirically.  We invoked the
		 *   compiler on the inline assembly below with each possible processor code
		 *   (-proc option) and noted either success or failure ("illegal instruction
		 *   for this processor").  We invoked the compiler with each possible
		 *   processor code to expand the __COLDFIRE__.  The successful outcome
		 *   correlates to the condition below.  The list of processor codes was
		 *   determined by searching for MCF[0-9] in the compiler executable.
		 *   Successful:
		 *     MCF5407	0x4070
		 *     MCF5470	0x4070
		 *     MCF5471	0x4071
		 *     MCF5472	0x4072
		 *     MCF5473	0x4073
		 *     MCF5474	0x4074
		 *     MCF5475	0x4075
		 *     MCF547X	0x4075
		 *     MCF5480	0x4080
		 *     MCF5481	0x4081
		 *     MCF5482	0x4082
		 *     MCF5483	0x4083
		 *     MCF5484	0x4084
		 *     MCF5485	0x4085
		 *   Failure:
		 *     MCF5100	0x0001
		 *     MCF51XX	0x0001
		 *     MCF5206	0x0001
		 *     MCF5206E	0x0001
		 *     MCF5206e	0x206e
		 *     MCF5208	0x2008
		 *     MCF5211	0x2011
		 *     MCF5212	0x2012
		 *     MCF5213	0x2013
		 *     MCF521X	0x2013
		 *     MCF5221X	0x0001
		 *     MCF52221	0x2221
		 *     MCF5222X	0x2223
		 *     MCF5223X	0x2235
		 *     MCF5249	0x2049
		 *     MCF5253	0x2053
		 *     MCF5270	0x2070
		 *     MCF5271	0x2071
		 *     MCF5272	0x2072
		 *     MCF5274	0x2074
		 *     MCF5275	0x2075
		 *     MCF5280	0x2080
		 *     MCF5281	0x2081
		 *     MCF5282	0x2082
		 *     MCF52XX	0x0001
		 *     MCF5307	0x3070
		 *     MCF5327	0x3027
		 *     MCF5328	0x3028
		 *     MCF5329	0x3029
		 *     MCF532X	0x3029
		 *     MCF53XX	0x0001
		 *     MCF5445X	0x0001
		 *     MCF548X	0x0001
		 *     MCF54XX	0x0001
		 */
		#if __COLDFIRE__ >= 0x4000
			asm {
				clr.l result
				tas.b p
				seq result
			}
		#else
			result = !*p;
			*p = 1;
		#endif
		return result;
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 0;
	}
/*---------------------------------------------------------------------------*/
#elif __TASKING__ && (__ARM__ || __CPARM__)
	/* TASKING VX-toolset for ARM 3.0 */
	/* TASKING VX-toolset for ARM v4.1r1 */
	/* ARMv7-M Architecture Reference Manual */
	/* ARM1136JF-S and ARM1136J-S Technical Reference Manual: Example of LDREX and STREX usage */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	#define ARMv456 __CPU_ARMV4T__ || __CPU_ARMV4__ || __CPU_ARMV5TE__ || __CPU_ARMV5T__ || __CPU_XS__

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		#if __CPU_ARMV6M__
			unsigned save = __get_PRIMASK();
			__disable_irq();
			result = *p;
			*p = 0;
			__set_PRIMASK(save);
		#elif ARMv456
			__asm(
				"	.arm\n"
				"	swp %0,%1,[%2]\n"
				#if __THUMB__
					"	.thumb\n"
				#endif
				: "=&r" (result) : "r" (0), "r" (p));
		#else
			int tmp;
			__asm(
				"	dmb\n"
				"1:	ldrex %0,[%2]\n"
				/* With v4.1r1 ARM simulator, handling of strex is broken: first two operands are reversed */
				"	strex %1,%3,[%2]\n"
				"	cmp %1,#0\n"
				"	bne 1p\n"
				: "=&r" (result), "=&r" (tmp)
				: "r" (p), "r" (0)
				: "cc");
		#endif
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		#if ARMv456
			int tmp;
			/* swp may bypass the cache, so it must be used to unlock as well */
			__asm(
				"	.arm\n"
				"	swp %0,%1,[%2]\n"
				#if __THUMB__
					"	.thumb\n"
				#endif
				: "=&r" (tmp) : "r" (1), "r" (p));
		#else
			*p = 1;
		#endif
	}
/*---------------------------------------------------------------------------*/
#elif __CWCC__ && __arm
	/* Freescale CodeWarrior for Microcontrollers, Kinetis
	//   CodeWarrior Development Studio for Microcontrollers V10.x Kinetis Build Tools Reference Manual
	*/
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	#if __thumb__
		#error Thumb not supported, compile this file in ARM mode
	#endif
	#if !defined(Libcov_armv)
		#error Add compiler option -DLibcov_armv=<n>, where <n> is the architecture version (for example 7 with ARMv7)
	#endif

	static inline int cov_atomic_tryLock(register cov_atomic_t* p)
	{
		register int result = 0;
		register int zero = 0;
		#if Libcov_armv < 6
			asm {
				swp result,zero,[p]
			}
		#else
			#error Compiler version "5.0 build 24 (build 24)" incorrectly translates this code.
			// Specifically, the strex 2nd and 3rd operands are reversed.
			asm {
				//mov r1,#0
				#if Libcov_armv == 6
					mcr p15,0,zero,c7,c10,5
				#else
					dmb
				#endif
			L1:
				ldrex result,[p]
				strex r3,zero,[p]
				cmp r3,#0
				bne L1
			}
		#endif
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		#if Libcov_armv < 6
			/* swp may bypass the cache, so it must be used to unlock as well */
			int tmp;
			int one = 1;
			asm {
				swp tmp,one,[p]
			}
		#else
			*p = cov_atomic_initializer;
		#endif
	}
/*---------------------------------------------------------------------------*/
#elif __m68k__ && __GNUC__
	typedef volatile char cov_atomic_t;
	#define cov_atomic_initializer 0

	/* If you see the error below, add -DLibcov_isa_a to your compile command
	 *   "Error: invalid instruction for this architecture"
	 * The tas.b instruction became available in ISA B.
	 */
	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		#if Libcov_isa_a
			result = !*p;
			*p = 1;
		#else
			__asm__ volatile("\n"
				"	clr.l %0\n"
				"	tas.b (%1)\n"
				"	seq.b %0\n"
				: "=&d" (result)
				: "a" (p)
				: "cc");
		#endif
		return result;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 0;
	}
/*---------------------------------------------------------------------------*/
#elif (__HIGHC__ || __HIGHC_ANSI__) && _PPC
	#include <ppc/asm.h>
	/* MetaWare High C/C++ 4.3
	 * High C/C++ Programmer's Guide for PowerPC
	 * See atomic-ppc-aix.s for additional references
	 */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	/* The compiler does not provide a way to reserve registers, so we use parameters for scratch registers */
	_Asm int cov_atomic_tryLock_asm(cov_atomic_t* p, int result, int zero)
	{
		% reg p, result, zero; lab L
		sync
		L:
		lwarx result,0,p
		stwcx. zero,0,p
		bne- L
		mr %r3,result
		% error
	}

	static _Inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result = 0;
		int zero = 0;
		return cov_atomic_tryLock_asm(p, result, zero);
	}

	static _Inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
	}
/*---------------------------------------------------------------------------*/
#elif  __TI_COMPILER_VERSION__ && (_TMS320C2000 || _TMS320C3x || _TMS320C4x)
	/* Texas Instruments cl2000
	 *   TMS320C28x CPU and Instruction Set Reference Guide
	 *   TMS320C28x Optimizing C/C++ Compiler v6.0
	 * Texas Instruments cl30
	 *   TMS320C3x/C4x Optimizing C Compiler User's Guide SPRU034H
	 *   TMS320C3x User's Guide SPRU031F
	 */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	#if !_TMS320C2000
		#define IER IE
	#endif
	extern cregister volatile unsigned int IER;

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		/* Disable interrupts */
		const unsigned ier = IER;
		IER = 0;
		/* Swap */
		result = *p;
		*p = 0;
		/* Restore interrupts */
		IER = ier;
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __AVR__
	/* Atmel AVR 8-bit */
	#include <util/atomic.h>

	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			result = *p;
			*p = 0;
		}
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __WATCOMC__ && _M_I86
	/* Watcom x86 16-bit */
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static inline int __watcall cov_atomic_tryLock(cov_atomic_t* p)
	{
		int newValue;
		#if __SMALL__ || __MEDIUM__
			__asm {
				mov bx,ax
				sub ax,ax
				xchg ax,[bx]
				mov newValue,ax
			}
		#else
			__asm {
				mov bx,ax
				sub ax,ax
				mov ds,dx
				xchg ax,[bx]
				mov newValue,ax
			}
		#endif
		return newValue;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __ICC8051__ || __ICCRL78__
	/* IAR Embedded Workbench for 8051
	// IAR C/C++ Compiler Reference Guide for the 8051 Microcontroller Architecture, v8.10
	//
	// IAR Embedded Workbench for RL78
	// IAR C/C++ Compiler Reference Guide for the Renesas RL78 Microcontroller Family, v1.20.4
	*/
	#include "intrinsics.h"
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		const __istate_t istate = __get_interrupt_state();
		__disable_interrupt();
		result = *p;
		*p = !cov_atomic_initializer;
		__set_interrupt_state(istate);
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __TASKING__ && __C166__
	/* TASKING VX-toolset for C166 User Guide MA119-800 (v3.0) February 25, 2011
	// Instruction Set Manual for the C166 Family of Infineon 16-Bit Single-Chip Microcontroller,
	//   Users Manual V2.0 Mar 2001
	*/
	typedef volatile __near int cov_atomic_t;
	#define cov_atomic_initializer 1

	static inline int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		__asm(
			"	atomic #2\n"
			"	movw %0,[%1]\n"
			"	movw [%1],%2\n"
			: "=&w" (result)
			: "i" (p), "w" (!cov_atomic_initializer));
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif _C166 && _CPUTYPE
	/* TASKING C166/ST10 Classic
	// C166/ST10 v8.9 C Cross-Compiler User's Manual v5.20
	*/
	typedef volatile _near int cov_atomic_t;
	#define cov_atomic_initializer 1

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		p = p;
		#pragma asm
		mov r13,#0
		atomic #2
		mov r4,[r12]
		mov [r12],r13
		#pragma endasm
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif microblaze
	/* Xilinx MicroBlaze Processor Reference Guide, Embedded Development Kit 13.3
	*/
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		int tmp;
		__asm__ volatile(
			"1:	lwx %0,%3,r0\n"
			"	swx r0,%3,r0\n"
			"	addic %1,r0,0\n"
			"	bnei %1,1b\n"
			: "=&r" (result), "=r" (tmp), "=m" (*p)
			: "r" (p)
			: "cc", "memory");
		return result;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = 1;
		__asm__ volatile ("" ::: "memory");
	}
/*---------------------------------------------------------------------------*/
#elif __18CXX
	/* Microchip MPLAB C18 */
	#include <p18cxxx.h>

	typedef volatile char cov_atomic_t;
	#define cov_atomic_initializer 1

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		unsigned char save = INTCON;
		INTCONbits.GIE = 0;
		result = *p;
		*p = 0;
		INTCON = save;
		return result;
	}

	static void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __C30
	/* Microchip MPLAB C Compiler for PIC24 MCUs and dsPIC DSCs v3.31 */

	#if __dsPIC30F__
		#include <p30Fxxxx.h>
	#elif __dsPIC33E__
		#include <p33Exxxx.h>
	#elif __dsPIC33F__
		#include <p33Fxxxx.h>
	#elif __PIC24E__
		#include <p24Exxxx.h>
	#elif __PIC24F__ || __PIC24FK__
		#include <p24Fxxxx.h>
	#elif __PIC24H__
		#include <p24Hxxxx.h>
	#else
		#error "unknown Microchip PIC24 device family"
	#endif
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 1

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		const unsigned DISICNT_save = DISICNT;
		__asm__ volatile ("disi #0x3FFF");
		result = *p;
		*p = 0;
		DISICNT = DISICNT_save;
		return result;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
		__asm__ volatile ("" ::: "memory");
	}
/*---------------------------------------------------------------------------*/
#elif __XC8
	/* Microchip MPLAB XC8 C v1.11 */
	/* MPLAB XC8 C Compiler Users Guide: 5.9.4 Enabling Interrupts */
	#include <xc.h>

	typedef volatile char cov_atomic_t;
	#define cov_atomic_initializer 1

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		unsigned char save = INTCON;
		di();
		result = *p;
		*p = 0;
		INTCON = save;
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __XC16
	/* Microchip MPLAB XC16 C */
	/* MPLAB XC16 C Compiler Users Guide: 11.7 Enabling/Disabling Interrupts */
	#include <xc.h>

	typedef volatile char cov_atomic_t;
	#define cov_atomic_initializer 1

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		int result;
		ET_AND_SAVE_CPU_IPL(current_cpu_ipl, 7);
		result = *p;
		*p = 0;
		RESTORE_CPU_IPL(current_cpu_ipl);
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __XC32
	/* Microchip MPLAB XC32 C */
	/* PIC32 Peripheral Libraries for MPLAB C32 Compiler: 7.1 System Functions */
	#include <xc.h>
	#include <plib.h>

	typedef volatile char cov_atomic_t;
	#define cov_atomic_initializer 1

	static int cov_atomic_tryLock(cov_atomic_t* p)
	{
		const unsigned status = INTDisableInterrupts();
		const int result = *p;
		*p = 0;
		INTRestoreInterrupts(status);
		return result;
	}

	static inline void cov_atomic_unlock(cov_atomic_t* p)
	{
		*p = cov_atomic_initializer;
	}
/*---------------------------------------------------------------------------*/
#elif __GNUC__ * 10 + __GNUC_MINOR__ >= 41
	/* GCC 4.1 and later "Built-in functions for atomic memory access"
	// Architectures known supported:
	//   Atmel AVR32
	*/
	typedef volatile int cov_atomic_t;
	#define cov_atomic_initializer 0

	static __inline__ int cov_atomic_tryLock(cov_atomic_t* p)
	{
		return __sync_lock_test_and_set(p, 1) == cov_atomic_initializer;
	}

	static __inline__ void cov_atomic_unlock(cov_atomic_t* p)
	{
		__sync_lock_release(p);
	}
#else
	#include "../mlx_bullseye/atomic-user.h"
#endif
