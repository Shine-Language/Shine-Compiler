#include "shine/error.h"

namespace shine {

namespace {

const char* templateFor(Err code) {
    switch (code) {
        // lexer
        case Err::UnterminatedComment:    return "unterminated comment";
        case Err::UnterminatedString:     return "unterminated string";
        case Err::BadEscapeSequence:      return "bad escape sequence";
        case Err::UnexpectedChar:         return "unexpected character '{}'";

        // parser
        case Err::ExpectedToken:          return "expected {} {}, got {}";
        case Err::ExpectedType:           return "expected a type";
        case Err::ExpectedExpression:     return "expected an expression";
        case Err::UnexpectedEof:          return "unexpected end of file in {}";

        // codegen: types & variables
        case Err::UnknownType:            return "unknown type '{}'";
        case Err::VoidVariable:           return "variables cannot have type void";
        case Err::VariableRedeclared:     return "variable '{}' is already declared";
        case Err::UndeclaredIdentifier:   return "undeclared identifier '{}'";
        case Err::ImmutableAssign:        return "cannot assign to immutable variable '{}'";

        // codegen: functions & calls
        case Err::UndeclaredFunction:     return "call to undeclared function '{}'";
        case Err::ArgCountMismatch:       return "'{}' expects {} argument(s), got {}";

        // codegen: control flow
        case Err::BreakOutsideLoop:       return "'stop' used outside of a loop";
        case Err::ContinueOutsideLoop:    return "'cont' used outside of a loop";

        // codegen: builtins
        case Err::WriteArgCount:          return "write() takes exactly 1 argument";
        case Err::WriteArgType:           return "write() only supports string literals or int expressions";
        case Err::UserInputArgCount:      return "user_input() takes exactly 1 argument";
        case Err::UserInputArgType:       return "user_input() prompt must be a string literal";
        case Err::TerminalPauseArgCount:  return "terminal.pause() takes at most 1 argument";
        case Err::TerminalPauseArgType:   return "terminal.pause() argument is just a placeholder name";

        // codegen: internal
        case Err::CodegenFailure:         return "codegen error in '{}': {}";
        case Err::UnhandledStatement:     return "unhandled statement";
        case Err::UnhandledExpression:    return "unhandled expression";
        case Err::UnknownBinaryOp:        return "unknown binary operator '{}'";
    }
    return "unknown error";
}

} // namespace

std::string defaultMessage(Err code, const std::vector<std::string>& args) {
    std::string out;
    const char* tmpl = templateFor(code);
    size_t argIdx = 0;
    for (size_t i = 0; tmpl[i] != '\0'; i++) {
        if (tmpl[i] == '{' && tmpl[i + 1] == '}') {
            if (argIdx < args.size()) out += args[argIdx++];
            i++; // skip the '}'
            continue;
        }
        out += tmpl[i];
    }
    return out;
}

}