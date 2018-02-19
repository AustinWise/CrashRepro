
# Crash of .NET Core 2 on LX-branded SmartOS

## Notes

Probably introduced by [dotnet/coreclr#9650](https://github.com/dotnet/coreclr/pull/9650).

This pull request changed how SIGSEGV is handled.
The SIGSEGV handler now switches to an alternate stack.
