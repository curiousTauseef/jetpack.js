//
// Created by Duzhong Chen on 2020/4/3.
//

#include <string>
#include "ConstantFolding.h"

namespace jetpack {

    inline Sp<Literal> MakeStringLiteral(const IMString& str) {
        auto lit = std::make_shared<Literal>();
        lit->ty = Literal::Ty::String;
        lit->str_ = str;
        lit->raw = IMString::FromUTF16(u"\"") + str + IMString::FromUTF16(u"\"");
        return lit;
    }

    inline Sp<Literal> MakeIntLiteral(std::int32_t tmp) {
        auto lit = std::make_shared<Literal>();
        lit->ty = Literal::Ty::Double;
        lit->str_ = IMString::FromUTF8(std::to_string(tmp));
        lit->raw = lit->str_;
        return lit;
    }

    inline bool IsValieResult(std::int64_t tmp) {
        return tmp >= std::numeric_limits<std::int32_t>::min() && tmp <= std::numeric_limits<std::int32_t>::max();
    }

    Sp<Expression> ContantFolding::TryBinaryExpression(const Sp<BinaryExpression> &binary) {
        if (binary->left->type == SyntaxNodeType::Literal
            && binary->right->type == SyntaxNodeType::Literal) {

            auto left_lit = std::dynamic_pointer_cast<Literal>(binary->left);
            auto right_lit = std::dynamic_pointer_cast<Literal>(binary->right);

            if (binary->operator_ == u"+" && left_lit->ty == Literal::Ty::String && right_lit->ty == Literal::Ty::String) {
                IMString result = left_lit->str_ + right_lit->str_;
                return MakeStringLiteral(result);
            } else if (left_lit->ty == Literal::Ty::Double && right_lit->ty == Literal::Ty::Double) {
                std::int32_t left_int = left_lit->str_.ToSimpleInt();
                if (left_int < 0) {
                    return binary;
                }
                std::int32_t right_int = right_lit->str_.ToSimpleInt();
                if (right_int < 0) {
                    return binary;
                }

                std::int64_t tmp_result = 0;
                if (binary->operator_ == u"+") {
                    tmp_result = left_int + right_int;
                } else if (binary->operator_ == u"-") {
                    tmp_result = left_int - right_int;
                } else {
                    return binary;
                }

                if (!IsValieResult(tmp_result)) {
                    return binary;
                }
                return MakeIntLiteral(static_cast<std::int32_t>(tmp_result));
            }
        }
        return binary;
    }

}
