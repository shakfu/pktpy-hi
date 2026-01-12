# Changelog

## [0.2.0]

### Added
- **Version macros**: `PH_VERSION_MAJOR`, `PH_VERSION_MINOR`, `PH_VERSION_PATCH`, `PH_VERSION_STRING`
- **Function call variants**:
  - `ph_callmethod2`, `ph_callmethod3` (base variants with 2 and 3 arguments)
  - `ph_callmethod2_raise`, `ph_callmethod3_raise` (exception propagation)
  - `ph_callmethod2_r`, `ph_callmethod3_r` (register-backed)
  - All `*_r_raise` variants combining stable storage with exception propagation
- **Value extraction**: `ph_is_truthy_raise` for exception-propagating truthiness check
- **Argument macros**: `PH_ARG_REF(i, var)` for raw `py_Ref` without type checking
- **List helpers**: `ph_list_from_bools` for building lists from C bool arrays
- **Bounds checking**: All `PH_ARG_*` macros now verify argument index is within `argc`

### Changed
- **Renamed** `ph_int`, `ph_str`, `ph_float`, `ph_bool` to `ph_tmp_int`, `ph_tmp_str`, `ph_tmp_float`, `ph_tmp_bool` to clarify temporary nature
- **Register validation**: `ph_*_r` functions now return NULL on invalid register (was assert)
- **Integer overflow fix**: Scope management uses `ptrdiff_t` for pointer arithmetic
- **Headers independent**: `pktpy_hi.h` and `pktpy_hi.hpp` no longer reference each other
- **Documentation**: Updated api-design.md and README.md for accuracy

### Removed
- `docs/initial-analysis.md` (superseded by api-design.md and README.md)
- Backward compatibility aliases (`ph_int`, etc.) - project not yet released

## [0.1.2]

## [0.1.1]

### Added
- **pktpy_hi.hpp** (`include/pktpy_hi.hpp`)
  - Initial higher-level cpp-header wrapper for pocketpy.h

## [0.1.0]

### Added
- **pktpy_hi.h** (`include/pktpy_hi.h`)
  - Initial higher-level c-header wrapper for pocketpy.h
