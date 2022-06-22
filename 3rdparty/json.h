/*
MIT License

Copyright (c) 2022 Cedric Jimenez

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef JSON_H
#define JSON_H

// Disable GCC warnings
#ifdef __GNUC__
#pragma GCC diagnostic push
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wexceptions"
#else
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif // __GNUC__

// Use exception for critical parse errors
// instead of asserts
#include <stdexcept>
namespace rapidjson
{
/** @brief Specific exception class dedicated to critical parse errors */
class parse_exception : public std::logic_error
{
  public:
    /** @brief Default constructor */
    parse_exception(const char* reason) : std::logic_error(reason) { }
};
} // namespace rapidjson
#define EXCEPTION_REASON_STRINGIFY(x) #x
#define RAPIDJSON_ASSERT(x) \
    if (!(x))               \
    throw parse_exception(EXCEPTION_REASON_STRINGIFY(x))

// Include rapidjson's headers
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/schema.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

// Restore GCC warnings
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif // __GNUC__

#endif // JSON_H
