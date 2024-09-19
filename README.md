Bai Nangua 白南瓜 (white pumpkin)
================================

A vulkan renderer written to investigate and experiment with C++20 features and functional programming in C++.

In this code I make a bunch of intentional design choices, some which may be a bad idea. Only by implementing
them can we find out how terrible they are.

Value Semantics
- Immutable data structures where possible.
- Take advantage of the cheap copies and structural sharing of immer's implementation.
- Pass around dumb structs as values instead of using references to classes packed full of methods.
- IMPORTANT: custom memory allocators will make a big difference in performance.

Functional Programming
- Use Haskell-style "bracket" functions although under the hood it's probably still RAII.
- Use (abuse?) the pipe operator | to compose multiple small functions.
- Use row types to allow more freedom of function composition order.
