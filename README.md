
# Crash of .NET Core 2 on LX-branded SmartOS

The crash happens whenever the [login page](https://github.com/AustinWise/DinnerKillPoints/blob/master/src/DkpWeb/Views/Account/Login.cshtml)
of my ASP.NET Core website is visit. The crash does not happen on .NET Core 1.1
and earlier or when running on native linux.

In the [csharp](./csharp) folder is a minimal reproduction of the bug in the
form of a .NET Core console app.

The [c](./c) folder contains a C program that can detect the problem.

# Root Cause

.NET Core handles `SIGSEGV` and turns it into a `NullReferenceException`.
When installing the `SIGSEGV` handler, .NET Core uses `sigaltstack(2)` to define
an alternate stack.

When the `SIGSEGV` handler runs, it first checks to see if the fault address
is near the end of the stack the thread was running on at the time of the fault.
If so, it aborts the program after printing an stack-overflow error message.

This is where .NET starts to abuse the signal facility. It switches back to
executing on the original stack, albeit a bit below where the fault occured.
The `SEHProcessException` that processes the exeception may never it return.
After executing the any catch handlers it finds, it unwinds the stack and
restores the CPU context to resume executing. The signal handler never returns.

The SmartOS LX signal dispatcher records when it starts executing a signal
handler using the alternate stack from `sigaltstack(2)`. If another signal is
raised while before the first signal handler returns, the handler is executed
on the stack the thread was using at the time of the fault. This means if
the signal handler never returns, the only time the alternate stack will be used
is the first time an exception is dispatched. This differs the Linux behavior.
Linux appears to check to see if the stack pointer at the time the fault occured
lies within the alternate stack. If the stack pointer is not contained within
the alternet stack, Linux always starts executing the handler at the top of the
alternate stack.

The SmartOS behavior causes problems for .NET Core. The `SIGSEGV` handler
captures the machine context using .NET Core's `RtlCaptureContext` function
and stores this information on the alternate stack.
`ExecuteHandlerOnOriginalStack` passes a pointer to this structure to
`signal_handler_worker` when it pivots to executing on the orignal stack.
Unforunatly on SmartOS, the second time the `SIGSEGV` handler is executed it
will run on the regular stack. When it thinks it pivoting back to a different
stack, it is actually is the same stack in about the same place the SmartOS
picked to setup the stack. The `signal_handler_worker` clobbers the information
stored on the stack by the `SIGSEGV` handler and evently the `iretq` instruction
in `RtlRestoreContext` segfaults when trying to restore the clobbered machine
state.

## Notes

Probably introduced by [dotnet/coreclr#9650](https://github.com/dotnet/coreclr/pull/9650).

This pull request changed how SIGSEGV is handled.
The SIGSEGV handler now switches to an alternate stack.

Removing the `SA_ONSTACK` from the signal handler registration
fixes the problem.

The program seg faults at the `iretq` instructions in `RtlRestoreContext` in
[amd64/context2.s](https://github.com/dotnet/coreclr/blob/release/2.0.0/src/pal/src/arch/amd64/context2.S#L204)
. There are two cases where the iretq instruction fails. In some cases
there are invalid values on the stack, because of earlier in the function
the wrong size `mov`s are used to place values on the stack. This results
in garbage on the stack being passed to `iretq`.

The second case is cause by all zeros being pasted to `iretq`. These
zeros come from the a `CONTEXT` that is lives on the stack being clobber.
The `CONTEXT` should be allocated on the alternate signal stack. However
it looks like the second `SIGSEGV` is delivered on the regular thread stack.

