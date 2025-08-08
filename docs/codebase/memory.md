# Memory management

- Release: invoked when refcount is zero or on garbage collection
    - Calls destructor -> decreases refcounts
    - Frees own memory

- Finalize:
    - Often invoked from own destructor (but not always??)
    - Also invoked from the garbage collector after marking, before releasing..
    - What is its purpose if Release shall do its job?
