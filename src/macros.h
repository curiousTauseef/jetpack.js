//
// Created by Duzhong Chen on 2019/9/4.
//

#pragma once

#define DO(EXPR) \
    if (!(EXPR)) return false;

#define U(EXP) \
    utils::To_UTF16(EXP)