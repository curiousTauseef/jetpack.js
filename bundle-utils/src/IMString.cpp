//
// Created by Duzhong Chen on 2020/4/9.
//

#include "IMString.h"
#include <random>

namespace jetpack {
    static int xmlLittleEndian = 1;

    /**
     * UTF8ToUTF16LE:
     * @outb:  a pointer to an array of bytes to store the result
     * @outlen:  the length of @outb
     * @in:  a pointer to an array of UTF-8 chars
     * @inlen:  the length of @in
     *
     * Take a block of UTF-8 chars in and try to convert it to an UTF-16LE
     * block of chars out.
     *
     * Returns the number of byte written, or -1 by lack of space, or -2
     *     if the transcoding failed.
     */
    static int
    UTF8ToUTF16LE(unsigned char* outb, int *outlen,
                  const unsigned char* in, int *inlen) {
        unsigned short* out = (unsigned short*) outb;
        const unsigned char* processed = in;
        unsigned short* outstart= out;
        unsigned short* outend;
        const unsigned char* inend= in+*inlen;
        unsigned int c, d;
        int trailing;
        unsigned char *tmp;
        unsigned short tmp1, tmp2;

        if (in == NULL) {
            /*
         * initialization, add the Byte Order Mark
         */
            if (*outlen >= 2) {
                outb[0] = 0xFF;
                outb[1] = 0xFE;
                *outlen = 2;
                *inlen = 0;
                return(2);
            }
            *outlen = 0;
            *inlen = 0;
            return(0);
        }
        outend = out + (*outlen / 2);
        while (in < inend) {
            d= *in++;
            if      (d < 0x80)  { c= d; trailing= 0; }
            else if (d < 0xC0) {
                /* trailing byte in leading position */
                *outlen = (out - outstart) * 2;
                *inlen = processed - in;
                return(-2);
            } else if (d < 0xE0)  { c= d & 0x1F; trailing= 1; }
            else if (d < 0xF0)  { c= d & 0x0F; trailing= 2; }
            else if (d < 0xF8)  { c= d & 0x07; trailing= 3; }
            else {
                /* no chance for this in UTF-16 */
                *outlen = (out - outstart) * 2;
                *inlen = processed - in;
                return(-2);
            }

            if (inend - in < trailing) {
                break;
            }

            for ( ; trailing; trailing--) {
                if ((in >= inend) || (((d= *in++) & 0xC0) != 0x80))
                    break;
                c <<= 6;
                c |= d & 0x3F;
            }

            /* assertion: c is a single UTF-4 value */
            if (c < 0x10000) {
                if (out >= outend)
                    break;
                if (xmlLittleEndian) {
                    *out++ = c;
                } else {
                    tmp = (unsigned char *) out;
                    *tmp = c ;
                    *(tmp + 1) = c >> 8 ;
                    out++;
                }
            }
            else if (c < 0x110000) {
                if (out+1 >= outend)
                    break;
                c -= 0x10000;
                if (xmlLittleEndian) {
                    *out++ = 0xD800 | (c >> 10);
                    *out++ = 0xDC00 | (c & 0x03FF);
                } else {
                    tmp1 = 0xD800 | (c >> 10);
                    tmp = (unsigned char *) out;
                    *tmp = (unsigned char) tmp1;
                    *(tmp + 1) = tmp1 >> 8;
                    out++;

                    tmp2 = 0xDC00 | (c & 0x03FF);
                    tmp = (unsigned char *) out;
                    *tmp  = (unsigned char) tmp2;
                    *(tmp + 1) = tmp2 >> 8;
                    out++;
                }
            }
            else
                break;
            processed = in;
        }
        *outlen = (out - outstart) * 2;
        *inlen = processed - in;
        return(0);
    }

    /**
     * UTF16LEToUTF8:
     * @out:  a pointer to an array of bytes to store the result
     * @outlen:  the length of @out
     * @inb:  a pointer to an array of UTF-16LE passwd as a byte array
     * @inlenb:  the length of @in in UTF-16LE chars
     *
     * Take a block of UTF-16LE ushorts in and try to convert it to an UTF-8
     * block of chars out. This function assume the endian properity
     * is the same between the native type of this machine and the
     * inputed one.
     *
     * Returns the number of byte written, or -1 by lack of space, or -2
     *     if the transcoding fails (for *in is not valid utf16 string)
     *     The value of *inlen after return is the number of octets consumed
     *     as the return value is positive, else unpredictiable.
     */
    static int
    UTF16LEToUTF8(unsigned char* out, int *outlen,
                  const unsigned char* inb, int *inlenb) {
        unsigned char* outstart = out;
        const unsigned char* processed = inb;
        unsigned char* outend = out + *outlen;
        unsigned short* in = (unsigned short*) inb;
        unsigned short* inend;
        unsigned int c, d, inlen;
        unsigned char *tmp;
        int bits;

        if ((*inlenb % 2) == 1)
            (*inlenb)--;
        inlen = *inlenb / 2;
        inend = in + inlen;
        while ((in < inend) && (out - outstart + 5 < *outlen)) {
            if (xmlLittleEndian) {
                c= *in++;
            } else {
                tmp = (unsigned char *) in;
                c = *tmp++;
                c = c | (((unsigned int)*tmp) << 8);
                in++;
            }
            if ((c & 0xFC00) == 0xD800) {    /* surrogates */
                if (in >= inend) {           /* (in > inend) shouldn't happens */
                    break;
                }
                if (xmlLittleEndian) {
                    d = *in++;
                } else {
                    tmp = (unsigned char *) in;
                    d = *tmp++;
                    d = d | (((unsigned int)*tmp) << 8);
                    in++;
                }
                if ((d & 0xFC00) == 0xDC00) {
                    c &= 0x03FF;
                    c <<= 10;
                    c |= d & 0x03FF;
                    c += 0x10000;
                }
                else {
                    *outlen = out - outstart;
                    *inlenb = processed - inb;
                    return(-2);
                }
            }

            /* assertion: c is a single UTF-4 value */
            if (out >= outend)
                break;
            if      (c <    0x80) {  *out++=  c;                bits= -6; }
            else if (c <   0x800) {  *out++= ((c >>  6) & 0x1F) | 0xC0;  bits=  0; }
            else if (c < 0x10000) {  *out++= ((c >> 12) & 0x0F) | 0xE0;  bits=  6; }
            else                  {  *out++= ((c >> 18) & 0x07) | 0xF0;  bits= 12; }

            for ( ; bits >= 0; bits-= 6) {
                if (out >= outend)
                    break;
                *out++= ((c >> bits) & 0x3F) | 0x80;
            }
            processed = (const unsigned char*) in;
        }
        *outlen = out - outstart;
        *inlenb = processed - inb;
        return(0);
    }

    std::uint32_t IMRawString::Hash() {
        if (is_long_ && hash == 0) {
            hash = luaS_hash(raw, size, IMStringFactory::GetInstance().seed_);
        }
        return hash;
    }

    IMRawString::~IMRawString() {
        if (raw != nullptr) {
            delete[] raw;
            raw = nullptr;
        }
    }

    IMString IMString::FromUTF8(const char* str) {
        std::size_t s = std::strlen(str);
        return FromUTF8(str, s);
    }

    IMString IMString::FromUTF8(const std::string& content) {
        return IMStringFactory::GetInstance().FromUTF8(content);
    }

    IMString IMString::FromUTF8(const char* str, std::size_t s) {
        return IMStringFactory::GetInstance().FromUTF8(str, s);
    }

    IMString IMString::FromUTF16(const char16_t *str) {
        std::size_t s = 0;
        while (str[s++] != u'\0');
        return FromUTF16(str, s - 1);
    }

    IMString IMString::FromUTF16(const std::u16string &u16) {
        return FromUTF16(u16.c_str(), u16.size());
    }

    IMString IMString::FromUTF16(char16_t ch) {
        char16_t buf[2] = { ch, 0 };
        return FromUTF16(buf, 1);
    }

    IMString IMString::FromUTF16(const char16_t* str, std::size_t s) {
        return IMStringFactory::GetInstance().FromUTF16(str, s);
    }

    IMString::IMString() {
        raw_str = IMStringFactory::GetInstance().empty_raw_str;
        raw_str->ref_count++;
    }

    IMString::IMString(const jetpack::IMString &that) {
        raw_str = that.raw_str;
        raw_str->ref_count++;
    }

    IMString::IMString(jetpack::IMString &&that) {
        raw_str = that.raw_str;
        that.raw_str = nullptr;
    }

    IMString & IMString::operator=(const IMString &that) {
        DecreaseRef();
        raw_str = that.raw_str;
        raw_str->ref_count++;
        return *this;
    }

    IMString & IMString::operator=(IMString &&that) {
        DecreaseRef();
        raw_str = that.raw_str;
        that.raw_str = nullptr;
        return *this;
    }

    bool IMString::operator==(const jetpack::IMString &that) const {
        if (raw_str == that.raw_str) {
            return true;
        }
        if (Size() != that.Size()) {
            return false;
        }
        if (Hash() != that.Hash()) {
            return false;
        }
        return raw_str->StrictEqual(*that.raw_str);
    }

    bool IMString::operator!=(const IMString &that) const {
        return !((*this) == that);
    }

    bool IMString::operator==(const char16_t *that) const {
        return u16strcmp(raw_str->raw, that) == 0;
    }

    char16_t IMString::operator[](std::size_t idx) const {
        return raw_str->raw[idx];
    }

    IMString IMString::operator+(const IMString &that) const {
        std::size_t s = Size() + that.Size() + 1;
        auto buffer = new char16_t[s];
        buffer[s - 1] = u'\0';

        for (std::size_t i = 0; i < Size(); i++) {
            buffer[i] = raw_str->raw[i];
        }

        char16_t* second = buffer + Size();
        for (std::size_t i = 0; i < that.Size(); i++) {
            second[i] = that.raw_str->raw[i];
        }

        IMString result = IMString::FromUTF16(buffer, s - 1);
        delete[] buffer;
        return result;
    }

    IMString & IMString::operator+=(const IMString &that) {
        (*this) = (*this) + that;

        return *this;
    }

    IMString IMString::Slice(std::size_t begin, std::size_t end) const {
        if (begin >= Size()) {
            return IMString();
        }
        if (end > Size()) {
            end = Size();
        }

        std::size_t s = end - begin + 1;
        char16_t* buffer = new char16_t[s];
        buffer[s - 1] = u'\0';

        for (std::size_t i = begin; i < end; i++) {
            buffer[i - begin] = (*this)[i];
        }

        IMString result = IMString::FromUTF16(buffer, s - 1);

        delete[] buffer;

        return result;
    }

    std::uint32_t IMString::Hash() const {
        return raw_str->Hash();
    }

    std::string IMString::ToString() const {
        int outSize = (Size() + 1) * 4;
        int inSize = Size() * 2;

        auto buffer = new char[outSize];
        buffer[outSize - 1] = '\0';

        if (UTF16LEToUTF8(reinterpret_cast<unsigned char *>(buffer), &outSize,
                          reinterpret_cast<const unsigned char *>(raw_str->raw), &inSize)) {
            delete[] buffer;
            return std::string();
        }

        std::string result = std::string(buffer, outSize);

        delete[] buffer;
        return result;
    }

    IMString::~IMString() {
        DecreaseRef();
    }

    void IMString::DecreaseRef() {
        if (raw_str == nullptr) {
            return;
        }
        if (--raw_str->ref_count == 0) {
            if (!raw_str->is_long_ && IMStringFactory::GetInstance().empty_raw_str != nullptr) {
                IMStringFactory::GetInstance().RemoveFromHash(raw_str);
            }
            delete raw_str;
            raw_str = nullptr;
        }
    }

    IMStringFactory& IMStringFactory::GetInstance() {
        static IMStringFactory instance;
        return instance;
    };

    IMStringFactory::IMStringFactory() {
        std::default_random_engine generator;
        std::uniform_int_distribution<std::uint32_t> counter_dis(
                0, std::numeric_limits<std::uint32_t>::max());

        seed_ = counter_dis(generator);

        empty_raw_str = new IMRawString();
        empty_raw_str->is_long_ = 0;
        empty_raw_str->ref_count = 1;
        empty_raw_str->hash = luaS_hash(u"", 0, seed_);
        empty_raw_str->size = 0;
        empty_raw_str->raw = new char16_t[1];
        empty_raw_str->raw[0] = u'\0';

        hash_.Init();
    }

    IMString IMStringFactory::FromUTF8(const std::string &content) {
        return FromUTF8(content.c_str(), content.size());
    }

    IMString IMStringFactory::FromUTF8(const char* str, std::size_t s) {
        std::size_t u16bufSize = s * 2 + 1;
        auto buffer = new unsigned char[u16bufSize];
        buffer[u16bufSize - 1] = 0;

        int outLen = u16bufSize;
        int inLen = s;
        if (UTF8ToUTF16LE(buffer, &outLen, reinterpret_cast<const unsigned char *>(str), &inLen)) {
            delete[] buffer;
            return IMString();
        }

        IMString result = FromUTF16(reinterpret_cast<char16_t*>(buffer), outLen / 2);

        delete[] buffer;

        return result;
    }

    IMString IMStringFactory::FromUTF16(const char16_t *str, std::size_t s) {
        if (s == 0) {
            return IMString(empty_raw_str);
        }

        if (s < SHORT_LEN_LIMIT) {
            std::lock_guard<std::mutex> lk(hash_mutex_);

            std::uint32_t hash = luaS_hash(str, s, seed_);
            std::uint32_t bucket_id = hash % hash_.Capacity();

            if (hash_.size_ > hash_.Capacity()) {
                hash_.Rehash(hash_.Capacity() * 2);
            }

            Bucket* bucket = hash_.data_[bucket_id];
            while (bucket != nullptr) {
                if (bucket->val_->Hash() == hash && u16strcmp(bucket->val_->raw, str) == 0) {
                    return IMString(bucket->val_);
                }
                bucket = bucket->next_;
            }

            auto raw_str = new IMRawString();
            raw_str->ref_count = 0;
            raw_str->is_long_ = 0;
            raw_str->hash = hash;
            raw_str->size = s;
            raw_str->raw = new char16_t[s + 1];
            for (std::size_t i = 0; i < s; i++) {  // copy data
                raw_str->raw[i] = str[i];
            }
            raw_str->raw[s] = u'\0';

            bucket = new Bucket;
            bucket->val_ = raw_str;
            bucket->next_ = hash_.data_[bucket_id];
            hash_.data_[bucket_id] = bucket;

            hash_.size_++;

            return IMString(raw_str);
        } else {
            auto raw_str = new IMRawString();
            raw_str->ref_count = 0;
            raw_str->is_long_ = 1;
            raw_str->hash = 0;
            raw_str->size = s;
            raw_str->raw = new char16_t[s + 1];
            for (std::size_t i = 0; i < s; i++) {  // copy data
                raw_str->raw[i] = str[i];
            }
            raw_str->raw[s] = u'\0';

            return IMString(raw_str);
        }
    }

    void IMStringFactory::RemoveFromHash(IMRawString *raw_str) {
        std::lock_guard<std::mutex> lk(hash_mutex_);
        hash_.Erase(*raw_str);
    }

    IMStringFactory::~IMStringFactory() {
        delete empty_raw_str;
        empty_raw_str = nullptr;
    }

}
