// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

/*++



Module Name:

    context.c

Abstract:

    Implementation of GetThreadContext/SetThreadContext/DebugBreak.
    There are a lot of architecture specifics here.



--*/

#include "palinternal.h"
#include "context.h"

#include <sys/ptrace.h> 
#include <errno.h>
#include <unistd.h>


#define CONTEXT_AREA_MASK 0xffff
#ifdef _X86_
#define CONTEXT_ALL_FLOATING (CONTEXT_FLOATING_POINT | CONTEXT_EXTENDED_REGISTERS)
#elif defined(_AMD64_)
#define CONTEXT_ALL_FLOATING CONTEXT_FLOATING_POINT
#elif defined(_ARM_)
#define CONTEXT_ALL_FLOATING CONTEXT_FLOATING_POINT
#elif defined(_ARM64_)
#define CONTEXT_ALL_FLOATING CONTEXT_FLOATING_POINT
#else
#error Unexpected architecture.
#endif

#if !HAVE_MACH_EXCEPTIONS

#ifndef __GLIBC__
typedef int __ptrace_request;
#endif

#if HAVE_MACHINE_REG_H
#include <machine/reg.h>
#endif  // HAVE_MACHINE_REG_H
#if HAVE_MACHINE_NPX_H
#include <machine/npx.h>
#endif  // HAVE_MACHINE_NPX_H

#if HAVE_PT_REGS
#include <asm/ptrace.h>
#endif  // HAVE_PT_REGS

#ifdef _AMD64_
#define ASSIGN_CONTROL_REGS \
        ASSIGN_REG(Rbp)     \
        ASSIGN_REG(Rip)     \
        ASSIGN_REG(SegCs)   \
        ASSIGN_REG(EFlags)  \
        ASSIGN_REG(Rsp)     \

#define ASSIGN_INTEGER_REGS \
        ASSIGN_REG(Rdi)     \
        ASSIGN_REG(Rsi)     \
        ASSIGN_REG(Rbx)     \
        ASSIGN_REG(Rdx)     \
        ASSIGN_REG(Rcx)     \
        ASSIGN_REG(Rax)     \
        ASSIGN_REG(R8)     \
        ASSIGN_REG(R9)     \
        ASSIGN_REG(R10)     \
        ASSIGN_REG(R11)     \
        ASSIGN_REG(R12)     \
        ASSIGN_REG(R13)     \
        ASSIGN_REG(R14)     \
        ASSIGN_REG(R15)     \

#else
#error Dont know how to assign registers on this architecture
#endif

#define ASSIGN_ALL_REGS     \
        ASSIGN_CONTROL_REGS \
        ASSIGN_INTEGER_REGS \


/*++
Function :
    CONTEXTToNativeContext
    
    Converts a CONTEXT record to a native context.

Parameters :
    CONST CONTEXT *lpContext : CONTEXT to convert
    native_context_t *native : native context to fill in

Return value :
    None

--*/
void CONTEXTToNativeContext(CONST CONTEXT *lpContext, native_context_t *native)
{
#define ASSIGN_REG(reg) MCREG_##reg(native->uc_mcontext) = lpContext->reg;
    if ((lpContext->ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL)
    {
        ASSIGN_CONTROL_REGS
    }

    if ((lpContext->ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER)
    {
        ASSIGN_INTEGER_REGS
    }
#undef ASSIGN_REG

#if HAVE_GREGSET_T || HAVE_GREGSET_T
#if HAVE_GREGSET_T
    if (native->uc_mcontext.fpregs == nullptr)
#elif HAVE___GREGSET_T
    if (native->uc_mcontext.__fpregs == nullptr)
#endif
    {
        // If the pointer to the floating point state in the native context
        // is not valid, we can't copy floating point registers regardless of
        // whether CONTEXT_FLOATING_POINT is set in the CONTEXT's flags.
        return;
    }
#endif

    if ((lpContext->ContextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT)
    {
#ifdef _AMD64_
        FPREG_ControlWord(native) = lpContext->FltSave.ControlWord;
        FPREG_StatusWord(native) = lpContext->FltSave.StatusWord;
        FPREG_TagWord(native) = lpContext->FltSave.TagWord;
        FPREG_ErrorOffset(native) = lpContext->FltSave.ErrorOffset;
        FPREG_ErrorSelector(native) = lpContext->FltSave.ErrorSelector;
        FPREG_DataOffset(native) = lpContext->FltSave.DataOffset;
        FPREG_DataSelector(native) = lpContext->FltSave.DataSelector;
        FPREG_MxCsr(native) = lpContext->FltSave.MxCsr;
        FPREG_MxCsr_Mask(native) = lpContext->FltSave.MxCsr_Mask;

        for (int i = 0; i < 8; i++)
        {
            FPREG_St(native, i) = lpContext->FltSave.FloatRegisters[i];
        }

        for (int i = 0; i < 16; i++)
        {
            FPREG_Xmm(native, i) = lpContext->FltSave.XmmRegisters[i];
        }
#endif
    }

    // TODO: Enable for all Unix systems
#if defined(_AMD64_) && defined(XSTATE_SUPPORTED)
    if ((lpContext->ContextFlags & CONTEXT_XSTATE) == CONTEXT_XSTATE)
    {
        _ASSERTE(FPREG_HasExtendedState(native));
        memcpy_s(FPREG_Xstate_Ymmh(native), sizeof(M128A) * 16, lpContext->VectorRegister, sizeof(M128A) * 16);
    }
#endif //_AMD64_ && XSTATE_SUPPORTED
}

/*++
Function :
    CONTEXTFromNativeContext
    
    Converts a native context to a CONTEXT record.

Parameters :
    const native_context_t *native : native context to convert
    LPCONTEXT lpContext : CONTEXT to fill in
    ULONG contextFlags : flags that determine which registers are valid in
                         native and which ones to set in lpContext

Return value :
    None

--*/
void CONTEXTFromNativeContext(const native_context_t *native, LPCONTEXT lpContext,
                              ULONG contextFlags)
{
    lpContext->ContextFlags = contextFlags;

#define ASSIGN_REG(reg) lpContext->reg = MCREG_##reg(native->uc_mcontext);
    if ((contextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL)
    {
        ASSIGN_CONTROL_REGS
#if defined(_ARM_)
        // WinContext assumes that the least bit of Pc is always 1 (denoting thumb)
        // although the pc value retrived from native context might not have set the least bit.
        // This becomes especially problematic if the context is on the JIT_WRITEBARRIER.
        lpContext->Pc |= 0x1;
#endif
    }

    if ((contextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER)
    {
        ASSIGN_INTEGER_REGS
    }
#undef ASSIGN_REG

#if HAVE_GREGSET_T || HAVE___GREGSET_T
#if HAVE_GREGSET_T
    if (native->uc_mcontext.fpregs == nullptr)
#elif HAVE___GREGSET_T
    if (native->uc_mcontext.__fpregs == nullptr)
#endif
    {
        // Reset the CONTEXT_FLOATING_POINT bit(s) and the CONTEXT_XSTATE bit(s) so it's
        // clear that the floating point and extended state data in the CONTEXT is not
        // valid. Since these flags are defined as the architecture bit(s) OR'd with one
        // or more other bits, we first get the bits that are unique to each by resetting
        // the architecture bits. We determine what those are by inverting the union of
        // CONTEXT_CONTROL and CONTEXT_INTEGER, both of which should also have the 
        // architecture bit(s) set.
        const ULONG floatingPointFlags = CONTEXT_FLOATING_POINT & ~(CONTEXT_CONTROL & CONTEXT_INTEGER);
        const ULONG xstateFlags = CONTEXT_XSTATE & ~(CONTEXT_CONTROL & CONTEXT_INTEGER);

        lpContext->ContextFlags &= ~(floatingPointFlags | xstateFlags);

        // Bail out regardless of whether the caller wanted CONTEXT_FLOATING_POINT or CONTEXT_XSTATE
        return;
    }
#endif

    if ((contextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT)
    {
#ifdef _AMD64_
        lpContext->FltSave.ControlWord = FPREG_ControlWord(native);
        lpContext->FltSave.StatusWord = FPREG_StatusWord(native);
        lpContext->FltSave.TagWord = FPREG_TagWord(native);
        lpContext->FltSave.ErrorOffset = FPREG_ErrorOffset(native);
        lpContext->FltSave.ErrorSelector = FPREG_ErrorSelector(native);
        lpContext->FltSave.DataOffset = FPREG_DataOffset(native);
        lpContext->FltSave.DataSelector = FPREG_DataSelector(native);
        lpContext->FltSave.MxCsr = FPREG_MxCsr(native);
        lpContext->FltSave.MxCsr_Mask = FPREG_MxCsr_Mask(native);

        for (int i = 0; i < 8; i++)
        {
            lpContext->FltSave.FloatRegisters[i] = FPREG_St(native, i);
        }

        for (int i = 0; i < 16; i++)
        {
            lpContext->FltSave.XmmRegisters[i] = FPREG_Xmm(native, i);
        }
#endif
    }

#ifdef _AMD64_
    if ((contextFlags & CONTEXT_XSTATE) == CONTEXT_XSTATE)
    {
    // TODO: Enable for all Unix systems
#if XSTATE_SUPPORTED
        if (FPREG_HasExtendedState(native))
        {
            memcpy_s(lpContext->VectorRegister, sizeof(M128A) * 16, FPREG_Xstate_Ymmh(native), sizeof(M128A) * 16);
        }
        else
#endif // XSTATE_SUPPORTED
        {
            // Reset the CONTEXT_XSTATE bit(s) so it's clear that the extended state data in
            // the CONTEXT is not valid.
            const ULONG xstateFlags = CONTEXT_XSTATE & ~(CONTEXT_CONTROL & CONTEXT_INTEGER);
            lpContext->ContextFlags &= ~xstateFlags;
        }
    }
#endif // _AMD64_
}


#else // !HAVE_MACH_EXCEPTIONS

#error deleted macOS stuff

#endif // !HAVE_MACH_EXCEPTIONS
