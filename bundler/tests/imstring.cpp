//
// Created by Duzhong Chen on 2020/4/9.
//

#include <cstdlib>
#include <cstring>
#include <IMString.h>
using namespace jetpack;

#include <gtest/gtest.h>

TEST(IMString, Create) {
    static const char* INIT = "short";

    IMString str1 = IMString::FromUTF8(INIT);
    EXPECT_EQ(str1.raw_str->ref_count, 1);

    IMString str2 = IMString::FromUTF8(INIT);
    EXPECT_EQ(str1.raw_str->ref_count, 2);

    EXPECT_EQ(str1.Size(), std::strlen(INIT));

    EXPECT_EQ(str1.raw_str, str2.raw_str);

    EXPECT_EQ(str1.ToString(), std::string(INIT));

    EXPECT_TRUE(str1 == str2);
}

TEST(IMString, EQ) {
    IMString str1 = IMString::FromUTF8("short1");
    IMString str2 = IMString::FromUTF8("short2");

    EXPECT_FALSE(str1 == str2);
}

TEST(IMString, RefCount) {
    IMString str1 = IMString::FromUTF8("short1");
    EXPECT_EQ(str1.raw_str->ref_count, 1);

    {
        IMString str2 = str1;
        EXPECT_EQ(str1.raw_str->ref_count, 2);
    }

    EXPECT_EQ(str1.raw_str->ref_count, 1);
}

TEST(IMString, Long) {
    std::string content;

    for (int i = 0; i < 100; i++) {
        content.push_back('1');
    }

    IMString str = IMString::FromUTF8(content);
    EXPECT_TRUE(str.raw_str->is_long_);

    EXPECT_NE(str.Hash(), 0);

    IMString str2 = IMString::FromUTF8(content);

    EXPECT_NE(str.raw_str, str2.raw_str);

    EXPECT_EQ(str2.Size(), content.size());
}
