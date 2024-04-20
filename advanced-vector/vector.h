#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept
    {
        return data_.GetAddress();
    }

    iterator end() noexcept
    {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept
    {
        return const_iterator(data_.GetAddress());
    }

    const_iterator end() const noexcept
    {
        return const_iterator(data_.GetAddress() + size_);
    }

    const_iterator cbegin() const noexcept
    {
        return begin();
    }

    const_iterator cend() const noexcept
    {
        return end();
    }

    Vector() = default;

    explicit Vector(size_t size)
        :data_(size),
        size_(size)

    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        :data_(other.size_),
        size_(other.size_)

    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept

    {
        Swap(other);
    }

    Vector& operator=(const Vector& rhs) noexcept
    {
        if (this != &rhs) {
            if (data_.Capacity() < rhs.size_) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + std::min(rhs.size_, size_), data_.GetAddress());
                if (rhs.size_ < size_) {                    
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else {                   
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept
    {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    size_t Size() const noexcept
    {
        return size_;
    }

    size_t Capacity() const noexcept
    {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept
    {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept
    {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity)
    {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Swap(Vector& other) noexcept
    {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size)
    {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else {
            if (new_size > Capacity()) {
                const size_t new_capacity = std::max(new_size, Capacity() * 2);
                Reserve(new_capacity);
            }
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value)
    {
       EmplaceBack(value);
    }

    void PushBack(T&& value)
    {
        EmplaceBack(std::move(value));
    }
    
    //Не проходит несколько тестов если я делаю что то вроде такого. В чем может быть проблема?
    /*
    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        auto result = Emplace(end(), std::forward<Args>(args)...);
        return *result;
    }
    */
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args)
    {
        T* result = nullptr;
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            result = new(new_data + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            result = new(data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *result;
    }

    void PopBack() noexcept {
        if (size_ > 0) {
            std::destroy_at(data_.GetAddress() + size_ - 1);
            --size_;
        }
    }

template <typename... Args>
iterator Emplace(const_iterator pos, Args&&... args) {
    assert(pos >= begin() && pos <= end());
    size_t shift = pos - begin();
    iterator result = nullptr;
    if (size_ == Capacity()) {
        result = EmplaceWithReallocate(shift,std::forward<Args>(args)...);
    }
    else {
        result = EmplaceWithoutReallocate(shift, std::forward<Args>(args)...);
    }
    ++size_;
    return result;
}
    
    iterator Erase(const_iterator pos) noexcept
    {
        size_t shift = pos - begin();
        if (pos >= begin() && pos <= end()) {           
            std::move(begin() + shift + 1, end(), begin() + shift);
            PopBack();
            
        }
        return begin() + shift;
    }

    iterator Insert(const_iterator pos, const T& value)
    {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value)
    {
        return Emplace(pos, std::move(value));
    }


    ~Vector()
    {
        std::destroy_n(data_.GetAddress(), size_);
    }

private:
    static T* Allocate(size_t n)
    {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept
    {
        operator delete(buf);
    }


    static void CopyConstruct(T* buf, const T& elem)
    {
        new (buf) T(elem);
    }
    
template <typename... Args>
iterator EmplaceWithReallocate(size_t shift, Args&&... args)
{
    RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
    iterator res = new (new_data + shift) T(std::forward<Args>(args)...);
    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
        std::uninitialized_move_n(data_.GetAddress(), shift, new_data.GetAddress());
        std::uninitialized_move_n(data_.GetAddress() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
    }
    else {
        try {
            std::uninitialized_copy_n(data_.GetAddress(), shift, new_data.GetAddress());
            std::uninitialized_copy_n(data_.GetAddress() + shift, size_ - shift, new_data.GetAddress() + shift + 1);
        }
        catch (...) {
            std::destroy_n(new_data.GetAddress() + shift, 1);
            throw;
        }
    }
    std::destroy_n(begin(), size_);
    data_.Swap(new_data);
    return res;
}

template <typename... Args>
iterator EmplaceWithoutReallocate(size_t shift, Args&&... args)
{
    if (size_ != 0) {
        new (data_ + size_) T(std::move(*(data_.GetAddress() + size_ - 1)));
        try {
            std::move_backward(begin() + shift,
                data_.GetAddress() + size_,
                data_.GetAddress() + size_ + 1);
        }
        catch (...) {
            std::destroy_n(data_.GetAddress() + size_, 1);
            throw;
        }
        std::destroy_at(begin() + shift);
    }
    iterator result = new (data_ + shift) T(std::forward<Args>(args)...);
    return result;
}


    RawMemory<T> data_;
    size_t size_ = 0;
};