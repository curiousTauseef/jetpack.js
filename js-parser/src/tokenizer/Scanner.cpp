//
// Created by Duzhong Chen on 2019/9/3.
//

#include <iostream>
#include "Scanner.h"
#include "Utils.h"
#include "../parser/ErrorMessage.h"
#include "../parser/JsKeywordsContants.h"

namespace jetpack {

    using namespace std;

#define DO(EXPR) \
    if (!(EXPR)) return false;

    Scanner::Scanner(IMString source, std::shared_ptr<parser::ParseErrorHandler> error_handler):
            source_(std::move(source)), error_handler_(std::move(error_handler)) {

    }

    Scanner::ScannerState Scanner::SaveState() {
        ScannerState state {
                index_, line_number_, line_start_,
        };
        return state;
    }

    void Scanner::RestoreState(const Scanner::ScannerState &state) {
        index_ = state.index_;
        line_number_ = state.line_number_;
        line_start_ = state.line_start_;
    }

    void Scanner::ThrowUnexpectedToken() {
        ThrowUnexpectedToken(ParseMessages::UnexpectedTokenIllegal);
    }

    void Scanner::ThrowUnexpectedToken(const std::string &message) {
        throw error_handler_->CreateError(message, index_, line_number_, index_ - line_start_ + 1);
    }

    void Scanner::TolerateUnexpectedToken() {
        TolerateUnexpectedToken(ParseMessages::UnexpectedTokenIllegal);
    }

    void Scanner::TolerateUnexpectedToken(const std::string &message) {
        auto error = error_handler_->CreateError(message, index_, line_number_, index_ - line_start_ + 1);
        error_handler_->TolerateError(error);
    }

    std::vector<std::shared_ptr<Comment>> Scanner::SkipSingleLineComment(std::uint32_t offset) {
        std::vector<std::shared_ptr<Comment>> result;
        std::uint32_t start = 0;
        SourceLocation loc;

        start = index_ - offset;
        loc.start_.line_ = line_number_;
        loc.start_.column_ = index_ - line_start_ - offset;

        while (!IsEnd()) {
            char32_t ch = CodePointAt(index_);
            index_++;

            if (utils::IsLineTerminator(ch)) {
                loc.end_ = Position { line_number_, index_ - line_start_ - 1 };
                auto comment = new Comment {
                        false,
                        source_.Slice(start + offset, index_),
                        make_pair(start, index_ - 1),
                        loc
                };
                result.emplace_back(comment);
                if (ch == 13 && CodePointAt(index_) == 10) {
                    ++index_;
                }
                ++line_number_;
                line_start_ = index_;
                return result;
            }

        }

        loc.end_ = Position { line_number_, index_ - line_start_ };
        auto comment = new Comment {
                false,
                source_.Slice(start + offset, index_ ),
                make_pair(start, index_),
                loc,
        };
        result.emplace_back(comment);

        return result;
    }

    std::vector<std::shared_ptr<Comment>> Scanner::SkipMultiLineComment() {
        std::vector<std::shared_ptr<Comment>> result;
        std::uint32_t start = 0;
        SourceLocation loc;

        start = index_ - 2;
        loc.start_ = Position {
                line_number_,
                index_ - line_start_ - 2,
        };
        loc.end_ = Position { 0, 0 };

        while (!IsEnd()) {
            char32_t ch = CodePointAt(index_);
            if (utils::IsLineTerminator(ch)) {
                if (ch == 0x0D && CodePointAt(index_ + 1) == 0x0A) {
                    ++index_;
                }
                ++line_number_;
                ++index_;
                line_start_ = index_;
            } else if (ch == 0x2A) {
                if (CodePointAt(index_ + 1) == 0x2F) {
                    index_ += 2;
                    loc.end_ = Position {
                            line_number_,
                            index_ - line_start_,
                    };
                    auto comment = new Comment {
                            true,
                            source_.Slice(start + 2, index_ - 2),
                            make_pair(start, index_),
                            loc,
                    };
                    result.emplace_back(comment);
                    return result;
                }

                ++index_;
            } else {
                ++index_;
            }
        }

        loc.end_ = Position {
                line_number_,
                index_ - line_start_,
        };
        auto comment = new Comment {
                true,
                source_.Slice(start + 2, index_),
                make_pair(start, index_),
                loc,
        };
        result.emplace_back(comment);

        TolerateUnexpectedToken();
        return result;
    }

    void Scanner::ScanComments(std::vector<std::shared_ptr<Comment>> &result) {
        bool start = index_ == 0;

        while (!IsEnd()) {
            char32_t ch = CodePointAt(index_);

            if (utils::IsWhiteSpace(ch)) {
                ++index_;
            } else if (utils::IsLineTerminator(ch)) {
                ++index_;

                if (ch == 0x0D && CodePointAt(index_) == 0x0A) {
                    ++index_;
                }
                ++line_number_;
                line_start_ = index_;
                start = true;
            } else if (ch == 0x2F) {
                ch = CodePointAt(index_ + 1);
                if (ch == 0x2F) {
                    index_ += 2;
                    auto comments = SkipSingleLineComment(2);
                    result.insert(result.end(), comments.begin(), comments.end());
                    start = true;
                } else if (ch == 0x2A) {  // U+002A is '*'
                    index_ += 2;
                    auto comments = SkipMultiLineComment();
                    result.insert(result.end(), comments.begin(), comments.end());
                } else if (start && ch == 0x2D) { // U+002D is '-'
                    // U+003E is '>'
                    if ((CodePointAt(index_ + 1) == 0x2D) && (CodePointAt(index_ + 2) == 0x3E)) {
                        // '-->' is a single-line comment
                        index_ += 3;
                        auto comments = SkipSingleLineComment(3);
                        result.insert(result.end(), comments.begin(), comments.end());
                    } else {
                        break;
                    }
                } else if (ch == 0x3C && !is_module_) { // U+003C is '<'
                    if (source_.Slice(index_ + 1, index_ + 4) == u"!--") {
                        index_ += 4; // `<!--`
                        auto comments = SkipSingleLineComment(4);
                        result.insert(result.end(), comments.begin(), comments.end());
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            } else {
                break;
            }

        }
    }

    char32_t Scanner::CodePointAt(std::uint32_t index, std::uint32_t* size) const {
        char32_t cp = source_[index];

        if (cp >= 0xD800 && cp <= 0xDBFF) {
            char32_t second = source_[index + 1];
            if (second >= 0xDC00 && second <= 0xDFFF) {
                char32_t first = cp;
                cp = (first - 0xD800) * 0x400 + second - 0xDC00 + 0x10000;
                if (size) {
                    *size = 2;
                }
            }
        }

        if (size) {
            *size = 1;
        }

        return cp;
    }

#define MAYBE_WORD(WORD) \
    if (str_ == WORD) return true;

    bool Scanner::IsFutureReservedWord(JsTokenType t) {
        return t == JsTokenType::K_Enum ||
               t == JsTokenType::K_Export ||
               t == JsTokenType::K_Import ||
               t == JsTokenType::K_Super;
    }

    JsTokenType Scanner::IsStrictModeReservedWord(const IMString &str_) {
        if (str_ == u"implements") return JsTokenType::KS_Implements;
        if (str_ == u"interface") return JsTokenType::KS_Interface;
        if (str_ == u"package") return JsTokenType::KS_Package;
        if (str_ == u"private") return JsTokenType::KS_Private;
        if (str_ == u"protected") return JsTokenType::KS_Protected;
        if (str_ == u"public") return JsTokenType::KS_Public;
        if (str_ == u"static") return JsTokenType::KS_Static;
        if (str_ == u"yield") return JsTokenType::K_Yield;
        if (str_ == u"let") return JsTokenType::K_Let;
        return JsTokenType::Invalid;
    }

    bool Scanner::IsRestrictedWord(const IMString &str_) {
        MAYBE_WORD(u"eval")
        MAYBE_WORD(u"arguments")
        return false;
    }

    JsTokenType Scanner::ToKeyword(const IMString &str_) {
        switch (str_.Size()) {
            case 2:
                if (str_ == S_IF) return JsTokenType::K_If;
                if (str_ == S_IN) return JsTokenType::K_In;
                if (str_ == S_DO) return JsTokenType::K_Do;
                return JsTokenType::Invalid;

            case 3:
                if (str_ == u"var") return JsTokenType::K_Var;
                if (str_ == u"for") return JsTokenType::K_For;
                if (str_ == u"new") return JsTokenType::K_New;
                if (str_ == u"try") return JsTokenType::K_Try;
                if (str_ == u"let") return JsTokenType::K_Let;
                return JsTokenType::Invalid;

            case 4:
                if (str_ == u"this") return JsTokenType::K_This;
                if (str_ == u"else") return JsTokenType::K_Else;
                if (str_ == u"case") return JsTokenType::K_Case;
                if (str_ == u"void") return JsTokenType::K_Void;
                if (str_ == u"with") return JsTokenType::K_With;
                if (str_ == u"enum") return JsTokenType::K_Enum;
                return JsTokenType::Invalid;

            case 5:
                if (str_ == u"while") return JsTokenType::K_While;
                if (str_ == u"break") return JsTokenType::K_Break;
                if (str_ == u"catch") return JsTokenType::K_Catch;
                if (str_ == u"throw") return JsTokenType::K_Throw;
                if (str_ == u"const") return JsTokenType::K_Const;
                if (str_ == u"yield") return JsTokenType::K_Yield;
                if (str_ == u"class") return JsTokenType::K_Class;
                if (str_ == u"super") return JsTokenType::K_Super;
                return JsTokenType::Invalid;

            case 6:
                if (str_ == u"return") return JsTokenType::K_Return;
                if (str_ == u"typeof") return JsTokenType::K_Typeof;
                if (str_ == u"delete") return JsTokenType::K_Delete;
                if (str_ == u"switch") return JsTokenType::K_Switch;
                if (str_ == u"export") return JsTokenType::K_Export;
                if (str_ == u"import") return JsTokenType::K_Import;
                return JsTokenType::Invalid;

            case 7:
                if (str_ == u"default") return JsTokenType::K_Default;
                if (str_ == u"finally") return JsTokenType::K_Finally;
                if (str_ == u"extends") return JsTokenType::K_Extends;
                return JsTokenType::Invalid;

            case 8:
                if (str_ == u"function") return JsTokenType::K_Function;
                if (str_ == u"continue") return JsTokenType::K_Continue;
                if (str_ == u"debugger") return JsTokenType::K_Debugger;
                return JsTokenType::Invalid;

            case 10:
                if (str_ == u"instanceof") return JsTokenType::K_Instanceof;
                return JsTokenType::Invalid;

            default:
                return JsTokenType::Invalid;
        }
    }

#undef MAYBE_WORD

    inline uint32_t HexValue(char32_t ch) {
        if (ch >= 'A' && ch <= 'F') {
            ch = 'a' + (ch - 'A');
        }
        return string("0123456789abcdef").find(ch);
    }

    bool Scanner::ScanHexEscape(char16_t ch, char32_t& code) {
        std::uint32_t len = (ch == 'u') ? 4 : 2;

        for (std::uint32_t i = 0; i < len; ++i) {
            if (!IsEnd() && utils::IsHexDigit(CodePointAt(index_))) {
                code = code * 16 + HexValue(CodePointAt(index_++));
            } else {
                return false;
            }
        }

        return true;
    }

    char32_t Scanner::ScanUnicodeCodePointEscape() {
        char16_t ch = source_[index_];
        char32_t code = 0;

        if (ch == '}') {
            ThrowUnexpectedToken();
        }

        while (!IsEnd()) {
            ch = source_[index_++];
            if (!utils::IsHexDigit(ch)) {
                break;
            }
            code = code * 16 + (ch - '0');
        }

        if (code > 0x10FFFF || ch != '}') {
            ThrowUnexpectedToken();
        }

        return code;
    }

    IMString Scanner::GetIdentifier() {
        std::uint32_t start = index_++;

        while (!IsEnd()) {
            auto ch = source_[index_];
            if (ch == 0x5C) {
                // Blackslash (U+005C) marks Unicode escape sequence.
                index_ = start;
                return GetComplexIdentifier();
            } else if (ch >= 0xD800 && ch < 0xDFFF) {
                // Need to handle surrogate pairs.
                index_ = start;
                return GetComplexIdentifier();
            }
            if (utils::IsIdentifierPart(ch)) {
                ++index_;
            } else {
                break;
            }
        }

        return source_.Slice(start, index_);
    }

    IMString Scanner::GetComplexIdentifier() {
        std::uint32_t cp_size_;
        auto cp = CodePointAt(index_, &cp_size_);
        index_ += cp_size_;

        std::u16string result;

        // '\u' (U+005C, U+0075) denotes an escaped character.
        char32_t ch = 0;
        if (cp == 0x5C) {
            if (source_[index_] != 0x75) {
                ThrowUnexpectedToken();
            }
            ++index_;
            if (source_[index_] == '{') {
                ++index_;
                ch = ScanUnicodeCodePointEscape();
            } else {
                if (!ScanHexEscape('u', ch) || ch == '\\' || !utils::IsIdentifierStart(ch)) {
                    ThrowUnexpectedToken();
                }
            }
            result.push_back(ch);
        }

        while (!IsEnd()) {
            cp = CodePointAt(index_);
            if (!utils::IsIdentifierPart(cp)) {
                break;
            }

            std::u16string ch_ = utils::FromCodePoint(cp);

            result.insert(result.end(), ch_.begin(), ch_.end());

            std::cout << index_ << std::endl;
            index_ += ch_.size();

            // '\u' (U+005C, U+0075) denotes an escaped character.
            if (cp == 0x5C) {
                result = result.substr(0, result.size() - 1);
                if (source_[index_] != 0x75) {
                    ThrowUnexpectedToken();
                }
                ++index_;
                if (source_[index_] == '{') {
                    ++index_;
                    ch = ScanUnicodeCodePointEscape();
                } else {
                    if (!ScanHexEscape('u', ch) || ch == '\\' || !utils::IsIdentifierPart(ch)) {
                        ThrowUnexpectedToken();
                    }
                }
                result.push_back(ch);
            }
        }

        return IMString::FromUTF16(result.c_str(), result.size());
    }

    bool Scanner::OctalToDecimal(char16_t ch, std::uint32_t &result) {
        bool octal = (ch != '0');
        result = ch - '0';

        if (!IsEnd() && utils::IsOctalDigit(source_[index_])) {
            octal = true;
            result = result * 8 + (source_[index_] - '0');

            // 3 digits are only allowed when string starts
            // with 0, 1, 2, 3
            if (ch - '0' && !IsEnd() && utils::IsOctalDigit(source_[index_])) {
                result = result * 8 + (source_[index_] - '0');
            }
        }

        return octal;
    }

    Token Scanner::ScanIdentifier() {
        auto start = index_;
        Token tok;

        IMString id;
        if (source_[start] == 0x5C) {
            id = GetComplexIdentifier();
        } else {
            id = GetIdentifier();
        }

        if (id.Size() == 1) {
            tok.type_ = JsTokenType::Identifier;
        } else if ((tok.type_ = ToKeyword(id)) != JsTokenType::Invalid) {
            // nothing
        } else if (id == u"null") {
            tok.type_ = JsTokenType::NullLiteral;
        } else if (id == u"true" || id == u"false") {
            tok.type_ = JsTokenType::BooleanLiteral;
        } else {
            tok.type_ = JsTokenType::Identifier;
        }

        if (tok.type_ != JsTokenType::Identifier && (start + id.Size() != index_)) {
            auto restore = index_;
            index_ = start;
            TolerateUnexpectedToken(ParseMessages::InvalidEscapedReservedWord);
            index_ = restore;
        }

        tok.value_ = move(id);
        tok.range_ = make_pair(start, index_);
        tok.line_number_ = line_number_;
        tok.line_start_ = line_start_;

        return tok;
    }

    Token Scanner::ScanPunctuator() {
        auto start = index_;

        char16_t ch = source_[index_];
        IMString str;

        JsTokenType t;
        switch (ch) {
            case u'(':
                t = JsTokenType::LeftParen;
                if (ch == u'{') {
                    curly_stack_.push(IMString::FromUTF16(u'{'));
                }
                ++index_;
                break;

            case u'{':
                t = JsTokenType::LeftBracket;
                if (ch == u'{') {
                    curly_stack_.push(IMString::FromUTF16(u'{'));
                }
                ++index_;
                break;

            case u'.':
                t = JsTokenType::Dot;
                ++index_;
                if (source_[index_] == u'.' && source_[index_ + 1] == u'.') {
                    // Spread operator: ...
                    t = JsTokenType::Spread;
                    index_ += 2;
                    str = IMString::FromUTF16(u"...");
                }
                break;

            case u'}':
                t = JsTokenType::RightBracket;
                ++index_;
                if (!curly_stack_.empty()) {
                    curly_stack_.pop();
                }
                break;

            case u')':
                t = JsTokenType::RightParen;
                ++index_;
                break;

            case u';':
                t = JsTokenType::Semicolon;
                ++index_;
                break;

            case u',':
                t = JsTokenType::Comma;
                ++index_;
                break;

            case u'[':
                t = JsTokenType::LeftBrace;
                ++index_;
                break;

            case u']':
                t = JsTokenType::RightBrace;
                ++index_;
                break;

            case u':':
                t = JsTokenType::Colon;
                ++index_;
                break;

            case u'?':
                t = JsTokenType::Ask;
                ++index_;
                break;

            case u'~':
                t = JsTokenType::Wave;
                ++index_;
                break;

            case u'<':
                ++index_;
                if (source_[index_] == u'<') { // <<
                    index_ ++;
                    if (source_[index_] == u'=') { // <<=
                        index_ ++;
                        t = JsTokenType::LeftShiftAssign;
                    } else {
                        t = JsTokenType::LeftShift;
                    }
                } else if (source_[index_] == u'=') { // <=
                    index_++;
                    t = JsTokenType::LessEqual;
                } else {
                    t = JsTokenType::LessThan;
                }
                break;

            case u'>':
                ++index_;
                if (source_[index_] == u'>') { // >>
                    index_++;
                    if (source_[index_] == u'>') { // >>>
                        index_++;
                        if (source_[index_] == u'=') {
                            index_++;
                            t = JsTokenType::ZeroFillRightShiftAssign;
                        } else {
                            t = JsTokenType::ZeroFillRightShift;
                        }
                    } else {
                        t = JsTokenType::RightShift;
                    }
                } else if (source_[index_] == u'=') {
                    index_++;
                    t = JsTokenType::GreaterEqual;
                } else {
                    t = JsTokenType::GreaterThan;
                }
                break;

            case u'=':
                ++index_;
                if (source_[index_] == u'=') {
                    ++index_;
                    if (source_[index_] == u'=') {
                        ++index_;
                        t = JsTokenType::StrictEqual;
                    } else {
                        t = JsTokenType::Equal;
                    }
                } else if (source_[index_] == u'>') {
                    ++index_;
                    t = JsTokenType::Arrow;
                } else {
                    t = JsTokenType::Assign;
                }
                break;

            case u'!':
                ++index_;
                if (source_[index_] == u'=') {
                    ++index_;
                    if (source_[index_] == u'=') {
                        ++index_;
                        t = JsTokenType::StrictNotEqual;
                    } else {
                        t = JsTokenType::NotEqual;
                    }
                } else {
                    t = JsTokenType::Not;
                }
                break;

            case u'+':
                ++index_;
                if (source_[index_] == u'=') {
                    ++index_;
                    t = JsTokenType::PlusAssign;
                } else if (source_[index_] == u'+') {
                    ++index_;
                    t = JsTokenType::Increase;
                } else {
                    t = JsTokenType::Plus;
                }
                break;

            case u'-':
                ++index_;
                if (source_[index_] == u'=') {
                    ++index_;
                    t = JsTokenType::MinusAssign;
                } else if (source_[index_] == u'-') {
                    ++index_;
                    t = JsTokenType::Decrease;
                } else {
                    t = JsTokenType::Minus;
                }
                break;

            case u'*':
                ++index_;
                if (source_[index_] == u'=') {
                    ++index_;
                    t = JsTokenType::MulAssign;
                } else if (source_[index_] == u'*') {
                    ++index_;
                    if (source_[index_] == u'=') {
                        ++index_;
                        t = JsTokenType::PowAssign;
                    } else {
                        t = JsTokenType::Pow;
                    }
                } else {
                    t = JsTokenType::Mul;
                }
                break;

            case u'%':
                ++index_;
                if (source_[index_] == u'=') {
                    ++index_;
                    t = JsTokenType::ModAssign;
                } else {
                    t = JsTokenType::Mod;
                }
                break;

            case u'/':
                ++index_;
                if (source_[index_] == u'=') {
                    ++index_;
                    t = JsTokenType::DivAssign;
                } else {
                    t = JsTokenType::Div;
                }
                break;

            case u'^':
                ++index_;
                if (source_[index_] == u'=') {
                    ++index_;
                    t = JsTokenType::BitXorAssign;
                } else {
                    t = JsTokenType::Xor;
                }
                break;

            case u'&':
                ++index_;
                if (source_[index_] == u'&') {
                    ++index_;
                    t = JsTokenType::And;
                } else if (source_[index_] == u'=') {
                    ++index_;
                    t = JsTokenType::BitAndAssign;
                } else {
                    t = JsTokenType::BitAnd;
                }
                break;

            case u'|':
                ++index_;
                if (source_[index_] == u'|') {
                    ++index_;
                    t = JsTokenType::Or;
                } else if (source_[index_] == u'=') {
                    ++index_;
                    t = JsTokenType::BitOrAssign;
                } else {
                    t = JsTokenType::BitOr;
                }
                break;

        }

        if (index_ == start) {
            ThrowUnexpectedToken();
        }

        str = source_.Slice(start, index_);

        return {
                t,
                str,
                SourceLocation(),
                line_number_,
                line_start_,
                make_pair(start, index_)
        };
    }

    Token Scanner::ScanHexLiteral(std::uint32_t start) {
        std::u16string num;
        Token tok;

        while (!IsEnd()) {
            if (!utils::IsHexDigit(CharAt(index_))) {
                break;
            }
            num.push_back(CharAt(index_++));
        }

        if (num.size() == 0) {
            ThrowUnexpectedToken();
        }

        if (utils::IsIdentifierStart(CharAt(index_))) {
            ThrowUnexpectedToken();
        }

        tok.type_ = JsTokenType::NumericLiteral;
        tok.value_ = IMString::FromUTF16(u"0x") + IMString::FromUTF16(num.c_str(), num.size());
        tok.line_start_ = line_start_;
        tok.line_number_ = line_number_;
        tok.range_ = make_pair(start, index_);

        return tok;
    }

    Token Scanner::ScanBinaryLiteral(std::uint32_t start) {
        std::u16string num;
        char16_t ch;

        while (!IsEnd()) {
            ch = source_[index_];
            if (ch != u'0' && ch != u'1') {
                break;
            }
            num.push_back(source_[index_++]);
        }

        if (num.empty()) {
            // only 0b or 0B
            ThrowUnexpectedToken();
        }

        if (!IsEnd()) {
            ch = source_[index_++];
            /* istanbul ignore else */
            if (utils::IsIdentifierStart(ch) || utils::IsDecimalDigit(ch)) {
                ThrowUnexpectedToken();
            }
        }

        return {
                JsTokenType::NumericLiteral,
                IMString::FromUTF16(num.c_str(), num.size()),
                SourceLocation(),
                line_number_,
                line_start_,
                make_pair(start, index_),
        };
    }

    Token Scanner::ScanOctalLiteral(char16_t prefix, std::uint32_t start) {
        std::u16string num;
        bool octal = false;

        if (utils::IsOctalDigit(prefix)) {
            octal = true;
            num = u'0' + source_[index_++];
        } else {
            ++index_;
        }

        while (!IsEnd()) {
            if (!utils::IsOctalDigit(source_[index_])) {
                break;
            }
            num.push_back(source_[index_++]);
        }

        if (!octal && num.size() == 0) {
            // only 0o or 0O
            ThrowUnexpectedToken();
        }

        if (utils::IsIdentifierStart(source_[index_]) || utils::IsDecimalDigit(source_[index_])) {
            ThrowUnexpectedToken();
        }

        return {
                JsTokenType::NumericLiteral,
                IMString::FromUTF16(num.c_str(), num.size()),
                SourceLocation(),
                line_number_,
                line_start_,
                make_pair(start, index_),
        };
    }

    bool Scanner::IsImplicitOctalLiteral() {
        // Implicit octal, unless there is a non-octal digit.
        // (Annex B.1.1 on Numeric Literals)
        for (std::uint32_t i = index_ + 1; i < Length(); ++i) {
            char16_t ch = source_[i];
            if (ch == '8' || ch == '9') {
                return false;
            }
            if (!utils::IsOctalDigit(ch)) {
                return true;
            }
        }

        return true;
    }

    Token Scanner::ScanNumericLiteral() {
        auto start = index_;
        char16_t ch = source_[start];
        if (!(utils::IsDecimalDigit(ch) || (ch == u'.'))) {
            auto err = error_handler_->CreateError("Numeric literal must start with a decimal digit or a decimal point", index_, line_number_, index_ - line_start_);
            throw std::move(err);
        }

        std::u16string num;
        if (ch != u'.') {
            num.push_back(source_[index_++]);
            ch = source_[index_];

            // Hex number starts with '0x'.
            // Octal number starts with '0'.
            // Octal number in ES6 starts with '0o'.
            // Binary number in ES6 starts with '0b'.
            if (num[0] == u'0') {
                if (ch == u'x' || ch == u'X') {
                    ++index_;
                    return ScanHexLiteral(start);
                }
                if (ch == u'b' || ch == u'B') {
                    ++index_;
                    return ScanBinaryLiteral(start);
                }
                if (ch == u'o' || ch == u'O') {
                    return ScanOctalLiteral(ch, start);
                }

                if (ch && utils::IsOctalDigit(ch)) {
                    if (IsImplicitOctalLiteral()) {
                        return ScanOctalLiteral(ch, start);
                    }
                }
            }

            while (utils::IsDecimalDigit(source_[index_])) {
                num.push_back(source_[index_++]);
            }
            ch = source_[index_];
        }

        if (ch == u'.') {
            num.push_back(source_[index_++]);
            while (utils::IsDecimalDigit(source_[index_])) {
                num.push_back(source_[index_++]);
            }
            ch = source_[index_];
        }

        if (ch == 'e' || ch == 'E') {
            num.push_back(CharAt(index_++));

            ch = CharAt(index_);
            if (ch == '+' || ch == '-') {
                num.push_back(CharAt(index_++));
            }
            if (utils::IsDecimalDigit(CharAt(index_))) {
                while (utils::IsDecimalDigit(CharAt(index_))) {
                    num.push_back(CharAt(index_++));
                }
            } else {
                ThrowUnexpectedToken();
            }
        }

        if (utils::IsIdentifierStart(CharAt(index_))) {
            ThrowUnexpectedToken();
        }

        return {
                JsTokenType::NumericLiteral,
                IMString::FromUTF16(num.c_str(), num.size()),
                SourceLocation(),
                line_number_,
                line_start_,
                make_pair(start, index_),
        };
    }

    Token Scanner::ScanStringLiteral() {
        auto start = index_;
        char16_t quote = CharAt(start);

        if (!(quote == '\'' || quote == '"')) {
            auto err = error_handler_->CreateError("String literal must starts with a quote", index_, line_number_, index_ - line_start_);
            throw std::move(err);
        }

        ++index_;
        bool octal = false;
        std::u16string str;

        while (!IsEnd()) {
            char16_t ch = CharAt(index_++);

            if (ch == quote) {
                quote = 0;
                break;
            } else if (ch == '\\') {
                ch = CharAt(index_++);
                if (!ch || !utils::IsLineTerminator(ch)) {
                    char32_t unescaped = 0;
                    switch (ch) {
                        case u'u':
                            if (CharAt(index_) == u'{') {
                                ++index_;
                                char32_t tmp = ScanUnicodeCodePointEscape();

                                utils::AddU32ToUtf16(str, tmp);
                            } else {
                                if (!ScanHexEscape(ch, unescaped)) {
                                    ThrowUnexpectedToken();
                                }

                                utils::AddU32ToUtf16(str, unescaped);
                            }
                            break;

                        case u'x':
                            if (!ScanHexEscape(ch, unescaped)) {
                                ThrowUnexpectedToken();
                            }
                            utils::AddU32ToUtf16(str, unescaped);
                            break;

                        case u'n':
                            str += '\n';
                            break;
                        case u'r':
                            str += '\r';
                            break;
                        case u't':
                            str += '\t';
                            break;
                        case u'b':
                            str += '\b';
                            break;
                        case u'f':
                            str += '\f';
                            break;
                        case u'v':
                            str += '\x0B';
                            break;
                        case u'8':
                        case u'9':
                            str += ch;
                            TolerateUnexpectedToken();
                            break;

                        default:
                            if (ch && utils::IsOctalDigit(ch)) {
                                std::uint32_t octToDec;
                                octal = OctalToDecimal(ch, octToDec);

                                utils::AddU32ToUtf16(str, octToDec);
                            } else {
                                str += ch;
                            }
                            break;
                    }
                } else {
                    ++line_number_;
                    if (ch == '\r' && CharAt(index_) == '\n') {
                        ++index_;
                    }
                    line_start_ = index_;
                }
            } else if (utils::IsLineTerminator(ch)) {
                break;
            } else {
                str += ch;
            }
        }

        if (quote != 0) {
            index_ = start;
            ThrowUnexpectedToken();
        }

        Token tok;
        tok.type_ = JsTokenType::StringLiteral;
        tok.value_ = IMString::FromUTF16(str.c_str(), str.size());
        tok.octal_ = octal;
        tok.line_number_ = line_number_;
        tok.line_start_ = line_start_;
        tok.range_ = make_pair(start, index_);

        return tok;
    }

    Token Scanner::ScanTemplate() {
        std::u16string cooked;
        bool terminated = false;
        std::uint32_t start = index_;

        bool head = (source_[start] == '`');
        bool tail = false;
        std::uint32_t rawOffset = 2;

        ++index_;

        while (!IsEnd()) {
            char16_t ch = source_[index_++];
            if (ch == u'`') {
                rawOffset = 1;
                tail = true;
                terminated = true;
                break;
            } else if (ch == u'$') {
                if (source_[index_]== u'{') {
                    curly_stack_.push(IMString::FromUTF16(u"${"));
                    ++index_;
                    terminated = true;
                    break;
                }
                cooked.push_back(ch);
            } else if (ch == u'\\') {
                ch = source_[index_++];
                if (!utils::IsLineTerminator(ch)) {
                    switch (ch) {
                        case u'n':
                            cooked.push_back('\n');
                            break;
                        case u'r':
                            cooked.push_back('\r');
                            break;
                        case u't':
                            cooked.push_back('\t');
                            break;
                        case u'u':
                            if (source_[index_] == u'{') {
                                ++index_;
                                char16_t tmp = ScanUnicodeCodePointEscape();
                                cooked.push_back(tmp);
                            } else {
                                auto restore = index_;
                                char32_t unescapedChar;
                                if (ScanHexEscape(ch, unescapedChar)) {
                                    cooked.push_back(unescapedChar);
                                } else {
                                    index_= restore;
                                    cooked.push_back(ch);
                                }
                            }
                            break;
                        case u'x':
                            char32_t unescaped;
                            if (!ScanHexEscape(ch, unescaped)) {
                                ThrowUnexpectedToken();
                            }
                            cooked.push_back(unescaped);
                            break;
                        case u'b':
                            cooked.push_back(u'\b');
                            break;
                        case u'f':
                            cooked.push_back(u'\f');
                            break;
                        case u'v':
                            cooked.push_back(u'\v');
                            break;

                        default:
                            if (ch == u'0') {
                                if (utils::IsDecimalDigit(CharAt(index_))) {
                                    // Illegal: \01 \02 and so on
                                    ThrowUnexpectedToken();
                                }
                                cooked.push_back(u'\0');
                            } else if (utils::IsOctalDigit(ch)) {
                                // Illegal: \1 \2
                                ThrowUnexpectedToken();
                            } else {
                                cooked.push_back(ch);
                            }
                            break;
                    }
                } else {
                    ++line_number_;
                    if (ch == '\r' && CharAt(index_) == '\n') {
                        ++index_;
                    }
                    line_start_ = index_;
                }
            } else if (utils::IsLineTerminator(ch)) {
                ++line_number_;
                if (ch == '\r' && CharAt(index_) == '\n') {
                    ++index_;
                }
                line_start_  = index_;
                cooked.push_back('\n');
            } else {
                cooked.push_back(ch);
            }
        }

        if (!terminated) {
            ThrowUnexpectedToken();
        }

        if (!head) {
            curly_stack_.pop();
        }

        Token tok;
        tok.type_ = JsTokenType::Template;
        tok.value_ = source_.Slice(start + 1, index_ - rawOffset);
        tok.line_number_ = line_number_;
        tok.line_start_ = line_start_;
        tok.range_ = make_pair(start, index_);
        tok.cooked_ = IMString::FromUTF16(cooked.c_str(), cooked.size());
        tok.head_ = head;
        tok.tail_ = tail;

        return tok;
    }

    IMString Scanner::ScanRegExpBody() {
        char16_t ch = source_[index_];
        if (ch != u'/') {
            ThrowUnexpectedToken("Regular expression literal must start with a slash");
        }

        std::u16string str;
        str.push_back(source_[index_++]);
        bool class_marker = false;
        bool terminated = false;

        while (!IsEnd()) {
            ch = source_[index_++];
            str.push_back(ch);
            if (ch == u'\\') {
                ch = source_[index_++];
                if (utils::IsLineTerminator(ch)) {
                    ThrowUnexpectedToken(ParseMessages::UnterminatedRegExp);
                }
                str.push_back(ch);
            } else if (utils::IsLineTerminator(ch)) {
                ThrowUnexpectedToken(ParseMessages::UnterminatedRegExp);
            } else if (class_marker) {
                if (ch == u']') {
                    class_marker = false;
                }
            } else {
                if (ch == u'/') {
                    terminated = true;
                    break;
                } else if (ch == u'[') {
                    class_marker = true;
                }
            }
        }

        if (!terminated) {
            ThrowUnexpectedToken(ParseMessages::UnterminatedRegExp);
        }

        return IMString::FromUTF16(str.substr(1, str.size() - 2));
    }

    IMString Scanner::ScanRegExpFlags() {
        std::u16string str;
        std::u16string flags;
        while (!IsEnd()) {
            char16_t ch = source_[index_];
            if (!utils::IsIdentifierPart(ch)) {
                break;
            }

            ++index_;
            if (ch == u'\\' && !IsEnd()) {
                ch = source_[index_];
                if (ch == u'u') {
                    ++index_;
                    auto restore = index_;
                    char32_t char_;
                    if (ScanHexEscape(u'u', char_)) {
                        flags.push_back(char_);
                        for (str += u"\\u"; restore < index_; ++ restore) {
                            str.push_back(source_[restore]);
                        }
                    } else {
                        index_ = restore;
                        flags += u"u";
                        str += u"\\u";
                    }
                    TolerateUnexpectedToken();
                } else {
                    str += u"\\";
                    TolerateUnexpectedToken();
                }
            } else {
                flags.push_back(ch);
                str.push_back(ch);
            }
        }

        return IMString::FromUTF16(flags);
    }

    Token Scanner::ScanRegExp() {
//        auto start = index_;

        auto pattern = ScanRegExpBody();
        auto flags = ScanRegExpFlags();

        Token token;
        token.type_ = JsTokenType::RegularExpression;
        token.line_number_ = line_number_;
        token.line_start_ = line_start_;
        token.value_ = IMString::FromUTF16(u"/") + pattern + IMString::FromUTF16(u"/") + flags;

        return token;
    }

    Token Scanner::Lex() {
        if (IsEnd()) {
            Token tok;
            tok.type_ = JsTokenType::EOF_;
            tok.line_number_ = line_number_;
            tok.line_start_ = line_start_;
            tok.range_ = make_pair(index_, index_);
            return tok;
        }

        char16_t cp = source_[index_];

        if (utils::IsIdentifierStart(cp)) {
            return ScanIdentifier();
        }
        // Very common: ( and ) and ;
        if (cp == 0x28 || cp == 0x29 || cp == 0x3B) {
            return ScanPunctuator();
        }

        // String literal starts with single quote (U+0027) or double quote (U+0022).
        if (cp == 0x27 || cp == 0x22) {
            return ScanStringLiteral();
        }

        // Dot (.) U+002E can also start a floating-point number, hence the need
        // to check the next character.
        if (cp == 0x2E) {
            if (utils::IsDecimalDigit(source_[index_ + 1])) {
                return ScanNumericLiteral();
            }
            return ScanPunctuator();
        }

        if (utils::IsDecimalDigit(cp)) {
            return ScanNumericLiteral();
        }

        // Template literals start with ` (U+0060) for template head
        // or } (U+007D) for template middle or template tail.
        if (cp == 0x60 || (cp == 0x7D && !curly_stack_.empty() && curly_stack_.top() == u"${")) {
            return ScanTemplate();
        }

        // Possible identifier start in a surrogate pair.
        if (cp >= 0xD800 && cp < 0xDFFF) {
            if (utils::IsIdentifierStart(CodePointAt(index_))) {
                return ScanIdentifier();
            }
        }

        return ScanPunctuator();
    }

}
