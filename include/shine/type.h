#pragma once
#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

namespace shine {

enum class TypeKind {
    Void,
    Int,
    Pointer,
    Struct,
    Array,
};

struct Type {
    TypeKind kind = TypeKind::Void;
    int bitWidth = 32;   // meaningful for Int
    bool isSigned = true; // meaningful for Int
    std::shared_ptr<Type> pointee; // meaningful for Pointer
    std::string structName; // meaningful for Struct
    std::shared_ptr<Type> element; // meaningful for Array
    int64_t length = 0;   // meaningful for Array

    bool isVoid() const { return kind == TypeKind::Void; }
    bool isInt() const { return kind == TypeKind::Int; }
    bool isPointer() const { return kind == TypeKind::Pointer; }
    bool isStruct() const { return kind == TypeKind::Struct; }
    bool isArray() const { return kind == TypeKind::Array; }

    bool equals(const Type& other) const {
        if (kind != other.kind) return false;
        if (kind == TypeKind::Int) return bitWidth == other.bitWidth && isSigned == other.isSigned;
        if (kind == TypeKind::Pointer) return pointee && other.pointee && pointee->equals(*other.pointee);
        if (kind == TypeKind::Struct) return structName == other.structName;
        if (kind == TypeKind::Array) return length == other.length && element && other.element && element->equals(*other.element);
        return true;
    }

    // Canonical name for diagnostics (not necessarily how it was spelled).
    std::string canonicalName() const {
        switch (kind) {
            case TypeKind::Void:    return "void";
            case TypeKind::Int:     return (isSigned ? "i" : "u") + std::to_string(bitWidth);
            case TypeKind::Pointer: return (pointee ? pointee->canonicalName() : "?") + "*";
            case TypeKind::Struct:  return structName;
            case TypeKind::Array:   return "[" + std::to_string(length) + "]" + (element ? element->canonicalName() : "?");
        }
        return "?";
    }

    static Type makeVoid() { return Type{TypeKind::Void, 0, false, nullptr, ""}; }
    static Type makeInt(int bitWidth, bool isSigned) { return Type{TypeKind::Int, bitWidth, isSigned, nullptr, ""}; }
    static Type makePointer(Type pointee) {
        Type t;
        t.kind = TypeKind::Pointer;
        t.pointee = std::make_shared<Type>(std::move(pointee));
        return t;
    }
    static Type makeStruct(std::string name) {
        Type t;
        t.kind = TypeKind::Struct;
        t.structName = std::move(name);
        return t;
    }
    static Type makeArray(Type element, int64_t length) {
        Type t;
        t.kind = TypeKind::Array;
        t.element = std::make_shared<Type>(std::move(element));
        t.length = length;
        return t;
    }
};

inline bool resolveTypeName(const std::string& name, Type& out) {
    if (name == "int")  { out = Type::makeInt(32, true); return true; }
    if (name == "void") { out = Type::makeVoid();         return true; }
    if (name.size() >= 2 && (name[0] == 'i' || name[0] == 'u')) {
        std::string digits = name.substr(1);
        bool allDigits = !digits.empty() &&
            std::all_of(digits.begin(), digits.end(), [](unsigned char c) { return std::isdigit(c); });
        if (allDigits) {
            int bits = std::stoi(digits);
            if (bits == 8 || bits == 16 || bits == 32 || bits == 64) {
                out = Type::makeInt(bits, name[0] == 'i');
                return true;
            }
        }
    }
    return false;
}

}