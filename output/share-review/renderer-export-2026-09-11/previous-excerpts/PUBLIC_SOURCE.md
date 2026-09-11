# Public reference

The hook examples illustrate mechanisms already present in this public revision:

[Public framework](https://github.com/luka-dev/mib2q-carplay-rgi/blob/f36790d450392516ed9cbfab9cc06aa13a08bf31/hook/framework/hook_framework.c)

Reference commit: `f36790d450392516ed9cbfab9cc06aa13a08bf31`.

The public framework already contains next-symbol lookup, lazy initialization,
process filtering and a module-dispatch mechanism. The examples here are small
educational adaptations, not verbatim copies or a complete framework export.
The process-name comparison is exact and returns an explicit unknown state;
the example API is fictional and its observations are limited to counters.

The two video helper implementations are selected local excerpts. They are not
claimed to be files from the public revision. Original development paths and
private integration details are not included in this selection.
