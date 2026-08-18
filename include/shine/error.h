#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include "shine/token.h"

namespace shine {

enum class Err {
    // lexer
    UnterminatedComment,
    UnterminatedString,
    BadEscapeSequence,
    UnexpectedChar,

    // parser
    ExpectedToken,
    ExpectedType,
    ExpectedExpression,
    UnexpectedEof,
    ArrayLengthZero,

    // codegen: types & variables
    UnknownType,
    VoidVariable,
    VariableRedeclared,
    UndeclaredIdentifier,
    ImmutableAssign,
    AddressOfNonVariable,
    DerefNonVariable,
    DerefNonPointer,
    TypeMismatch,
    ReturnTypeMismatch,
    VoidExpression,
    UnknownStruct,
    UnknownField,
    FieldAccessNonStruct,
    StructLiteralMismatch,
    ConditionType,
    EmptyArrayLiteral,
    IndexNonArray,
    IndexNonInt,
    ArrayIndexOutOfBounds,
    ArrayLiteralMismatch,

    // codegen: functions & calls
    UndeclaredFunction,
    ArgCountMismatch,

    // codegen: control flow
    BreakOutsideLoop,
    ContinueOutsideLoop,

    // codegen: builtins
    WriteArgCount,
    WriteArgType,
    UserInputArgCount,
    UserInputArgType,
    TerminalPauseArgCount,
    TerminalPauseArgType,

    // codegen: internal
    CodegenFailure,
    UnhandledStatement,
    UnhandledExpression,
    UnknownBinaryOp,
};

std::string defaultMessage(Err code, const std::vector<std::string>& args = {});

std::string suggestClosest(const std::string& name, const std::vector<std::string>& candidates);

class CompileError : public std::runtime_error {
public:
    CompileError(SourceLoc loc, std::string msg)
        : std::runtime_error(format(loc, msg)), loc_(loc) {}

    CompileError(SourceLoc loc, Err code, const std::vector<std::string>& args = {})
        : CompileError(loc, defaultMessage(code, args)) {}

    const SourceLoc& loc() const { return loc_; }

private:
    static std::string format(const SourceLoc& loc, const std::string& msg) {
        return loc.file + ":" + std::to_string(loc.line) + ":" +
               std::to_string(loc.col) + ": error: " + msg;
    }
    SourceLoc loc_;
};

}