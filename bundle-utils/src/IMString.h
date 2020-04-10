//
// Created by Duzhong Chen on 2020/4/9.
//

#pragma once

#include <atomic>
#include <mutex>
#include <cinttypes>
#include "GenericHash.hpp"

#define LUAI_HASHLIMIT  5
#define SHORT_LEN_LIMIT 40

namespace jetpack {

    // copy from lua source code
    inline std::uint32_t luaS_hash (const char16_t *str, size_t l, std::uint32_t seed) {
        std::uint32_t h = seed ^ static_cast<std::uint32_t>(l);
        size_t step = (l >> LUAI_HASHLIMIT) + 1;
        for (; l >= step; l -= step)
            h ^= ((h<<5) + (h>>2) + static_cast<std::uint32_t>(str[l - 1]));
        return h;
    }

    inline int u16strcmp(const char16_t* str1, const char16_t* str2) {
        std::size_t idx = 0;

        while (true) {
            if (str1[idx] < str2[idx]) {
                return -1;
            } else if (str1[idx] > str2[idx]) {
                return 1;
            } else { // equal
                if (str1[idx] == u'\0') {
                    return 0;
                } else {
                    idx++;
                }
            }
        }
    }

    class IMRawString {
    public:
        std::atomic<std::int32_t> ref_count;
        std::uint8_t is_long_ = 0;
        std::uint32_t hash = 0;
        std::uint32_t size = 0;
        char16_t* raw = nullptr;

        std::uint32_t Hash();

        inline bool StrictEqual(const IMRawString& that) {
            return u16strcmp(raw, that.raw) == 0;
        }

        ~IMRawString();

    };

    inline bool IsDecimalDigit(char32_t cp) {
        return (cp >= 0x30 && cp <= 0x39);      // 0..9
    }

    class IMString {
    public:
        static IMString FromUTF8(const char* str);
        static IMString FromUTF8(const std::string& content);
        static IMString FromUTF8(const char* str, std::size_t s);
        static IMString FromUTF16(char16_t ch);
        static IMString FromUTF16(const std::u16string& u16);
        static IMString FromUTF16(const char16_t* str);
        static IMString FromUTF16(const char16_t* str, std::size_t s);

        IMString();
        IMString(const IMString& that);
        explicit IMString(IMString&& that);

        explicit IMString(IMRawString* raw_str_): raw_str(raw_str_) {
            raw_str->ref_count++;
        }

        IMString& operator=(const IMString& that);
        IMString& operator=(IMString&& that);

        bool operator==(const IMString& that) const;
        bool operator==(const char16_t* that) const;

        bool operator!=(const IMString& that) const;

        IMString operator+(const IMString& that) const;

        IMString& operator+=(const IMString& that);

        char16_t operator[](std::size_t idx) const;

        IMString Slice(std::size_t begin, std::size_t end) const;

        ~IMString();

        std::uint32_t Hash() const;

        std::string ToString() const;

        inline std::uint32_t Size() const {
            return raw_str->size;
        }

        inline const char16_t* Data() const {
            return raw_str->raw;
        }

        inline std::int32_t ToSimpleInt() const {
            std::int64_t result = 0;

            for (std::size_t i = 0; i < Size(); i++) {
                auto ch = (*this)[i];
                if (i == 0 && ch == u'0') {
                    return -1;
                }
                if (!IsDecimalDigit(ch)) {
                    return -1;
                }
                result = result * 10 + (ch - u'0');
                if (result > std::numeric_limits<std::int32_t>::max()) {
                    return -1;
                }
            }

            return static_cast<std::int32_t>(result);
        }

        inline bool Empty() const {
            return Size() == 0;
        }

        IMRawString* raw_str;

    private:
        void DecreaseRef();

    };

    class IMStringFactory {
    public:
        static IMStringFactory& GetInstance();
        static constexpr std::uint32_t MAX_HASH_CAP = 8192;

        IMStringFactory();
        IMStringFactory(const IMStringFactory&) = delete;
        explicit IMStringFactory(IMStringFactory&&) = delete;

        IMStringFactory& operator=(const IMStringFactory&) = delete;
        IMStringFactory& operator=(IMStringFactory&&) = delete;

        IMString FromUTF8(const std::string& content);
        IMString FromUTF8(const char* str, std::size_t s);
        IMString FromUTF16(const char16_t* str, std::size_t s);

        IMRawString* empty_raw_str;

        std::uint32_t seed_;

        void RemoveFromHash(IMRawString* raw_str);

        ~IMStringFactory();

        using Bucket = GenericHash<IMRawString, MAX_HASH_CAP>::Bucket;

    private:
        std::mutex hash_mutex_;
        GenericHash<IMRawString, MAX_HASH_CAP> hash_;

    };

    static_assert(sizeof(IMString) == 8);

}

template <>
struct std::hash<jetpack::IMString> {
    std::size_t operator()(const jetpack::IMString& str) const {
        return str.Hash();
    }
};
