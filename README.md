# colla
Standalone embedded C/C++ allocator

This is a small library without any dependencies, that can be compiled on any platform that has a standard C compiler.
The idea is to be able to do dynamic allocations from a block of memory that is obtained somehow.

The original idea was to provide simple allocations for an embedded device which mapped external RAM onto a single large buffer.
I wanted to use this platform to do some [wren](https://wren.io/) scripting, and the configurable allocator option there gave me the idea to taylor an allocator to fit the bill.
