#include "type_parser.h"
#include "../base/string_utils.h"

#include <unordered_map>

namespace clickhouse {

static const std::unordered_map<std::string, Type::Code> kTypeCode = {
    { "Int8",        Type::Int8 },
    { "Int16",       Type::Int16 },
    { "Int32",       Type::Int32 },
    { "Int64",       Type::Int64 },
    { "UInt8",       Type::UInt8 },
    { "UInt16",      Type::UInt16 },
    { "UInt32",      Type::UInt32 },
    { "UInt64",      Type::UInt64 },
    { "Float32",     Type::Float32 },
    { "Float64",     Type::Float64 },
    { "String",      Type::String },
    { "FixedString", Type::FixedString },
    { "DateTime",    Type::DateTime },
    { "Date",        Type::Date },
    { "Array",       Type::Array },
    { "Nullable",    Type::Nullable },
    { "Tuple",       Type::Tuple },
    { "Enum8",       Type::Enum8 },
    { "Enum16",      Type::Enum16 },
    { "UUID",        Type::UUID },
};

static Type::Code GetTypeCode(const std::string& name) {
    auto it = kTypeCode.find(name);
    if (it != kTypeCode.end()) {
        return it->second;
    }
    return Type::Void;
}

static TypeAst::Meta GetTypeMeta(const StringView& name) {
    if (name == "Array") {
        return TypeAst::Array;
    }

    if (name == "Null") {
        return TypeAst::Null;
    }

    if (name == "Nullable") {
        return TypeAst::Nullable;
    }

    if (name == "Tuple") {
        return TypeAst::Tuple;
    }

    if (name == "Enum8" || name == "Enum16") {
        return TypeAst::Enum;
    }

    return TypeAst::Terminal;
}


TypeParser::TypeParser(const StringView& name)
    : cur_(name.data())
    , end_(name.data() + name.size())
    , type_(nullptr)
{
}

TypeParser::~TypeParser() = default;

bool TypeParser::Parse(TypeAst* type) {
    type_ = type;
    open_elements_.push(type_);

    do {
        const Token& token = NextToken();

        switch (token.type) {
            case Token::Name:
                type_->meta = GetTypeMeta(token.value);
                type_->name = token.value.to_string();
                type_->code = GetTypeCode(type_->name);
                break;
            case Token::Number:
                type_->meta = TypeAst::Number;
                type_->value = std::stol(token.value.to_string());
                break;
            case Token::LPar:
                type_->elements.emplace_back(TypeAst());
                open_elements_.push(type_);
                type_ = &type_->elements.back();
                break;
            case Token::RPar:
                type_ = open_elements_.top();
                open_elements_.pop();
                break;
            case Token::Comma:
                type_ = open_elements_.top();
                open_elements_.pop();
                type_->elements.emplace_back(TypeAst());
                open_elements_.push(type_);
                type_ = &type_->elements.back();
                break;
            case Token::EOS:
                return true;
            case Token::Invalid:
                return false;
        }
    } while (true);
}

TypeParser::Token TypeParser::NextToken() {
    for (; cur_ < end_; ++cur_) {
        switch (*cur_) {
            case ' ':
            case '\n':
            case '\t':
            case '\0':
                continue;

            case '=':
            case '\'':
                continue;

            case '(':
                return Token{Token::LPar, StringView(cur_++, 1)};
            case ')':
                return Token{Token::RPar, StringView(cur_++, 1)};
            case ',':
                return Token{Token::Comma, StringView(cur_++, 1)};

            default: {
                const char* st = cur_;

                if (isalpha(*cur_) || *cur_ == '_') {
                    for (; cur_ < end_; ++cur_) {
                        if (!isalpha(*cur_) && !isdigit(*cur_) && *cur_ != '_') {
                            break;
                        }
                    }

                    return Token{Token::Name, StringView(st, cur_)};
                }

                if (isdigit(*cur_) || *cur_ == '-') {
                    for (++cur_; cur_ < end_; ++cur_) {
                        if (!isdigit(*cur_)) {
                            break;
                        }
                    }

                    return Token{Token::Number, StringView(st, cur_)};
                }

                return Token{Token::Invalid, StringView()};
            }
        }
    }

    return Token{Token::EOS, StringView()};
}


const TypeAst* ParseTypeName(const std::string& type_name) {
    // Cache for type_name.
    // Usually we won't have too many type names in the cache, so do not try to
    // limit cache size.
    static std::unordered_map<std::string, TypeAst> ast_cache;

    auto it = ast_cache.find(type_name);
    if (it != ast_cache.end()) {
        return &it->second;
    }

    auto& ast = ast_cache[type_name];
    if (TypeParser(type_name).Parse(&ast)) {
        return &ast;
    }
    ast_cache.erase(type_name);
    return nullptr;
}

}
