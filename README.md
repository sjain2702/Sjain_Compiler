# Lille Compiler – Semantic Analysis Phase (CSC-4510)

Student: **Shlok Jain**

## Build and run
```
cd "Compiler starter kit"
make                         # builds ./compiler
./compiler tests/semantic/ok_nested_scopes_and_shadowing.lille
make test                    # regression suite (tests/run_tests.sh)
```

## Symbol table = array of BSTs (`id_table.h/.cpp`)
* `roots[k]` is the BST of identifiers declared at scope level `k`; the array index *is* the scope level.
* `enter_scope()` activates the next level with an empty BST.
* `insert()` does a BST insert into the **current** level only (returns false on a duplicate in that level).
* `lookup()` searches the current level's BST, then each enclosing level outward – inner declarations hide outer ones.
* `exit_scope()` **prints every active level (0..current, including the one being exited) using an inorder
  traversal, then frees that level's BST** and deactivates the level.
* Each entry stores name, type, kind, scope level, line, position, value, parameter types/modes, return type.
* Level 0 = predefined library functions (`INT2STRING`, `REAL2STRING`, `STRING2INT`, `STRING2REAL`) + the program name;
  level 1 = program block; each procedure/function body and each FOR loop opens one more level.

## Semantic checks (done in `parser.cpp`, syntax-directed)
| Error | Rule |
|---|---|
| 81 / 82 | identifier not declared / declared twice in one scope (incl. duplicate parameters) |
| 85 | assignment (or `read`) target is not a variable or ref parameter (constants, FOR variables, routines) |
| 87 / 88 | RETURN value type vs. function type; RETURN outside a routine or with a value in a procedure |
| 89 | EXIT outside a loop |
| 90 / 91 / 121 | call of a non-procedure; procedure/program name used as a value; call of a non-function |
| 93 | assignment type mismatch (integer → real allowed) |
| 97 / 98 / 99 | argument count / type / kind (REF formal needs a variable) |
| 102 / 103 / 104 | ODD and `in` need integers; conditions must be boolean; ranges must be integer |
| 107 | closing block name differs from the block's name |
| 109 | function without a RETURN |
| 110 / 111 | constant with no value / initial value of wrong type |
| 114 / 115 / 116 | relational operand mismatch; `&` needs strings; arithmetic needs numbers |
| 123 | function with a REF parameter |

All errors are reported with line and position. After an error the offending expression becomes "unknown" and is
compatible with everything, so one mistake does not cascade.

## Tests
`tests/semantic/err_NNN_*.lille` – one deliberate error each (first line `-- EXPECT: NNN`); the suite checks that
exactly that one error is reported. `ok_*.lille` and the provided `program1..9` must give 0 errors and print the table.
`tests/semantic/ok_nested_scopes_and_shadowing.expected_output.txt` is the reference output for the video demo.

## Assumptions
* The predefined function list is in `id_table::declare_builtins()` (one line per function) – adjust to the Lille handout.
* FOR loop variables are read-only (error 85 on assignment), following the wording of error 85.
