//
// Created by Duzhong Chen on 2019/10/10.
//

#pragma once

#include "Token.h"
#include <IMString.h>
#include <utility>

struct Comment {
    bool multi_line_;
    jetpack::IMString value_;
    std::pair<std::uint32_t, std::uint32_t> range_;
    SourceLocation loc_;

};
