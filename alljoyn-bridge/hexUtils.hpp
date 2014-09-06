#ifndef __HEX_UTILS_HPP_
#define __HEX_UTILS_HPP_

#include <hive/defs.hpp>

namespace utils
{

inline hive::String toHex(const hive::String &str)
{
    hive::String hex;
    hex.reserve(2*str.size());
    for (size_t i = 0; i < str.size(); ++i)
    {
        const int x = str[i];
        hex.push_back("0123456789abcdef"[(x>>4)&0x0f]);
        hex.push_back("0123456789abcdef"[(x>>0)&0x0f]);
    }

    return hex;
}

inline hive::String fromHex(const hive::String &hex)
{
    hive::String str;
    str.reserve(hex.size()/2);
    for (size_t i = 0; i+1 < hex.size(); i += 2)
    {
        int H = hex[i+0];
        int L = hex[i+1];

        //printf("%c%c ->", char(H), char(L));

        if (H >= '0' && H <= '9')
            H -= '0';
        else if (H >= 'a' && H <= 'f')
            H -= 'a'-10;
        else if (H >= 'A' && H <= 'F')
            H -= 'A'-10;
        else
            H = 0;

        if (L >= '0' && L <= '9')
            L -= '0';
        else if (L >= 'a' && L <= 'f')
            L -= 'a'-10;
        else if (L >= 'A' && L <= 'F')
            L -= 'A'-10;
        else
            L = 0;

        //printf("%x%x\n", H, L);

        str.push_back((H<<4)|L);
    }

    assert(hex == toHex(str));
    return str;
}


/**
 * @brief Limit the payload to print.
 */
inline hive::String lim(const hive::String& data, size_t max_size)
{
    if (data.size() <= max_size)
        return data;
    else
        return data.substr(0, max_size) + "...";
}

} // utils namespace


#endif // __HEX_UTILS_HPP_
