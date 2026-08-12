#pragma once
#include <string>

namespace shine {

enum class TypeKind {
    Void,
    Int,
    // Pointer will be added alongside fixed-width ints in a later milestone.
};

struct Type {
    TypeKind kind = TypeKind::Void;
    int bitWidth = 32;   // meaningful for Int
    bool isSigned = true; // meaningful for Int

    bool isVoid() const { return kind == TypeKind::Void; }
    bool isInt() const { return kind == TypeKind::Int; }

    bool equals(const Type& other) const {
        if (kind != other.kind) return false;
        if (kind == TypeKind::Int) return bitWidth == other.bitWidth && isSigned == other.isSigned;
        return true;
    }

    // Canonical name for diagnostics (not necessarily how it was spelled).
    std::string canonicalName() const {
        switch (kind) {
            case TypeKind::Void: return "void";
            case TypeKind::Int:  return (isSigned ? "i" : "u") + std::to_string(bitWidth);
        }
        return "?";
    }

    static Type makeVoid() { return Type{TypeKind::Void, 0, false}; }
    static Type makeInt(int bitWidth, bool isSigned) { return Type{TypeKind::Int, bitWidth, isSigned}; }
};

inline bool resolveTypeName(const std::string& name, Type& out) {
    if (name == "int")  { out = Type::makeInt(32, true); return true; }
    if (name == "void") { out = Type::makeVoid();         return true; }
    return false;
}

}