#!/usr/bin/env bash
# Usage:
#   scripts/test_errors.sh          # runs every case, 1 through 23, in order
#   scripts/test_errors.sh <number> # runs just that one case
#
# Generates a tiny .shine snippet that triggers a specific default error
# message, compiles it with shinec, and prints the result.

set -uo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SHINEC="${SHINEC:-$ROOT_DIR/build/shinec}"

if [[ ! -x "$SHINEC" ]]; then
  echo "error: shinec not found at '$SHINEC'"
  echo "Build it first (e.g. cmake --build build), or set SHINEC=/path/to/shinec"
  exit 1
fi

run_case() {
TMP="$(mktemp /tmp/errtest_XXXX.shine)"
trap 'rm -f "$TMP" "${TMP%.shine}.o" "${TMP%.shine}"' RETURN

local n="$1"
write_case() { printf '%s' "$2" > "$TMP"; echo "== [$1] $3 =="; }

case "$n" in
  1) write_case 1 'fn int main() {
    r/ 0;
} /* unterminated comment
' "UnterminatedComment" ;;

  2) write_case 2 'fn int main() {
    write("unterminated;
    r/ 0;
}
' "UnterminatedString" ;;

  3) write_case 3 'fn int main() {
    write("bad \q escape");
    r/ 0;
}
' "BadEscapeSequence" ;;

  4) write_case 4 'fn int main() {
    r/ 0;
}
@
' "UnexpectedChar" ;;

  5) write_case 5 'fn int main(
' "ExpectedToken (also covers UnexpectedEof-ish cases)" ;;

  6) write_case 6 'fn main() {
    r/ 0;
}
' "ExpectedType" ;;

  7) write_case 7 'fn int main() {
    r/ ;
    ;
}
' "ExpectedExpression" ;;

  8) write_case 8 'fn int main() {
' "UnexpectedEof" ;;

  9) write_case 9 'fn foo main() {
    r/ 0;
}
' "ExpectedType again (see note: UnknownType is unreachable — parser only ever emits int/void types, so codegen mapType() never sees anything else)" ;;

  10) write_case 10 'fn int main() {
    let(void) x = 0;
    r/ 0;
}
' "VoidVariable" ;;

  11) write_case 11 'fn int main() {
    let(int) x = 1;
    let(int) x = 2;
    r/ 0;
}
' "VariableRedeclared" ;;

  12) write_case 12 'fn int main() {
    write(bogus);
    r/ 0;
}
' "UndeclaredIdentifier" ;;

  13) write_case 13 'fn int main() {
    let(int) x = 1;
    x = 2;
    r/ 0;
}
' "ImmutableAssign" ;;

  14) write_case 14 'fn int main() {
    r/ nope();
}
' "UndeclaredFunction" ;;

  15) write_case 15 'fn int add(a: int, b: int) {
    r/ a + b;
}
fn int main() {
    r/ add(1);
}
' "ArgCountMismatch" ;;

  16) write_case 16 'fn int main() {
    stop;
    r/ 0;
}
' "BreakOutsideLoop" ;;

  17) write_case 17 'fn int main() {
    cont;
    r/ 0;
}
' "ContinueOutsideLoop" ;;

  18) write_case 18 'fn int main() {
    write();
    r/ 0;
}
' "WriteArgCount" ;;

  19) write_case 19 'fn void nothing() {
    r/;
}
fn int main() {
    write(nothing());
    r/ 0;
}
' "WriteArgType" ;;

  20) write_case 20 'fn int main() {
    user_input();
    r/ 0;
}
' "UserInputArgCount" ;;

  21) write_case 21 'fn int main() {
    let(int) x = user_input(5);
    r/ 0;
}
' "UserInputArgType" ;;

  22) write_case 22 'fn int main() {
    terminal.pause(a, b);
    r/ 0;
}
' "TerminalPauseArgCount" ;;

  23) write_case 23 'fn int main() {
    terminal.pause(5);
    r/ 0;
}
' "TerminalPauseArgType" ;;

  *)
    echo "Usage: $0 [1-23]"
    echo
    echo " 1  UnterminatedComment      9  ExpectedType (see UnknownType note)  17 ContinueOutsideLoop"
    echo " 2  UnterminatedString      10  VoidVariable             18 WriteArgCount"
    echo " 3  BadEscapeSequence       11  VariableRedeclared       19 WriteArgType"
    echo " 4  UnexpectedChar          12  UndeclaredIdentifier     20 UserInputArgCount"
    echo " 5  ExpectedToken           13  ImmutableAssign          21 UserInputArgType"
    echo " 6  ExpectedType            14  UndeclaredFunction       22 TerminalPauseArgCount"
    echo " 7  ExpectedExpression      15  ArgCountMismatch         23 TerminalPauseArgType"
    echo " 8  UnexpectedEof           16  BreakOutsideLoop"
    return 1
    ;;
esac

cat "$TMP"
echo "-- compiler output --"
"$SHINEC" "$TMP" -o "${TMP%.shine}" 2>&1
echo
}

if [[ $# -eq 0 ]]; then
  for n in $(seq 1 23); do
    run_case "$n"
  done
else
  run_case "$1"
fi