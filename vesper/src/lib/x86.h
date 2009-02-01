//
// Copyright 2007 - 2009, Stanislav Karchebnyy <berkus+metta@madfire.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
/*********************************************************************
 *
 * Copyright (C) 2003-2004, 2006-2007,  Karlsruhe University
 *
 * File path:     arch/x86/x86.h
 * Description:   X86-64 CPU Specific constants
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 *
 ********************************************************************/
#pragma once

/**********************************************************************
 *    MMU
 **********************************************************************/
#define X86_4KPAGE_BITS	        12
#define X86_4KPAGE_SIZE		(__UL(1) << X86_4KPAGE_BITS)
#define X86_4KPAGE_MASK		(~(X86_4KPAGE_SIZE - 1))



/**********************************************************************
 *    FLAGS register
 **********************************************************************/
#define X86_FLAGS_CF    (__UL(1) <<  0)       /* carry flag                   */
#define X86_FLAGS_PF    (__UL(1) <<  2)       /* parity flag                  */
#define X86_FLAGS_AF    (__UL(1) <<  4)       /* auxiliary carry flag         */
#define X86_FLAGS_ZF    (__UL(1) <<  6)       /* zero flag                    */
#define X86_FLAGS_SF    (__UL(1) <<  7)       /* sign flag                    */
#define X86_FLAGS_TF    (__UL(1) <<  8)       /* trap flag                    */
#define X86_FLAGS_IF    (__UL(1) <<  9)       /* interrupt enable flag        */
#define X86_FLAGS_DF    (__UL(1) << 10)       /* direction flag               */
#define X86_FLAGS_OF    (__UL(1) << 11)       /* overflow flag                */
#define X86_FLAGS_NT    (__UL(1) << 14)       /* nested task flag             */
#define X86_FLAGS_RF    (__UL(1) << 16)       /* resume flag                  */
#define X86_FLAGS_VM    (__UL(1) << 17)       /* virtual 8086 mode            */
#define X86_FLAGS_AC    (__UL(1) << 18)       /* alignement check             */
#define X86_FLAGS_VIF   (__UL(1) << 19)       /* virtual interrupt flag       */
#define X86_FLAGS_VIP   (__UL(1) << 20)       /* virtual interrupt pending    */
#define X86_FLAGS_ID    (__UL(1) << 21)       /* CPUID flag                   */
#define X86_FLAGS_IOPL(x)       ((x & 3) << 12) /* the IO privilege level field */



/**********************************************************************
 *    control register bits (CR0, CR3, CR4)
 **********************************************************************/

#define X86_CR0_PE 	(__UL(1) <<  0)   /* enable protected mode            */
#define X86_CR0_MP 	(__UL(1) <<  1)   /* monitor coprocessor              */
#define X86_CR0_EM 	(__UL(1) <<  2)   /* disable fpu                      */
#define X86_CR0_TS 	(__UL(1) <<  3)   /* task switched                    */
#define X86_CR0_ET 	(__UL(1) <<  4)   /* extension type (always 1)        */
#define X86_CR0_NE 	(__UL(1) <<  5)   /* numeric error reporting mode     */
#define X86_CR0_WP 	(__UL(1) << 16)   /* force write protection on user
                                    read only pages for kernel       */
#define X86_CR0_NW 	(__UL(1) << 29)   /* not write through                */
#define X86_CR0_CD 	(__UL(1) << 30)   /* cache disabled                   */
#define X86_CR0_PG 	(__UL(1) << 31)   /* enable paging                    */

#define X86_CR3_PCD    	(__UL(1) <<  3)   /* page-level cache disable     */
#define X86_CR3_PWT    	(__UL(1) <<  4)   /* page-level writes transparent*/

#define X86_CR4_VME    	(__UL(1) <<  0)   /* virtual 8086 mode extension  */
#define X86_CR4_PVI    	(__UL(1) <<  1)   /* enable protected mode
                                        virtual interrupts           */
#define X86_CR4_TSD    	(__UL(1) <<  2)   /* time stamp disable           */
#define X86_CR4_DE     	(__UL(1) <<  3)   /* debug extensions             */
#define X86_CR4_PSE    	(__UL(1) <<  4)   /* page size extension (4MB)    */
#define X86_CR4_PAE    	(__UL(1) <<  5)   /* physical address extension   */
#define X86_CR4_MCE    	(__UL(1) <<  6)   /* machine check extensions     */
#define X86_CR4_PGE    	(__UL(1) <<  7)   /* enable global pages          */
#define X86_CR4_PCE    	(__UL(1) <<  8)   /* allow user to use rdpmc      */
#define X86_CR4_OSFXSR 	(__UL(1) <<  9)   /* enable fxsave/fxrstor + sse  */
#define X86_CR4_OSXMMEXCPT (__UL(1) << 10)   /* support for unmsk. SIMD exc. */
#define X86_CR4_VMXE   	(__UL(1) << 13)  /* vmx extensions                */


/**********************************************************************
 *    Model Specific Registers (MSRs)
 **********************************************************************/
/* Processor features in the EFER MSR. */
#define X86_EFER_MSR                0xC0000080

#define X86_EFER_SCE  (1 <<  0)       /* system call extensions       */
#define X86_EFER_LME  (1 <<  8)       /* long mode enabled            */
#define X86_EFER_LMA  (1 << 10)       /* long mode active             */
#define X86_EFER_NXE  (1 << 11)       /* nx bit enable                */
#define X86_EFER_SVME (1 << 12)       /* svm extensions               */

#define X86_FEATURE_CONTROL_MSR     0x0000003a
#define X86_FEAT_CTR_LOCK           (1 << 0)

#define X86_SYSENTER_CS_MSR         0x00000174
#define X86_SYSENTER_EIP_MSR        0x00000176
#define X86_SYSENTER_ESP_MSR        0x00000175
#define X86_DEBUGCTL_MSR            0x000001d9

# define X86_LASTBRANCHFROMIP_MSR   0x000001db
# define X86_LASTBRANCHTOIP_MSR     0x000001dc
# define X86_LASTINTFROMIP_MSR      0x000001dd
# define X86_LASTINTTOIP_MSR        0x000001de
# define X86_MTRRBASE_MSR(x)        (0x200 + 2*(x) + 0)
# define X86_MTRRMASK_MSR(x)        (0x200 + 2*(x) + 1)

# define X86_MISC_ENABLE_MSR        0x000001a0
# define X86_COUNTER_BASE_MSR       0x00000300
# define X86_CCCR_BASE_MSR          0x00000360
# define X86_TC_PRECISE_EVENT_MSR   0x000003f0
# define X86_PEBS_ENABLE_MSR        0x000003f1
# define X86_PEBS_MATRIX_VERT_MSR   0x000003f2
# define X86_DS_AREA_MSR            0x00000600
# define X86_LER_FROM_LIP_MSR       0x000001d7
# define X86_LER_TO_LIP_MSR         0x000001d8
# define X86_LASTBRANCH_TOS_MSR     0x000001da
# define X86_LASTBRANCH_0_MSR       0x000001db
# define X86_LASTBRANCH_1_MSR       0x000001dc
# define X86_LASTBRANCH_2_MSR       0x000001dd
# define X86_LASTBRANCH_3_MSR       0x000001de

/* Processor features in the MISC_ENABLE MSR. */
# define X86_ENABLE_FAST_STRINGS            (1 << 0)
# define X86_ENABLE_X87_FPU                 (1 << 2)
# define X86_ENABLE_THERMAL_MONITOR         (1 << 3)
# define X86_ENABLE_SPLIT_LOCK_DISABLE      (1 << 4)
# define X86_ENABLE_PERFMON                 (1 << 7)
# define X86_ENABLE_BRANCH_TRACE            (1 << 11)
# define X86_ENABLE_PEBS                    (1 << 12)

/* Preceise Event-Based Sampling (PEBS) support. */
# define X86_PEBS_REPLAY_TAG_MASK           ((__UL(1) << 12)-1)
# define X86_PEBS_UOP_TAG                   (__UL(1) << 24)
# define X86_PEBS_ENABLE_PEBS               (__UL(1) << 25)

/* Page Attribute Table (PAT) */
# define X86_CR_PAT_MSR             0x00000277
# define X86_PAT_UC         0x00
# define X86_PAT_WC         0x01
# define X86_PAT_WT         0x04
# define X86_PAT_WP         0x05
# define X86_PAT_WB         0x06
# define X86_PAT_UM         0x07

/* Virtual Machine Extensions (VMX) */
#if defined(CONFIG_VMX)
# define X86_FEAT_CTR_ENABLE_VMXON (1 << 2)

# define X86_VMX_BASIC_MSR         0x480
# define X86_VMX_PINBASED_CTLS_MSR 0x481
# define X86_VMX_CPUBASED_CTLS_MSR 0x482
# define X86_VMX_EXIT_CTLS_MSR     0x483
# define X86_VMX_ENTRY_CTLS_MSR    0x484
# define X86_VMX_MISC_MSR          0x485
# define X86_VMX_CR0_FIXED0_MSR    0x486
# define X86_VMX_CR0_FIXED1_MSR    0x487
# define X86_VMX_CR4_FIXED0_MSR    0x488
# define X86_VMX_CR4_FIXED1_MSR    0x489
#endif

/* Secure Virtual Machine Extensions (SVM) */
#if defined(CONFIG_SVM)
# define X86_SVM_VMCR_MSR          0xC0010114
# define X86_SVM_HSAVE_PA_MSR      0xC0010117
#endif

// kate: indent-width 4; replace-tabs on;
// vim: set et sw=4 ts=4 sts=4 cino=(4 :
