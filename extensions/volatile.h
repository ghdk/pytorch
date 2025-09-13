#ifndef VOLATILE_H
#define VOLATILE_H

namespace extensions
{

/**
 * C++20 deprecates some operators on volatile variables. Wrap volatile
 * variables to provide a familiar API.
 */
template <typename T>
struct Volatile
{
public:
    operator T()
    {
        return value_;
    }
    Volatile operator++(int)
    {
        Volatile ret = *this;
        value_ = value_ + 1;
        return ret;
    }
    Volatile operator--(int)
    {
        Volatile ret = *this;
        value_ = value_ - 1;
        return ret;
    }
    bool operator==(Volatile const& other) const
    {
        return value_ == other.value_;
    }
public:
    Volatile(T v)
    : value_{std::move(v)}
    {}
    Volatile(void) = default;
    Volatile(Volatile const&) = default;
    Volatile(Volatile&&) = default;
    Volatile& operator=(Volatile const&) = delete;
    Volatile& operator=(Volatile&&) = delete;
    Volatile& operator=(T d)
    {
        value_ = d;
        return *this;
    }
private:
    volatile T value_ = 0;
};

}  // namespace extensions

#endif  // VOLATILE_H
