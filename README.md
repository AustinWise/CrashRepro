
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
. In the failing case the `iretq` instruction loads a `0x0` value for `RIP` from
the stack. I now need to find out what is putting the `0x0` value there.
