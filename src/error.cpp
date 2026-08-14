#include "shine/error.h"
#include <algorithm>

namespace shine {

namespace {

const char* templateFor(Err code) {
    switch (code) {
        case Err::UnterminatedComment:    return "unterminated comment";
        case Err::UnterminatedString:     return "unterminated string";
        case Err::BadEscapeSequence:      return "bad escape sequence";
        case Err::UnexpectedChar:         return "unexpected character '{}'";

        case Err::ExpectedToken:          return "expected {} {}, got {}";
        case Err::ExpectedType:           return "expected a type";
        case Err::ExpectedExpression:     return "expected an expression";
        case Err::UnexpectedEof:          return "unexpected end of file in {}";

        case Err::UnknownType:            return "unknown type '{}'";
        case Err::VoidVariable:           return "variables cannot have type void";
        case Err::VariableRedeclared:     return "variable '{}' is already declared";
        case Err::UndeclaredIdentifier:   return "undeclared identifier '{}'{}";
        case Err::ImmutableAssign:        return "cannot assign to immutable variable '{}'";
        case Err::AddressOfNonVariable:   return "'&' can only be applied to a variable";
        case Err::DerefNonVariable:       return "'*' can only dereference a variable";
        case Err::DerefNonPointer:        return "cannot dereference non-pointer '{}'";
        case Err::TypeMismatch:           return "expected type '{}', got '{}'";
        case Err::ReturnTypeMismatch:     return "function '{}' expects return type '{}', got '{}'";
        case Err::VoidExpression:         return "expression cannot have type void";
        case Err::UnknownStruct:          return "unknown struct type '{}'";
        case Err::UnknownField:           return "struct '{}' has no field '{}'";
        case Err::FieldAccessNonStruct:   return "cannot access field '{}' on non-struct type '{}'";
        case Err::StructLiteralMismatch:  return "struct literal for '{}' does not match its fields";

        case Err::UndeclaredFunction:     return "call to undeclared function '{}'{}";
        case Err::ArgCountMismatch:       return "'{}' expects {} argument(s), got {}";

        case Err::BreakOutsideLoop:       return "'stop' used outside of a loop";
        case Err::ContinueOutsideLoop:    return "'cont' used outside of a loop";

        case Err::WriteArgCount:          return "write() takes exactly 1 argument";
        case Err::WriteArgType:           return "write() only supports string literals or int expressions";
        case Err::UserInputArgCount:      return "user_input() takes exactly 1 argument";
        case Err::UserInputArgType:       return "user_input() prompt must be a string literal";
        case Err::TerminalPauseArgCount:  return "terminal.pause() takes at most 1 argument";
        case Err::TerminalPauseArgType:   return "terminal.pause() argument is just a placeholder name";

        case Err::CodegenFailure:         return "codegen error in '{}': {}";
        case Err::UnhandledStatement:     return "unhandled statement";
        case Err::UnhandledExpression:    return "unhandled expression";
        case Err::UnknownBinaryOp:        return "unknown binary operator '{}'";
    }
    return "unknown error";
}

size_t levenshtein(const std::string& a, const std::string& b) {
    std::vector<std::vector<size_t>> d(a.size() + 1, std::vector<size_t>(b.size() + 1));
    for (size_t i = 0; i <= a.size(); i++) d[i][0] = i;
    for (size_t j = 0; j <= b.size(); j++) d[0][j] = j;
    for (size_t i = 1; i <= a.size(); i++) {
        for (size_t j = 1; j <= b.size(); j++) {
            size_t cost = a[i - 1] == b[j - 1] ? 0 : 1;
            d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost});
        }
    }
    return d[a.size()][b.size()];
}

}

std::string defaultMessage(Err code, const std::vector<std::string>& args) {
    std::string out;
    const char* tmpl = templateFor(code);
    size_t argIdx = 0;
    for (size_t i = 0; tmpl[i] != '\0'; i++) {
        if (tmpl[i] == '{' && tmpl[i + 1] == '}') {
            if (argIdx < args.size()) out += args[argIdx++];
            i++;
            continue;
        }
        out += tmpl[i];
    }
    return out;
}

std::string suggestClosest(const std::string& name, const std::vector<std::string>& candidates) {
    std::string best;
    size_t bestDist = std::string::npos;
    for (const auto& c : candidates) {
        size_t dist = levenshtein(name, c);
        if (dist < bestDist) {
            bestDist = dist;
            best = c;
        }
    }
    size_t threshold = std::max<size_t>(2, name.size() / 3);
    if (best.empty() || bestDist > threshold) return "";
    return " (did you mean '" + best + "'?)";
}

}