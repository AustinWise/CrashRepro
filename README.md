
# Crash of .NET Core 2 on LX-branded SmartOS

The crash happens whenever the [login page](https://github.com/AustinWise/DinnerKillPoints/blob/master/src/DkpWeb/Views/Account/Login.cshtml)
of my ASP.NET Core website is visit. The crash does not happen on .NET Core 1.1
and earlier or when running on native linux.

In the [csharp](./csharp) folder is a minimal reproduction of the bug in the
form of a .NET Core console app. It appears the problem is related to exception
handling.

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

