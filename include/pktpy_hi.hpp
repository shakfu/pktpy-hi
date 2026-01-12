/*
 * pktpy_hi.hpp - C++ wrapper for pocketpy C-API
 *
 * A modern C++17 wrapper using RAII, templates, and move semantics to provide
 * a safe, ergonomic interface to pocketpy.
 *
 * Key features:
 * - RAII scope management (automatic cleanup, even on exceptions)
 * - Move-only Value type (prevents register aliasing bugs at compile time)
 * - Variadic templates for function calls
 * - std::optional for safe argument extraction
 * - Type-safe result handling
 *
 * Namespace: ph
 */

#pragma once

#define PK_IS_PUBLIC_INCLUDE
#include "pocketpy.h"

#include <optional>
#include <string_view>
#include <type_traits>
#include <cassert>
#include <utility>

namespace ph {

// ============================================================================
// 1. Exception Handling Policy
// ============================================================================

enum class ExcPolicy {
    Print,   // Print and clear (fire-and-forget)
    Raise,   // Keep for propagation (caller must handle)
    Silent   // Clear silently
};

// ============================================================================
// 2. RAII Scope Management
// ============================================================================
//
// The Scope class automatically handles stack unwinding and exception cleanup.
// The destructor guarantees cleanup even on early returns or exceptions.
//
// Usage:
//   {
//       ph::Scope scope;
//       // ... do Python operations ...
//   }  // automatic cleanup here
//

class Scope {
    py_StackRef unwind_point_;
    ExcPolicy policy_;
    mutable bool exception_checked_ = false;

public:
    explicit Scope(ExcPolicy policy = ExcPolicy::Print)
        : unwind_point_(py_peek(0)), policy_(policy) {}

    ~Scope() {
        bool has_exc = py_checkexc();

        if (has_exc) {
            switch (policy_) {
                case ExcPolicy::Print:
                    py_printexc();
                    py_clearexc(unwind_point_);
                    break;
                case ExcPolicy::Silent:
                    py_clearexc(unwind_point_);
                    break;
                case ExcPolicy::Raise:
                    // Don't clear - caller handles it
                    // But still restore stack
                    break;
            }
        }

        // Restore stack
        int depth = static_cast<int>(py_peek(0) - unwind_point_);
        if (depth > 0) {
            py_shrink(depth);
        }
    }

    // Non-copyable, non-movable (tied to stack frame)
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

    [[nodiscard]] bool ok() const {
        exception_checked_ = true;
        return !py_checkexc();
    }

    [[nodiscard]] bool failed() const {
        return !ok();
    }
};

// ============================================================================
// 3. Result Type
// ============================================================================
//
// A simple result type for operations that can fail.
// Similar to std::optional but with clearer semantics for error handling.
//

template<typename T = py_GlobalRef>
class Result {
    bool ok_;
    T value_;

public:
    Result() : ok_(false), value_{} {}
    explicit Result(T val) : ok_(true), value_(val) {}

    static Result failure() { return Result(); }
    static Result success(T val) { return Result(val); }

    [[nodiscard]] explicit operator bool() const { return ok_; }
    [[nodiscard]] bool ok() const { return ok_; }

    [[nodiscard]] T value() const {
        assert(ok_ && "Result::value() called on failed result");
        return value_;
    }

    [[nodiscard]] T value_or(T default_val) const {
        return ok_ ? value_ : default_val;
    }

    [[nodiscard]] T operator*() const { return value(); }
};

// ============================================================================
// 4. Move-Only Value Type
// ============================================================================
//
// pocketpy uses global registers (r0-r7) for temporary storage. A common bug
// is register aliasing - storing a register pointer, then overwriting it:
//
//   py_newint(py_r0(), 1);
//   py_GlobalRef a = py_r0();
//   py_newint(py_r0(), 2);  // a now points to 2, not 1!
//
// The Value type prevents this at compile time by being move-only.
// Copying is deleted, so you can't accidentally alias registers.
//
// Usage:
//   auto a = ph::Value::integer(1, 0);   // owns register 0
//   auto b = ph::Value::integer(2, 1);   // owns register 1
//   auto c = std::move(a);               // transfer ownership
//   // a is now empty, c owns the value
//

class Value {
    py_GlobalRef ref_ = nullptr;
    int reg_ = -1;

    Value(py_GlobalRef ref, int reg) : ref_(ref), reg_(reg) {}

public:
    // Default constructor creates empty/nil value
    Value() = default;

    // Factory functions - each allocates a different register
    static Value integer(py_i64 val, int reg) {
        assert(reg >= 0 && reg < 8 && "register must be 0-7");
        py_newint(py_getreg(reg), val);
        return Value(py_getreg(reg), reg);
    }

    static Value floating(py_f64 val, int reg) {
        assert(reg >= 0 && reg < 8 && "register must be 0-7");
        py_newfloat(py_getreg(reg), val);
        return Value(py_getreg(reg), reg);
    }

    static Value string(const char* val, int reg) {
        assert(reg >= 0 && reg < 8 && "register must be 0-7");
        py_newstr(py_getreg(reg), val);
        return Value(py_getreg(reg), reg);
    }

    static Value boolean(bool val, int reg) {
        assert(reg >= 0 && reg < 8 && "register must be 0-7");
        py_newbool(py_getreg(reg), val);
        return Value(py_getreg(reg), reg);
    }

    // Wrap an existing reference (non-owning, no register)
    static Value wrap(py_Ref r) {
        return Value(const_cast<py_GlobalRef>(r), -1);
    }

    // Move semantics - transfer ownership
    Value(Value&& other) noexcept
        : ref_(other.ref_), reg_(other.reg_) {
        other.ref_ = nullptr;
        other.reg_ = -1;
    }

    Value& operator=(Value&& other) noexcept {
        if (this != &other) {
            ref_ = other.ref_;
            reg_ = other.reg_;
            other.ref_ = nullptr;
            other.reg_ = -1;
        }
        return *this;
    }

    // Copying is deleted - prevents register aliasing bugs
    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;

    // Access
    [[nodiscard]] py_GlobalRef ref() const { return ref_; }
    [[nodiscard]] operator py_Ref() const { return ref_; }
    [[nodiscard]] int reg() const { return reg_; }
    [[nodiscard]] bool valid() const { return ref_ != nullptr; }

    // Type checks
    [[nodiscard]] bool is_int() const { return ref_ && py_isint(ref_); }
    [[nodiscard]] bool is_float() const { return ref_ && py_isfloat(ref_); }
    [[nodiscard]] bool is_str() const { return ref_ && py_isstr(ref_); }
    [[nodiscard]] bool is_bool() const { return ref_ && py_isbool(ref_); }
    [[nodiscard]] bool is_none() const { return ref_ && py_isnone(ref_); }
    [[nodiscard]] bool is_nil() const { return !ref_ || py_isnil(ref_); }

    // Direct extraction (caller must verify type)
    [[nodiscard]] py_i64 to_int() const { return py_toint(ref_); }
    [[nodiscard]] py_f64 to_float() const { return py_tofloat(ref_); }
    [[nodiscard]] const char* to_str() const { return py_tostr(ref_); }
    [[nodiscard]] bool to_bool() const { return py_tobool(ref_); }

    // Safe extraction with defaults
    [[nodiscard]] py_i64 as_int(py_i64 def = 0) const {
        return is_int() ? to_int() : def;
    }

    [[nodiscard]] py_f64 as_float(py_f64 def = 0.0) const {
        if (is_float()) return to_float();
        if (is_int()) return static_cast<py_f64>(to_int());
        return def;
    }

    [[nodiscard]] const char* as_str(const char* def = "") const {
        return is_str() ? to_str() : def;
    }

    [[nodiscard]] bool as_bool(bool def = false) const {
        return is_bool() ? to_bool() : def;
    }

    // Type info
    [[nodiscard]] const char* type_name() const {
        return ref_ ? py_tpname(py_typeof(ref_)) : "nil";
    }
};

// ============================================================================
// 5. Execution Helpers
// ============================================================================

inline bool exec(const char* source, const char* filename = "<string>",
                 ExcPolicy policy = ExcPolicy::Print) {
    Scope scope(policy);
    py_exec(source, filename, EXEC_MODE, nullptr);
    return scope.ok();
}

inline bool exec_in(const char* source, const char* filename, py_Ref module,
                    ExcPolicy policy = ExcPolicy::Print) {
    Scope scope(policy);
    py_exec(source, filename, EXEC_MODE, module);
    return scope.ok();
}

inline Result<py_GlobalRef> eval(const char* source,
                                  ExcPolicy policy = ExcPolicy::Print) {
    Scope scope(policy);
    py_eval(source, nullptr);
    if (scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

inline Result<py_GlobalRef> eval_in(const char* source, py_Ref module,
                                     ExcPolicy policy = ExcPolicy::Print) {
    Scope scope(policy);
    py_eval(source, module);
    if (scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

// ============================================================================
// 6. Function Calls (Variadic Templates)
// ============================================================================
//
// Variadic templates provide a unified interface for calling Python functions
// with any number of arguments (up to 4). For more arguments, use the explicit
// argument array overload or call methods directly.
//
// Usage:
//   ph::call("print", ph::Value::string("hello", 0));
//   ph::call("add", ph::Value::integer(1, 0), ph::Value::integer(2, 1));
//

namespace detail {

// Copy value to a register for stable storage during call
inline py_GlobalRef to_reg(int reg, const Value& v) {
    py_assign(py_getreg(reg), v.ref());
    return py_getreg(reg);
}

inline py_GlobalRef to_reg(int reg, py_Ref r) {
    py_assign(py_getreg(reg), r);
    return py_getreg(reg);
}

} // namespace detail

// Call with 0 arguments
inline Result<py_GlobalRef> call(const char* func_name) {
    Scope scope;

    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        return Result<py_GlobalRef>::failure();
    }

    if (py_call(fn, 0, nullptr) && scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

// Call with 1 argument
template<typename A0>
Result<py_GlobalRef> call(const char* func_name, A0&& a0) {
    Scope scope;

    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        return Result<py_GlobalRef>::failure();
    }

    // Use register 4 for argument (registers 0-3 may be in use by caller)
    py_GlobalRef arg = detail::to_reg(4, std::forward<A0>(a0));
    if (py_call(fn, 1, arg) && scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

// Call with 2 arguments
template<typename A0, typename A1>
Result<py_GlobalRef> call(const char* func_name, A0&& a0, A1&& a1) {
    Scope scope;

    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        return Result<py_GlobalRef>::failure();
    }

    // Use registers 4-5 for arguments (contiguous)
    detail::to_reg(4, std::forward<A0>(a0));
    detail::to_reg(5, std::forward<A1>(a1));
    if (py_call(fn, 2, py_getreg(4)) && scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

// Call with 3 arguments
template<typename A0, typename A1, typename A2>
Result<py_GlobalRef> call(const char* func_name, A0&& a0, A1&& a1, A2&& a2) {
    Scope scope;

    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        return Result<py_GlobalRef>::failure();
    }

    // Use registers 4-6 for arguments (contiguous)
    detail::to_reg(4, std::forward<A0>(a0));
    detail::to_reg(5, std::forward<A1>(a1));
    detail::to_reg(6, std::forward<A2>(a2));
    if (py_call(fn, 3, py_getreg(4)) && scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

// Call with 4 arguments
template<typename A0, typename A1, typename A2, typename A3>
Result<py_GlobalRef> call(const char* func_name, A0&& a0, A1&& a1, A2&& a2, A3&& a3) {
    Scope scope;

    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        return Result<py_GlobalRef>::failure();
    }

    // Use registers 4-7 for arguments (contiguous)
    detail::to_reg(4, std::forward<A0>(a0));
    detail::to_reg(5, std::forward<A1>(a1));
    detail::to_reg(6, std::forward<A2>(a2));
    detail::to_reg(7, std::forward<A3>(a3));
    if (py_call(fn, 4, py_getreg(4)) && scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

// Call any callable reference with 0 arguments
inline Result<py_GlobalRef> call(py_Ref callable) {
    Scope scope;
    if (py_call(callable, 0, nullptr) && scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

// Call any callable reference with explicit argc/argv
inline Result<py_GlobalRef> call(py_Ref callable, int argc, py_Ref argv) {
    Scope scope;
    if (py_call(callable, argc, argv) && scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

// Call a method on an object with 0 arguments
inline Result<py_GlobalRef> call_method(py_Ref obj, const char* method_name) {
    Scope scope;

    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        return Result<py_GlobalRef>::failure();
    }

    if (py_vectorcall(0, 0) && scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

// Call a method with 1 argument
template<typename A0>
Result<py_GlobalRef> call_method(py_Ref obj, const char* method_name, A0&& a0) {
    Scope scope;

    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        return Result<py_GlobalRef>::failure();
    }

    // Push argument for vectorcall
    if constexpr (std::is_same_v<std::decay_t<A0>, Value>) {
        py_push(a0.ref());
    } else {
        py_push(std::forward<A0>(a0));
    }

    if (py_vectorcall(1, 0) && scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

// Call a method with 2 arguments
template<typename A0, typename A1>
Result<py_GlobalRef> call_method(py_Ref obj, const char* method_name, A0&& a0, A1&& a1) {
    Scope scope;

    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        return Result<py_GlobalRef>::failure();
    }

    // Push arguments for vectorcall
    if constexpr (std::is_same_v<std::decay_t<A0>, Value>) {
        py_push(a0.ref());
    } else {
        py_push(std::forward<A0>(a0));
    }
    if constexpr (std::is_same_v<std::decay_t<A1>, Value>) {
        py_push(a1.ref());
    } else {
        py_push(std::forward<A1>(a1));
    }

    if (py_vectorcall(2, 0) && scope.ok()) {
        return Result<py_GlobalRef>::success(py_retval());
    }
    return Result<py_GlobalRef>::failure();
}

// Store result in a specific register for stability
template<typename... Args>
Result<py_GlobalRef> call_r(int reg, const char* func_name, Args&&... args) {
    auto result = call(func_name, std::forward<Args>(args)...);
    if (result) {
        assert(reg >= 0 && reg < 8 && "register must be 0-7");
        py_assign(py_getreg(reg), result.value());
        return Result<py_GlobalRef>::success(py_getreg(reg));
    }
    return result;
}

// ============================================================================
// 7. Binding Helpers
// ============================================================================

inline void def(const char* sig, py_CFunction f) {
    py_GlobalRef main_mod = py_getmodule("__main__");
    py_bind(main_mod, sig, f);
}

inline void def_in(const char* module_path, const char* sig, py_CFunction f) {
    py_GlobalRef mod = py_getmodule(module_path);
    if (!mod) {
        mod = py_newmodule(module_path);
    }
    py_bind(mod, sig, f);
}

inline void set_global(const char* name, py_Ref val) {
    py_setglobal(py_name(name), val);
}

inline void set_global(const char* name, const Value& val) {
    py_setglobal(py_name(name), val.ref());
}

inline py_ItemRef get_global(const char* name) {
    return py_getglobal(py_name(name));
}

inline py_GlobalRef module(const char* path) {
    py_GlobalRef mod = py_getmodule(path);
    if (!mod) {
        mod = py_newmodule(path);
    }
    return mod;
}

// ============================================================================
// 8. Type-Safe Argument Extraction
// ============================================================================
//
// Templates with std::optional provide type-safe argument extraction in
// native functions.
//
// Usage in native function:
//   auto x = ph::arg<py_i64>(argv, 0);
//   if (!x) return false;  // type error already raised
//   auto y = ph::arg<py_i64>(argv, 1).value_or(10);  // with default
//

template<typename T>
struct ArgExtractor;

template<>
struct ArgExtractor<py_i64> {
    static std::optional<py_i64> get(py_StackRef argv, int i) {
        py_i64 val;
        if (py_castint(&argv[i], &val)) {
            return val;
        }
        return std::nullopt;
    }
};

template<>
struct ArgExtractor<py_f64> {
    static std::optional<py_f64> get(py_StackRef argv, int i) {
        py_f64 val;
        if (py_castfloat(&argv[i], &val)) {
            return val;
        }
        return std::nullopt;
    }
};

template<>
struct ArgExtractor<const char*> {
    static std::optional<const char*> get(py_StackRef argv, int i) {
        if (py_checkstr(&argv[i])) {
            return py_tostr(&argv[i]);
        }
        return std::nullopt;
    }
};

template<>
struct ArgExtractor<bool> {
    static std::optional<bool> get(py_StackRef argv, int i) {
        if (py_checkbool(&argv[i])) {
            return py_tobool(&argv[i]);
        }
        return std::nullopt;
    }
};

template<>
struct ArgExtractor<py_Ref> {
    static std::optional<py_Ref> get(py_StackRef argv, int i) {
        return &argv[i];  // Always succeeds
    }
};

// Main argument extraction function
template<typename T>
std::optional<T> arg(py_StackRef argv, int i) {
    return ArgExtractor<T>::get(argv, i);
}

// Check if argument should use default (nil or None)
inline bool arg_is_default(py_StackRef argv, int i) {
    return py_isnil(&argv[i]) || py_isnone(&argv[i]);
}

// ============================================================================
// 9. Return Helpers
// ============================================================================

inline bool ret_int(py_i64 val) {
    py_newint(py_retval(), val);
    return true;
}

inline bool ret_float(py_f64 val) {
    py_newfloat(py_retval(), val);
    return true;
}

inline bool ret_str(const char* val) {
    py_newstr(py_retval(), val);
    return true;
}

inline bool ret_bool(bool val) {
    py_newbool(py_retval(), val);
    return true;
}

inline bool ret_none() {
    py_newnone(py_retval());
    return true;
}

inline bool ret(py_Ref val) {
    py_assign(py_retval(), val);
    return true;
}

inline bool ret(const Value& val) {
    py_assign(py_retval(), val.ref());
    return true;
}

// ============================================================================
// 10. List Helpers
// ============================================================================

// Iterate with callback (lambda-friendly)
template<typename Fn>
bool list_foreach(py_Ref list, Fn&& callback) {
    int len = py_list_len(list);
    for (int i = 0; i < len; i++) {
        py_ItemRef item = py_list_getitem(list, i);
        if (!callback(i, item)) return false;
    }
    return true;
}

// Build list from initializer list
inline void list_from_ints(py_OutRef out, std::initializer_list<py_i64> vals) {
    py_newlistn(out, static_cast<int>(vals.size()));
    int i = 0;
    for (auto val : vals) {
        py_newint(py_list_getitem(out, i++), val);
    }
}

inline void list_from_floats(py_OutRef out, std::initializer_list<py_f64> vals) {
    py_newlistn(out, static_cast<int>(vals.size()));
    int i = 0;
    for (auto val : vals) {
        py_newfloat(py_list_getitem(out, i++), val);
    }
}

inline void list_from_strings(py_OutRef out, std::initializer_list<const char*> vals) {
    py_newlistn(out, static_cast<int>(vals.size()));
    int i = 0;
    for (auto val : vals) {
        py_newstr(py_list_getitem(out, i++), val);
    }
}

// Build list from any container
template<typename Container>
void list_from(py_OutRef out, const Container& vals) {
    py_newlistn(out, static_cast<int>(vals.size()));
    int i = 0;
    for (const auto& val : vals) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_integral_v<T>) {
            py_newint(py_list_getitem(out, i++), static_cast<py_i64>(val));
        } else if constexpr (std::is_floating_point_v<T>) {
            py_newfloat(py_list_getitem(out, i++), static_cast<py_f64>(val));
        } else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
            py_newstr(py_list_getitem(out, i++), val);
        }
    }
}

// ============================================================================
// 11. Debug Helpers
// ============================================================================

inline void print(py_Ref val) {
    Scope scope;
    if (py_repr(val)) {
        py_Callbacks* cb = py_callbacks();
        if (cb->print) {
            cb->print(py_tostr(py_retval()));
            cb->print("\n");
        }
    }
}

inline const char* repr(py_Ref val) {
    if (!py_repr(val)) {
        py_clearexc(nullptr);
        return "<repr failed>";
    }
    return py_tostr(py_retval());
}

inline const char* type_name(py_Ref val) {
    return py_tpname(py_typeof(val));
}

} // namespace ph
