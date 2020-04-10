//
// Created by Duzhong Chen on 2020/3/13.
//

#pragma once

#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace jetpack {

    template <class T,
            std::size_t MAX_CAPACITY = 4096,
            std::size_t MIN_CAPACITY = 64, class = std::void_t<> >
    class GenericHash : std::false_type {

    };

    template<class T, std::size_t MAX_CAPACITY, std::size_t MIN_CAPACITY>
    class GenericHash<
            T, MAX_CAPACITY, MIN_CAPACITY, std::void_t<
                    decltype(std::declval<T>().Hash())>> : std::true_type {
    public:
        static const std::size_t INIT_SIZE = 64;

        struct Bucket {
        public:
            Bucket* next_ = nullptr;
            T* val_ = nullptr;

        };

        inline
        GenericHash()
                : size_(0), data_(nullptr) {

        }

        inline
        GenericHash(GenericHash<T>&& that) noexcept {
            FreeMem();

            capacity_ = that.capacity_;
            size_ = that.size_;
            data_ = that.data_;

            that.size_ = 0;
            that.data_ = nullptr;
        }

        GenericHash(const GenericHash<T>&) = delete;

        GenericHash& operator=(const GenericHash<T>&) = delete;
        GenericHash& operator=(GenericHash<T>&& that) {
            FreeMem();

            capacity_ = that.capacity_;
            size_ = that.size_;
            data_ = that.data_;

            that.size_ = 0;
            that.data_ = nullptr;

            return *this;
        }

        inline void Init(std::size_t capacity = INIT_SIZE) {
            capacity_ = capacity;

            std::size_t alloc_size = sizeof(Bucket*) * capacity_;
            data_ = reinterpret_cast<Bucket**>(malloc(alloc_size));
            std::memset(data_, 0, alloc_size);
        }

        inline bool FindHash(std::uint32_t hash, GenericHash<T, MAX_CAPACITY, MIN_CAPACITY>::Bucket** ret) {
            std::uint32_t bucket_id = hash % capacity_;

            Bucket* bucket = data_[bucket_id];
            while (bucket != nullptr) {
                if (bucket->val_->Hash() == hash) {
                    if (ret) {
                        *ret = bucket;
                    }
                    return true;
                }
                bucket = bucket->next_;
            }
            return false;
        }

        inline bool CreateOrReturnBucket(
                T& value, GenericHash<T, MAX_CAPACITY, MIN_CAPACITY>::Bucket** ret) {

            if (size_ > capacity_) {
                Rehash(capacity_ * 2);
            }

            std::uint32_t hash = value.Hash();
            std::uint32_t bucket_id = hash % capacity_;

            Bucket* bucket = data_[bucket_id];
            while (bucket != nullptr) {
                if (bucket->val_->Hash() == hash && value.StrictEqual(*bucket->val_)) {
                    if (ret) {
                        *ret = bucket;
                    }
                    return false;
                }
                bucket = bucket->next_;
            }

            bucket = reinterpret_cast<GenericHash<T, MAX_CAPACITY, MIN_CAPACITY>::Bucket*>(
                    malloc(sizeof(GenericHash<T, MAX_CAPACITY, MIN_CAPACITY>::Bucket)));
            memset(bucket, 0, sizeof(GenericHash<T, MAX_CAPACITY, MIN_CAPACITY>::Bucket));
            bucket->next_ = data_[bucket_id];
            data_[bucket_id] = bucket;

            if (ret) {
                *ret = bucket;
            }

            size_++;

            return true;
        }

        void Erase(T& t) {
            std::uint32_t bucket_id = t.Hash() % capacity_;
            GenericHash<T, MAX_CAPACITY, MIN_CAPACITY>::Bucket** bucket = &data_[bucket_id];

            while (*bucket != nullptr) {
                if ((*bucket)->val_->Hash() == t.Hash() && t.StrictEqual(*(*bucket)->val_)) {
                    GenericHash<T, MAX_CAPACITY, MIN_CAPACITY>::Bucket* removing_bucket = *bucket;
                    (*bucket) = (*bucket)->next_;
                    free(removing_bucket);
                    --size_;

                    if (size_ < capacity_ / 4) {
                        Rehash(capacity_ / 2);
                    }
                    return;
                }

                bucket = &(*bucket)->next_;
            }
        }

        void Rehash(std::size_t capacity) {
            if (capacity > MAX_CAPACITY || capacity <= MIN_CAPACITY) {
                return;
            }

            Bucket** new_data = reinterpret_cast<Bucket**>(malloc(sizeof(Bucket*) * capacity));
            memset(new_data, 0, sizeof(Bucket*) * capacity);

            for (std::size_t i = 0; i < capacity_; i++) {
                Bucket* bucket = data_[i];
                while (bucket != nullptr) {
                    auto next = bucket->next_;  // save the next

                    std::uint32_t new_id = bucket->val_->Hash() % capacity;  // mod the new capacity

                    auto new_bucket = reinterpret_cast<GenericHash<T, MAX_CAPACITY, MIN_CAPACITY>::Bucket*>(
                            malloc(sizeof(GenericHash<T, MAX_CAPACITY, MIN_CAPACITY>::Bucket)));
                    memset(new_bucket, 0, sizeof(GenericHash<T, MAX_CAPACITY, MIN_CAPACITY>::Bucket));
                    new_bucket->next_ = new_data[new_id];
                    new_bucket->val_ = bucket->val_;
                    new_data[new_id] = new_bucket;

                    bucket = next;
                }
            }

            FreeMem();
            capacity_ = capacity;
            data_ = new_data;
        }

        inline std::size_t Size() const {
            return size_;
        }

        inline std::size_t Capacity() const {
            return capacity_;
        }

        Bucket** data_;

        ~GenericHash() {
            FreeMem();
            size_ = 0;
            data_ = nullptr;
        }

        std::size_t size_;
    private:
        void FreeMem() {
            if (data_ != nullptr) {
                Bucket* bucket = nullptr;
                for (std::size_t i = 0; i < capacity_; i++) {  // iterate list
                    bucket = data_[i];

                    while (bucket != nullptr) {
                        auto next = bucket->next_;  // remember the next

                        free(bucket);  // only free bucket, no need to free str

                        bucket = next;
                    }
                }

                free(data_);  // free the list
            }
        }

        std::size_t capacity_;

    };

}
