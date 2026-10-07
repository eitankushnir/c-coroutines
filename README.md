# Coroutines For C - Single Threaded Concurrency
Concurrency with no extra threads and kernel context switches.

## Features 
* New 'keywords' - `coroutine`, `yield`, `coroutine_spawn`, etc...
* Non-blocking I/O operations - `cr_read`, `cr_write`, etc...

## Making Coroutines
To make a coroutine use the `coroutine` macro. You must supply a name for the function.
```c
#define COROUTINE_IMPLEMENTATION
#include "coroutine.h"

coroutine(name) {
// Creates a function called name that can be run as a coroutine.
printf("Hello World\n");
...
}
```

If you want your coroutine to have an argument list, simple add the list of parameters separated by a `;`.  
You can access the arguments inside the function using the `get_args` macro with the function name and then using `args.<parameter_name>` to get the parameter value. 
```c
#define COROUTINE_IMPLEMENTATION
#include "coroutine.h"

coroutine(name, int a; int b; int c) {
// Make arguments accessible
get_args(name); 

// Get the value of c using args.c
printf("c: %d\n", args.c);
...
}
```
**A Coroutine can support up to 16 parameters**

## Using Scheduled Coroutines
In order to use a coroutine scheduler (which makes asnyc i/o possible) you must first set it up in the main function.  
You must also tear it down when finishing the program.
```c
#define COROUTINE_IMPLEMENTATION
#include "coroutine.h"

int main(...) {
// Creates the scheduler and makes main a coroutine.
setup_coroutines();
...
// Cleans up all used memeory
teardown_coroutines();
return 0;
}
```

To run a coroutine we simply use `coroutine_spawn` with the name of the function to run followed by function arguments if needed.
```c
#define COROUTINE_IMPLEMENTATION
#include "coroutine.h"

coroutine(example) {
printf ("Example\n");
}

int main(...) {
// Creates the scheduler and makes main a coroutine.
setup_coroutines();

// Spawn a coroutine running 'example'
coroutine_spawn(example);

// Cleans up all used memeory
teardown_coroutines();
return 0;
}
```

### Multitasking
A Coroutine runs until it decides to yield control back to the scheduler. A yield can be done using the following methods:
* `yield` macro - pauses execution but coroutine is still runnable.
* `yield_sleep(millis)` macro - pauses execution and will resume to run only after at least `millis` milliseconds have passed.
* `cr_read` / `cr_write` / Other I/O opertations - Tries to perform the operation. Instead of blocking yields until operation can be completed. **Does not block program**

## Using Unscheduled Coroutines
Coroutines can also be using in a caller/callee way. Instead of making them seperates tasks, coroutines are functions that can pause and resume execution.  
We must first initialize a coroutine using `coroutine_init`. This is when we pass in the arguments (if needed).

### Generators
Calling a coroutine is done via `coroutine_call`.
A coroutine can yield to allow the caller to resume in two ways:
* `yield` - Resume execution of the caller.
* `yield_val(type, value)` - Resume execution of the caller and return a value.

```c
#define COROUTINE_IMPLEMENTATION
#include "coroutine.h"
#include <stdio.h>

coroutine(counter, int start; int end; int step) {
  get_args(counter);

  while (args.start < args.end) {
    yield_val(int, args.start);
    args.start += args.step;
  }
}

int main() {
  coroutine_init(counter, 0, 10, 1);

  while (coroutine_is_alive(counter)) {
    int *curr = coroutine_call(counter);
    if (coroutine_is_alive(counter)) {
      printf("%d\n", *curr);
    }
  }

  coroutine_destroy(counter);
}
```
The example above will print the numbers from 0 to 9.
