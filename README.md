Bai Nangua 白南瓜 (white pumpkin)
================================

A Vulkan renderer written to investigate and experiment with C\+\+20 features and functional programming in C++.

In this code I make a bunch of intentional design choices. Some of these choices may be a bad idea.
Only by implementing them can we find out how terrible (or maybe not terrible?) they are.

Let's start off with:

Value Semantics
- Immutable data structures where possible.
- Take advantage of the cheap copies and structural sharing of immer's implementation.
- Pass around dumb structs as values instead of using references to classes packed full of methods.
- Custom memory allocators will make a big difference in performance!

Functional Programming
- Use Haskell-style "bracket" functions although under the hood it's probably still RAII :stuck_out_tongue_winking_eye:
- Use (abuse?) the pipe operator | to compose multiple small functions using expression templates.
- Use row polymorphic types to allow more freedom and flexibility in function composition order.
- Vulkan functions typically return a `VkResult` or `vk::Result` to indicate success. Where possible we convert these into `expected` values and use the related chain operators `.and_then()` and `.or_else`

---------

Licenses
========


```tanuki``` is under MPL2.0: https://www.mozilla.org/en-US/MPL/2.0/

```tl-expected``` is under CC0 public domain: http://creativecommons.org/publicdomain/zero/1.0/

----------

Further writings on current implementation details....
========


Chaining Function Wrappers with "|" and Row Types
------

When creating all the initial objects for Vulkan, you typically need to create and object and then use it
to create subsequent objects. For example:
- ```vk::Instance``` -> ```vk::PhysicalDevice```
- ```vk::Instance``` ```GLFWWindow``` -> ```vk::Surface```
- ```vk::Surface``` -> ```vk::Device``` ```vk::Queue```

All this interdependency makes it difficult to split all these creation events into different
functions, so typically there is one large "make all the initial objects" function. Alternatively, all of the
objects are dumped into an OOP-style class as they are created via class methods.

The process used here is to break out each creation event into a separate function and then pass the
result to the next function in the chain:

```
auto vulkanStages =
    GLFWOuterWrapper()
    | StandardVulkanInstance()
    | FirstSwapchainPhysicalDevice()
    | CreateGLFWWindowAndSurface()
    | StandardDevice() 
    | StandardVMAAllocator() 
    | InvokeInnerCode();
```

Each stage takes in objects/values created by previous stages. Then that stage adds or modifies values
and passes them to the next stage. For instance, the ```StandardDevice()``` stage takes an ```vk::Instance``` and
```vk::Surface``` passed as input, creates a ```vk::Device``` and passes that (along with the original inputs) to
the next stage.

How do we package up all these objects between stages? We can't just use a struct holding all the objects, since
the lifetimes of various objects are different.  We *could* use a struct full of ```std::optional```'s but
then we need a whole bunch of extra noise in our struct and we also need to check ```has_value()``` a lot just
to be safe.

What we can use instead are *Row Polymorphic Types* or "Open Tuples" which utilize [Row Polymorphism](https://en.wikipedia.org/wiki/Row_polymorphism).
In effect these are anonymous structs with named fields. What makes then different from a typical C ```struct```?
- You can add or remove fields from a Row Type at compile time, producing a new type.
- Two Row types are "equal" if they have the same fields. This is structural typing, where two values are
  of the same type if they carry the same information structure. This is in contrast to "nominal typing" where two values are
  the same type only if they have the same type name.
- A function can require or modify specific parts of a Row Type while ignoring the rest of it. 
  This allows you to write a function that (for example) accepts any type as input as long as that
  type has fields named "instance" and "config".

The Row Types used here are currently ```boost::hana::map```'s, built and modified at compile time.


(Ab)using coroutines for high concurrency in resource loading and unloading
-------

The resource loader uses a task-based system where a thread pool grabs suspended tasks off a queue and runs them, using `libcoro::thread_pool`.
Any code that loads or unloads a resource is a coroutine. This allows dependencies to be handled properly using `co_await`. For example is a texture
loader needs to get an allocated buffer, it can `co_await` for the allocator. While waiting the texture loader is suspended and other tasks can be run.
