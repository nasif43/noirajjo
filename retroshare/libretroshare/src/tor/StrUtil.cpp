/* Ricochet - https://ricochet.im/
 * Copyright (C) 2014, John Brooks <john.brooks@dereferenced.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 *    * Neither the names of the copyright owners nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "StrUtil.h"

ByteArray quotedString(const ByteArray &string)
{
    ByteArray out;
    out.reserve(string.size() * 2);

    out.push_back('"');

    for (ByteArray::size_type i = 0; i < string.size(); ++i)
    {
        switch (string[i])
        {
        case '"':
            out.append("\\\"");
            break;
        case '\\':
            out.append("\\\\");
            break;
        default:
            out.push_back(string[i]);
            break;
        }
    }

    out.push_back('"');
    return out;
}

ByteArray unquotedString(const ByteArray& string)
{
    if (string.size() < 2 || string[0] != '"')
        return string;

    ByteArray out;
    out.reserve(string.size() - 2);

    for (ByteArray::size_type i = 1; i < string.size(); ++i)
    {
        switch (string[i])
        {
        case '\\':
            if (++i < string.size())
                out.push_back(string[i]);
            break;
        case '"':
            return out;
        default:
            out.push_back(string[i]);
        }
    }

    return out;
}

std::list<ByteArray> splitQuotedStrings(const ByteArray &input, char separator)
{
    std::list<ByteArray> out;
    bool inquote = false;
    ByteArray::size_type start = 0;

    for (ByteArray::size_type i = 0; i < input.size(); ++i)
    {
        switch (input[i])
        {
        case '"':
            inquote = !inquote;
            break;
        case '\\':
            if (inquote)
                ++i;
            break;
        }

        if (!inquote && input[i] == separator)
        {
            out.push_back(input.mid(start, i - start));
            start = i+1;
        }
    }

    if (start < input.size())
        out.push_back(input.mid(start));

    return out;
}
