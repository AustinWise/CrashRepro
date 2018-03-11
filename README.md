
# Crash of .NET Core 2 on LX-branded SmartOS

The crash happens whenever the [login page](https://github.com/AustinWise/DinnerKillPoints/blob/master/src/DkpWeb/Views/Account/Login.cshtml)
of my ASP.NET Core website is visited. The crash does not happen on .NET Core 1.1
and earlier or when running on native linux.

In the [csharp](./csharp) folder is a minimal reproduction of the bug in the
form of a .NET Core console app.

The [c](./c) folder contains a C program that can detect the problem.

# Root Cause

When .NET Core runs on Linux, it uses a `SIGSEGV` handler to turn `SIGSEGV`s in
managed code into `NullReferenceException`s.
When installing the `SIGSEGV` handler, .NET Core uses `sigaltstack(2)` to define
an alternate stack.

When the
[`SIGSEGV` handler](https://github.com/dotnet/coreclr/blob/release/2.0.0/src/pal/src/exception/signal.cpp#L449)
runs, it first checks to see if the fault address
is near the end of the stack the thread was running on at the time of the fault.
If so, it aborts the program after printing an stack-overflow error message.

This is where .NET starts to abuse the signal facility. It
[switches back to executing on the original stack](https://github.com/dotnet/coreclr/blob/release/2.0.0/src/pal/src/arch/amd64/signalhandlerhelper.cpp),
albeit a bit below where the fault occured.
The `SEHProcessException` that processes the exeception
[may never it return](https://github.com/dotnet/coreclr/blob/release/2.0.0/src/pal/src/exception/seh.cpp#L248).
After executing the any catch handlers it finds, it unwinds the stack and
restores the CPU context to resume executing. The signal handler never returns.

The SmartOS LX signal dispatcher
[records](https://github.com/joyent/illumos-joyent/blob/4ad2b82f02940919076bbd75002408a5bfef6a80/usr/src/lib/brand/lx/lx_brand/common/signal.c#L1796)
when it starts executing a signal
handler using the alternate stack from `sigaltstack(2)`. If another signal is
raised while before the first signal handler returns,
[the alternate stack is not used](https://github.com/joyent/illumos-joyent/blob/4ad2b82f02940919076bbd75002408a5bfef6a80/usr/src/lib/brand/lx/lx_brand/common/signal.c#L1638-L1640).
This means if
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
picked to setup the stack.
[signal_handler_worker](https://github.com/dotnet/coreclr/blob/release/2.0.0/src/pal/src/exception/signal.cpp#L409-L436)
clobbers the information
stored on the stack by the `SIGSEGV` handler and evently the `iretq` instruction
in `RtlRestoreContext` segfaults when trying to restore the clobbered machine
state.

## Addtional Notes

CoreCLR started using the alternate signal stack in 2.0:
[dotnet/coreclr#9650](https://github.com/dotnet/coreclr/pull/9650).
