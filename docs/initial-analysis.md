# Analysis: Higher-Level Wrapper for pocketpy C-API

## Current API Characteristics

The existing pocketpy.h API (~1139 lines) is a **low-level, stack-based interface** with:
- 140+ functions across 17 categories
- Manual stack manipulation (push/pop/peek)
- Multiple reference types (`py_Ref`, `py_OutRef`, `py_ItemRef`, etc.) all as raw `py_TValue*`
- Boolean return + global exception state for error handling
- Two inconsistent binding paradigms (decl-based vs argc-based)

## Key Pain Points a Wrapper Could Address

| Problem | Impact | Wrapper Solution |
|---------|--------|------------------|
| Manual stack management | Easy to corrupt state, forget cleanup | Scope guards / RAII-style macros |
| Reference lifetime confusion | Use-after-free risks | Typed wrappers with clear semantics |
| Error handling boilerplate | Forgetting `py_checkexc()` | Auto-check patterns, exception-safe helpers |
| `py_OutRef` awkwardness | Non-idiomatic C | Return-by-value helpers |
| Binding inconsistency | Cognitive load | Single unified binding approach |

## Arguments For the Wrapper

1. **Safety**: Automatic stack cleanup prevents common bugs
2. **Ergonomics**: Reduce 5-10 lines of boilerplate to 1-2 lines
3. **Composability**: Both APIs usable together - drop to low-level when needed
4. **Discoverability**: Group related operations logically
5. **Error handling**: Exception-safe patterns reduce silent failures

## Arguments Against / Risks

1. **Maintenance burden**: Two APIs to keep in sync with upstream pocketpy changes
2. **Performance concerns**: Extra indirection (likely negligible, but worth measuring)
3. **Learning curve**: Users must understand both layers for advanced use
4. **Abstraction leakage**: Some low-level concepts will inevitably leak through
5. **Scope creep**: Risk of reinventing a C++ API in C (with macros)

## Verdict: Good idea, with caveats

A higher-level wrapper is justified **if you constrain scope**:

### Do:
- Create scope-managed execution contexts (automatic unwinding point tracking)
- Provide exception-safe function call wrappers
- Simplify common patterns: "call Python function with C args, get C result"
- Unify binding with a single, well-documented approach
- Use static inline functions (zero overhead, both APIs interoperable)

### Don't:
- Try to hide the stack model entirely (it leaks anyway)
- Create a "complete" abstraction (keep it thin)
- Add features beyond the underlying API

## Suggested Design Approach

```c
// Example: thin wrapper that manages unwinding automatically
typedef struct { py_StackRef unwind; } pkpy_scope;

static inline pkpy_scope pkpy_begin(void) {
    return (pkpy_scope){ py_peek(0) };
}

static inline bool pkpy_end(pkpy_scope* s, bool ok) {
    if (!ok) py_clearexc(&s->unwind);
    return ok;
}

// Usage:
pkpy_scope scope = pkpy_begin();
bool ok = py_exec("print('hello')", "<eval>", EXEC_MODE, NULL);
pkpy_end(&scope, ok);
```

This keeps both APIs fully interoperable while reducing error-prone boilerplate.

## Recommendation

Start with 3-5 thin wrapper functions targeting the highest-friction pain points (scope management, safe function calls, simplified binding). Expand only if those prove valuable.
