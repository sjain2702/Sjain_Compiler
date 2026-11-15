#!/bin/sh
# run_tests.sh - regression tests for the scanner, parser and semantic analyser.
# Student : Shlok Jain   Course : CSC-4510
#
#   1. Lexical regression checks (original tests).
#   2. Every tests/semantic/err_*.lille program must report EXACTLY ONE error and it
#      must be the error number named on its first line ("-- EXPECT: nn").
#   3. Every tests/semantic/ok_*.lille program and every provided sample (program1..9)
#      must compile with 0 errors AND print the symbol table on each scope exit.
#   4. Symbol-table printout is checked: scope headings, inorder (alphabetical) names.
set -u
cd "$(dirname "$0")/.."
make clean >/dev/null 2>&1
make >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 1; }
fail=0
bad() { echo "FAIL: $1"; fail=$((fail+1)); }

# ---- 1. lexical regressions -------------------------------------------------
out=$(./compiler tests/literal_regressions.lil 2>&1)
[ "$(printf '%s\n' "$out" | grep -c 'ERROR')" -eq 4 ] || bad "literal regressions (expected 4 lexical diagnostics)"
printf '%s\n' "$out" | grep -q 'range symbol' && bad "malformed number misreported as range symbol"
echo "lexical regressions checked"

# ---- 2. semantic error programs ---------------------------------------------
for f in tests/semantic/err_*.lille; do
	expect=$(sed -n '1s/.*EXPECT: *\([0-9]*\).*/\1/p' "$f")
	out=$(./compiler "$f" 2>&1)
	count=$(printf '%s\n' "$out" | grep -c 'Error #')
	printf '%s\n' "$out" | grep -q "Error #$expect " || bad "$f : expected Error #$expect"
	[ "$count" -eq 1 ] || bad "$f : expected exactly 1 error, got $count"
done
echo "semantic error programs checked"

# ---- 3. valid programs: zero errors + symbol table printed -------------------
for f in tests/semantic/ok_*.lille tests/semantic/../../program[1-9]; do
	out=$(./compiler "$f" 2>&1)
	printf '%s\n' "$out" | grep -q ' 0 errors found' || bad "$f : expected 0 errors"
	printf '%s\n' "$out" | grep -q 'SYMBOL TABLE' || bad "$f : no symbol table printed"
done
echo "valid programs checked"

# ---- 4. symbol-table content ------------------------------------------------
out=$(./compiler tests/semantic/ok_nested_scopes_and_shadowing.lille 2>&1)
printf '%s\n' "$out" | grep -q 'EXITING SCOPE LEVEL 3' || bad "no level-3 (nested routine) exit printout"
printf '%s\n' "$out" | grep -q 'Scope level 2   <-- scope being exited' || bad "exiting scope not marked"
# the FOR-loop variable must appear when its scope exits...
printf '%s\n' "$out" | grep -q 'FOR_IDENT' || bad "FOR_IDENT not shown"
# ...and must NOT survive into the table printed after the loop scope is gone
last_l1=$(printf '%s\n' "$out" | awk '/EXITING SCOPE LEVEL 1/{f=1} f' )
printf '%s\n' "$last_l1" | grep -q 'FOR_IDENT' && bad "for variable leaked after its scope exited"
# inorder traversal => names alphabetical inside a scope (level 1 of the final table)
names=$(printf '%s\n' "$last_l1" | awk '/^Scope level 1/{f=1;next} /^Scope level|^===/{f=0} f && NF && $1!="NAME"{print $1}' | head -20)
sorted=$(printf '%s\n' "$names" | sort)
[ "$names" = "$sorted" ] || bad "BST inorder traversal not alphabetical"
echo "symbol table output checked"

if [ "$fail" -eq 0 ]; then echo "ALL TESTS PASSED"; else echo "$fail TEST(S) FAILED"; exit 1; fi
