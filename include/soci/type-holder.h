//
// Copyright (C) 2004-2008 Maciej Sobczak, Stephen Hutton
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SOCI_TYPE_HOLDER_H_INCLUDED
#define SOCI_TYPE_HOLDER_H_INCLUDED

#include "soci/soci-backend.h"
// std
#include <ctime>
#include <typeinfo>

namespace soci
{

namespace details
{

// Returns U* as T*, if the dynamic type of the pointer is really T.
//
// This should be used instead of dynamic_cast<> because using it doesn't work
// when using libc++ and ELF visibility together. Luckily, when we don't need
// the full power of the cast, but only need to check if the types are the
// same, it can be done by comparing their type info objects.
//
// This function does _not_ replace dynamic_cast<> in all cases and notably
// doesn't allow the input pointer to be null.
template <typename T, typename U>
T* checked_ptr_cast(U* ptr)
{
    // Check if they're identical first, as an optimization, and then compare
    // their names to make it actually work with libc++.
    std::type_info const& ti_ptr = typeid(*ptr);
    std::type_info const& ti_ret = typeid(T);

    if (&ti_ptr != &ti_ret && std::strcmp(ti_ptr.name(), ti_ret.name()) != 0)
    {
        return NULL;
    }

    return static_cast<T*>(ptr);
}

// Base class holder + derived class type_holder for storing type data
// instances in a container of holder objects
template <typename T>
class type_holder;

class holder
{
public:
    holder(soci::data_type dt_) : dt(dt_) {}
    virtual ~holder() {}

    template<typename T>
    T get();
private:
    soci::data_type dt;
};

template <typename T>
class type_holder : public holder
{
public:
    type_holder(soci::data_type dt, T * t) : holder(dt), t_(t) {}
    ~type_holder() override { delete t_; }

    T value() const { return *t_; }

private:
    T * t_;
};

class type_holder_bad_cast : public std::bad_cast
{
public:
    type_holder_bad_cast(soci::data_type dt, const std::string& context)
    {
        info = std::bad_cast::what();
        switch (dt)
        {
            case soci::dt_string:               info.append(": expected std::string, got ");        break;
            case soci::dt_date:                 info.append(": expected std::tm, got ");            break;
            case soci::dt_double:               info.append(": expected double, got ");             break;
            case soci::dt_integer:              info.append(": expected int, got ");                break;
            case soci::dt_long_long:            info.append(": expected long long, got ");          break;
            case soci::dt_unsigned_long_long:   info.append(": expected unsigned long long, got "); break;
            case soci::dt_blob:                 info.append(": expected std::string(blob), got ");  break;
            case soci::dt_xml:                  info.append(": expected std::string(xml), got ");   break;
        }
        info.append(context);
    }
    const char* what() const noexcept override
    {
        return info.c_str();
    }
private:
    std::string info;
};

template<typename T, typename Enable = void>
struct type_holder_cast
{
    static_assert(std::is_same<T, void>::value, "Unmatched type_holder");
    static inline T cast(soci::data_type, const holder*)
    {
        throw std::bad_cast(); // Unreachable: only provided for compilation corectness
    }
};


template<typename T>
struct type_holder_cast<T, typename std::enable_if<std::is_arithmetic<T>::value>::type>
{
    static inline T cast(soci::data_type dt, const holder* hl)
    {
        switch (dt)
        {
            case soci::dt_double:               return (T)static_cast<const type_holder<double>*>(hl)->value();
            case soci::dt_integer:              return (T)static_cast<const type_holder<int>*>(hl)->value();
            case soci::dt_long_long:            return (T)static_cast<const type_holder<long long>*>(hl)->value();
            case soci::dt_unsigned_long_long:   return (T)static_cast<const type_holder<unsigned long long>*>(hl)->value();
            default: throw type_holder_bad_cast(dt, typeid(T).name());
        }
    }
};

template<>
struct type_holder_cast<std::string>
{
    static inline std::string cast(soci::data_type dt, const holder* hl)
    {
        switch (dt)
        {
            case soci::dt_blob:
            case soci::dt_xml:
            case soci::dt_string:   return static_cast<const type_holder<std::string>*>(hl)->value();
            default: throw type_holder_bad_cast(dt, "std::string");
        }
    }
};

template<>
struct type_holder_cast<std::tm>
{
    static inline std::tm cast(soci::data_type dt, const holder* hl)
    {
        switch (dt)
        {
            case soci::dt_date: return static_cast<const type_holder<std::tm>*>(hl)->value();
            default: throw type_holder_bad_cast(dt, "std::tm");
        }
    }
};

template<typename T>
T holder::get()
{
    return type_holder_cast<T>::cast(dt, this);
}

} // namespace details

} // namespace soci

#endif // SOCI_TYPE_HOLDER_H_INCLUDED
