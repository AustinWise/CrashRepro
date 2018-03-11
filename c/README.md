# C program for detecting and exposing the bug

This sample program mmaps 4 pieces of memory as non-readable and non-writeable.
It then tries to dereference each of the 4 pointers. A SIGSEGV handler handles
the protection error by changing the memory to be readable and writable and
containues execution.

When handling the segfault for the first memory location, the sigsegv handler
also writes to memory location 2. This tests the behavior of nested signals.

When handling the third and fouth segfaults, the signal handler does not return.
Instead it uses the `ucontext_t` provided to the signal handler to switch
execution context back to the point at which the segfault occured.

The context switching code is copied from the
[release/2.0.0](https://github.com/dotnet/coreclr/tree/release/2.0.0)
of CoreCLR.

# Example output from Ubuntu 17.10 running Linux kernel 4.13.0-36-generic

Notice that the RSP used for the signal handler for all 4 times is on the
alternate signal stack.

```
main stack: 0x7ffff11d0020 alt sig stack: 0x557023b67260
reading from mapped memory 0: 0
reading from mapped memory 1: 1
reading from mapped memory 2: 0
reading from mapped memory 3: 0
Observered signal stack rsp 0: 0x557023b68790
Observered signal stack rsp 1: 0x557023b67c50
Observered signal stack rsp 2: 0x557023b68790
Observered signal stack rsp 3: 0x557023b68790
all tests pass
```

# Example output from SmartOS 20180203T031130Z

Notice that in the last case the signal handler is execute on the main stack.

```
main stack: 0x7fffffeff480 alt sig stack: 0x1010
reading from mapped memory 0: 0
reading from mapped memory 1: 1
reading from mapped memory 2: 0
reading from mapped memory 3: 0
Observered signal stack rsp 0: 0x25f0
Observered signal stack rsp 1: 0x1be0
Observered signal stack rsp 2: 0x25f0
Observered signal stack rsp 3: 0x7fffffefea50
rsp[0] and rsp[3] don't match
rsp[3] is not in alt_signal_stack
some tests failed
```
