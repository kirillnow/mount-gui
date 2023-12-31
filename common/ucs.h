/* Copyright (c) 2015-2023 Kovshov K.A.

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

/**
 * @file ucs.h
 * @author Kovshov K.A.
 * @brief Set of functions dealing with UTF encodings
**/

#ifndef KIRILLNOW_UCS_H_INCLUDED
#define KIRILLNOW_UCS_H_INCLUDED
//-------------------------------------------------------------------------------------------------
#include <cstdint>
#include <string>
#include <stdexcept>
//-------------------------------------------------------------------------------------------------
namespace details
{
  template<class I> using is_ra_char_iterator = std::integral_constant<bool,
    std::is_same<typename std::iterator_traits<I>::iterator_category,
                 std::random_access_iterator_tag>::value &&
    std::is_same<typename std::iterator_traits<I>::value_type, char>::value>;

  template<typename T, typename...> struct _first_arg { using type = T; };
  template<typename... Ts> using _first_arg_t = typename _first_arg<Ts...>::type;
}
//-------------------------------------------------------------------------------------------------

///Checks if a given unicode code point labeled as <not a character>.
constexpr bool ucs_is_nonchar(uint32_t ucp) noexcept
{ return ((ucp&0xFFFE) == 0xFFFE) | (ucp >= 0xFDD0 && ucp <= 0xFDEF); }
//-------------------------------------------------------------------------------------------------

///Checks if a given unicode code point is a surrogate.
constexpr bool ucs_is_surrogate(uint32_t ucp) noexcept { return ucp>=0xD800 && ucp<=0xDFFF; }
//-------------------------------------------------------------------------------------------------

///Checks if a given unicode code is an the range defined by the Unicode Standard.
constexpr bool ucs_is_valid(uint32_t ucp) noexcept { return ucp <= 0x10FFFF; }
//-------------------------------------------------------------------------------------------------

/** @brief Проверяет и декодирует последовательность байт одного символа из UTF-8.
 *  @param bs     4 байта исходной строки, LE порядок байт.
 *  @param ucp    Возвращаемое значение символа (кодовой точки) юникода (-1 если код некорректен).
 *  @return Число байт, кодирующих символ (err_ln если код некорректен).
 */
constexpr uint8_t decode_utf8(uint32_t bs, uint32_t& ucp, uint8_t err_ln = 0) noexcept
{
  uint8_t ln = err_ln;
  if(!( bs&0x80 )) { ln = 1; ucp = bs&0xFF; }
  else if(!( (bs&0xC0E0)^0x80C0 ))
   { ln=2; ucp=(bs&0x1F)<<6 | (bs&0x3F00)>>8; }
  else if(!( (bs&0xC0C0F0)^0x8080E0 ))
   { ln=3; ucp=(bs&0x0F)<<12 | (bs&0x3F00)>>2 | (bs&0x3F0000)>>16; }
  else if(!( (bs&0xC0C0C0F8)^0x808080F0 ))
   { ln=4; ucp=(bs&0x07)<<18 | (bs&0x3F00)<<4 | (bs&0x3F0000)>>10 | ((bs>>24)&0x3F); }
  else ucp = uint32_t(-1);
  return ln;
}
//-------------------------------------------------------------------------------------------------

/** @brief Increments *first* until it's pointing to a (possible invalid) utf-8 code point.
 *  @return *false* if *last* is reached.
 */
template<typename I, typename = std::enable_if_t<details::is_ra_char_iterator<I>::value>>
constexpr bool sync_utf8(I &first, I last) noexcept
{
  for(; first != last; ++first) if((*first & 0xC0) != 0x80) { return true; }
  return false;
}
//-------------------------------------------------------------------------------------------------

class utf8_view
{
  protected:
    const char* _M_first  = nullptr;
    const char* _M_last   = nullptr;
    uint32_t    _M_val    = -1;
    uint8_t     _M_len    = 1;
    int8_t      _M_cwidth = -1;

    constexpr explicit utf8_view(int,const char* end) noexcept :_M_first(end), _M_last(end) {}

  public:
    typedef std::input_iterator_tag  iterator_category;
    typedef uint32_t                 value_type;
    typedef uint32_t                 reference;
    typedef value_type*              pointer;
    typedef std::streamoff           difference_type;

    ///Value obtained by dereferencing an iterator that's pointing to the invalid utf-8 sequence.
    static constexpr value_type inval = value_type(-1);

    constexpr utf8_view() = default;

    template<typename I, typename = std::enable_if_t<details::is_ra_char_iterator<I>::value>>
    constexpr utf8_view(I first, I last) noexcept
      :_M_first(&*first), _M_last(&*last) { if(first != last) update(); }

    #ifdef __cpp_lib_string_view
      constexpr utf8_view(std::string_view str) noexcept :utf8_view{str.begin(), str.end()} {}
    #else
      utf8_view(const std::string& str) noexcept :utf8_view{str.begin(), str.end()} {}
    #endif
    constexpr utf8_view(const char* str, size_t len) noexcept :utf8_view{str, str + len} {}

    utf8_view(std::string&&) = delete; //Temporary would be destroyed before use

    #ifdef __cpp_lib_string_view
      constexpr operator std::string_view() const noexcept
      { return {base(), size_t(_M_last - _M_first)}; }

      template<class T = std::string_view::const_iterator,
               typename = std::enable_if_t<std::is_constructible<T, const char*>::value>>
      constexpr operator std::string_view::const_iterator() const noexcept { return T{_M_first}; }
    #endif
    template<class T = std::string::const_iterator,
             typename = std::enable_if_t<std::is_constructible<T, const char*>::value>>
    operator std::string::const_iterator() const noexcept { return T{_M_first}; }

    constexpr void swap(utf8_view& r) noexcept { utf8_view t = *this; *this = r; r = t; }

    constexpr utf8_view   begin() const noexcept { return *this; }
    constexpr utf8_view   end()   const noexcept { return utf8_view{0, _M_last}; }

    ///Returns pointer to a first byte in the current code point.
    constexpr const char* base()  const noexcept { return _M_first; }

    ///Returns length of the current code point in bytes (1 for inval).
    constexpr uint8_t     len()   const noexcept { return _M_len; }

    ///Returns true if begin() == end()
    constexpr bool        empty() const noexcept { return _M_first == _M_last; }

    constexpr value_type operator*() const noexcept { return _M_val; }

    constexpr utf8_view& operator++() noexcept
    { _M_first += _M_len; if(!empty()) { update(); } return *this; }

    constexpr utf8_view  operator++(int) noexcept { utf8_view r(*this); ++*this; return r; }

    ///Returns *true* if the iterator points to a valid utf-8 code point.
    constexpr bool is_valid() const noexcept { return ucs_is_valid(_M_val); }

    ///Checks if the unicode point is encoded with the shortest possible byte sequence.
    constexpr bool is_shortest() const noexcept
    { return _M_len == ((_M_val&(~0x7FF))?((_M_val&(~0xFFFF))?4:3):(_M_val&(~0x7F))?2:1); }

    ///Advance iterator until a valid utf-8 code point is found or the end of the view is reached.
    constexpr bool sync() noexcept
    {
      for(;; _M_first+=len()) { if(!sync_utf8(_M_first, _M_last)) return false;
                                if(update(), is_valid())          return true;  }
    }

    ///Updates internal state of the iterator (for example if pointed-to bytes are changed).
    constexpr void update() noexcept
    {
      uint32_t bs = -1;
      const auto len = _M_last - _M_first;
      const auto p   = _M_first; //can't just cast it to uint8_t* in constexpr function :(
      if(len >= 4)      bs = uint8_t(*p) | uint32_t(uint8_t(p[1]))<<8  |
                                           uint32_t(uint8_t(p[2]))<<16 |
                                           uint32_t(uint8_t(p[3]))<<24;
      else if(len == 3) bs = uint8_t(*p) | uint32_t(uint8_t(p[1]))<<8
                                         | uint32_t(uint8_t(p[2]))<<16;
      else if(len == 2) bs = uint8_t(*p) | uint32_t(uint8_t(p[1]))<<8;
      else if(len == 1) bs = uint8_t(*p);
      _M_len = decode_utf8(bs, _M_val, 1);
    }

    friend constexpr bool operator==(const utf8_view& l, const utf8_view& r) noexcept
    { return l._M_first == r._M_first; }
    friend constexpr bool operator!=(const utf8_view& l, const utf8_view& r) noexcept
    { return l._M_first != r._M_first; }
    friend constexpr bool operator<(const utf8_view& l, const utf8_view& r)  noexcept
    { return l._M_first < r._M_first; }
    friend constexpr bool operator<=(const utf8_view& l, const utf8_view& r) noexcept
    { return l._M_first <= r._M_first; }
    friend constexpr bool operator>(const utf8_view& l, const utf8_view& r)  noexcept
    { return l._M_first > r._M_first; }
    friend constexpr bool operator>=(const utf8_view& l, const utf8_view& r) noexcept
    { return l._M_first >= r._M_first; }
};

constexpr void swap(utf8_view& l, utf8_view& r) noexcept { l.swap(r); }

/*  Best not to encourage use of the surprisingly costly operations...
constexpr utf8_view& operator+=(utf8_view& l, utf8_view::difference_type n) noexcept
{ return std::advance(l, n), l; }

constexpr utf8_view operator+(const utf8_view& l, utf8_view::difference_type n) noexcept
{ utf8_view t(l); return std::advance(t, n), t; }
*/
//-------------------------------------------------------------------------------------------------

/** @brief Проверяет если строка состоит из корректно представленных символов UTF-8.
 */
template<bool check_nonchar = false, bool check_surrogates = false,
         bool check_shortest = true, bool check_null = true, class... Args,
         typename = std::enable_if_t<std::is_constructible<utf8_view,
                                       std::add_lvalue_reference_t<Args>...>::value>>
constexpr bool validate_utf8(Args&&... args) noexcept
{
  for(auto f = utf8_view{args...}, l = f.end(); f != l; ++f) //no std::forward to allow string&&
   {
    if(!f.is_valid() || (check_null && !*f) || (check_shortest && !f.is_shortest()) ||
       (check_surrogates && ucs_is_surrogate(*f)) || (check_nonchar && ucs_is_nonchar(*f)))
      return false;
   }
  return true;
}
//-------------------------------------------------------------------------------------------------

/// Reads utf16 BOM and returns [le=0: LE; be=1: BE; def: NO BOM].
template<int le = 0, int be = 1>
inline int rd_utf16_bom(const char* &str, size_t &sz, int def = 0) noexcept
{
  if(sz<2) return def;
  char16_t s0=(uint16_t(*str)<<8)|str[1];
  return (s0==0xFEFF)?sz-=2, str+=2, be : ((s0==0xFFFE)?sz-=2, str+=2, le : def);
}
//-------------------------------------------------------------------------------------------------

///Swap endianness of the utf16 string.
constexpr void utf16_bswap(char16_t* first, const char16_t* last) noexcept
{
  for(; first != last; ++first)
    *first = (uint16_t(*first) >> 8) | (uint16_t(*first) << 8); //GCC and CLANG can vectorize this
}
//-------------------------------------------------------------------------------------------------

/** @brief ISO 8859-1 to UTF-8 conversion function.
 */
inline std::string latin1_to_utf8(const char *s, size_t sz)
{
  std::string rtv;
  const char *bgn, *end=s+sz;
  while(s<end)
   {
    if(*s&128)
     {
      rtv.push_back(0xC0|(*s>>6));
      rtv.push_back(0xBF&*s);
      ++s; continue;
     }
    for(bgn=s; s<end && !(*s&128); ++s);
    rtv.append(bgn, s-bgn);
   }
  return rtv;
}
//-------------------------------------------------------------------------------------------------

/** @brief UTF-16 [with specific endianness] to UTF-8 conversion function.
 *  @note Not most optimized routine, but comparable to std.
 */
template<bool big_e=false>
constexpr size_t utf16_to_utf8(const char *in, size_t sz, char *out) noexcept
{
  uint32_t ucs=0; sz &= ~(size_t)1; //odd sizes are rounded down
  char *o = out;
  for(const uint8_t *i=(uint8_t*)in, *e=i+sz; i<e; i+=2, ++o)
   {
    ucs = uint32_t(i[!big_e])<<8 | i[big_e];
    if((ucs&0xFC00)==0xD800 && i+2<e && (i[2+!big_e]&0xFC)==0xDC)
     {
      i+=2; ucs &= 0x03FF;
      ucs = (ucs<<10 | uint32_t(i[!big_e]&3)<<8 | i[big_e]) + 0x010000;
     } //pass standalone surrogates as-is
    if(ucs&(~0x7FF))  //encode_utf8, but I don't like mucking around with force_inline.
     {
      if(ucs&(~0xFFFF))
       { *o=ucs>>18|0xF0;         ++o; *o=(ucs>>12 & 0x3F)|0x80; ++o;
         *o=(ucs>>6 & 0x3F)|0x80; ++o; *o=(ucs&0x3F)|0x80;            }
      else
       { *o=ucs>>12|0xE0; ++o; *o=(ucs>>6&0x3F)|0x80; ++o; *o=(ucs&0x3F)|0x80; }
     }
    else { if(ucs&(~0x7F)) { *o=ucs>>6|0xC0; ++o; *o=(ucs&0x3F)|0x80; }
           else *o = ucs;                                                      }
   }
  return o-out;
}
//-------------------------------------------------------------------------------------------------

/** @brief Convert UTF-8 to UTF-16 [platform endianness].
 */
constexpr size_t utf8_to_utf16(const char* &in, size_t &isz, char16_t* out,
                               bool add_bom = false, bool stop_at_inv = false) noexcept
{
  size_t w_count=0;
  if(add_bom) { *out=0xFEFF; ++out; ++w_count; }

  auto f = utf8_view(in, isz), l = f.end();
  while(f != l)
   {
    if(!f.is_valid()) { if(stop_at_inv) break; else f.sync(); }
    else if(f.len() == 4)
     {
      uint32_t ucp = *f - 0x010000; *out = (ucp>>10)|0xD800;    ++out;
                                    *out = (ucp&0x03FF)|0xDC00; ++out;
      w_count += 2; ++f;
     }
    else { *(out++) = *f; ++w_count; ++f; }
   }
  in = f.base(); isz = l.base() - in;
  return w_count;
}
//-------------------------------------------------------------------------------------------------

/** @brief Convert UTF32 [platfom endianness] to UTF8.
 */
constexpr size_t utf32_to_utf8(const char32_t* in, size_t sz, char* out) noexcept
{
  uint32_t ucs=0;
  char *o = out;
  for(const char32_t *e = in+sz; in<e; ++in, ++o)
   {
    ucs = *in;
    if(ucs&(~0x7FF))
     {
      if(ucs&(~0xFFFF))
       { *o=ucs>>18|0xF0;         ++o; *o=(ucs>>12 & 0x3F)|0x80; ++o;
         *o=(ucs>>6 & 0x3F)|0x80; ++o; *o=(ucs&0x3F)|0x80;            }
      else
       { *o=ucs>>12|0xE0; ++o; *o=(ucs>>6&0x3F)|0x80; ++o; *o=(ucs&0x3F)|0x80; }
     }
    else { if(ucs&(~0x7F)) { *o=ucs>>6|0xC0; ++o; *o=(ucs&0x3F)|0x80; }
           else *o = ucs;                                                      }
   }
  return o-out;
}
//-------------------------------------------------------------------------------------------------

/** @brief Convert UTF8 to UTF32 [platfom endianness].
 */
constexpr size_t utf8_to_utf32(const char* &in, size_t &isz, char32_t* out,
                               bool add_bom = false, bool stop_at_inv = false) noexcept
{
  size_t w_count = 0;
  if(add_bom) { *out=0xFEFF; ++out; ++w_count; }

  auto f = utf8_view(in, isz), l = f.end();
  while(f != l)
   {
    if(f.is_valid()) { *(out++) = *f; ++w_count; ++f; }
    else { if(stop_at_inv) break; else f.sync(); }
   }
  in = f.base(); isz = l.base() - in;
  return w_count;
}
//-------------------------------------------------------------------------------------------------

/** @brief UTF-16 [with specific endianness] to UTF-8 conversion function.
 */
template<bool big_e=false> inline std::string utf16_to_utf8(const char *in, size_t sz)
{
  std::string rtv; rtv.resize(sz+sz/2);
  rtv.resize(utf16_to_utf8<big_e>(in, sz, (char*)rtv.data()));
  return rtv;
}
//-------------------------------------------------------------------------------------------------
/** @brief UTF-16 [with specific endianness] to UTF-8 conversion function.
 */
template<bool big_e=false> inline std::string utf16_to_utf8(const char16_t *in, size_t sz)
{
  std::string rtv; rtv.resize(sz*3);
  rtv.resize(utf16_to_utf8<big_e>((char*)in, sz*2, (char*)rtv.data()));
  return rtv;
}
//-------------------------------------------------------------------------------------------------
/** @brief Convert UTF-8 to [platform endianness] UTF-16.
 */
inline std::u16string utf8_to_utf16(const char* in, size_t sz, bool add_bom = false,
                                                               bool stop_at_inv = false)
{
  std::u16string rtv; rtv.resize(sz);
  rtv.resize(utf8_to_utf16(in, sz, (char16_t*)rtv.data(), add_bom, stop_at_inv));
  return rtv.shrink_to_fit(), rtv;
}
//-------------------------------------------------------------------------------------------------
/** @brief Convert UTF-8 to [platform endianness] UTF-16 in std::wstring.
 */
inline std::wstring utf8_to_utf16w(const char* in, size_t sz, bool add_bom = false,
                                                              bool stop_at_inv = false)
{
  if(sizeof(wchar_t) != sizeof(char16_t))
    throw std::logic_error("utf8_to_utf16w: wchar_t is not 16 bit type!");

  std::wstring rtv; rtv.resize(sz);
  rtv.resize(utf8_to_utf16(in, sz, (char16_t*)rtv.data(), add_bom, stop_at_inv));
  return rtv.shrink_to_fit(), rtv;
}
//-------------------------------------------------------------------------------------------------

/** @brief UTF-32 [with platform endianness] to UTF-8 conversion function.
 */
inline std::string utf32_to_utf8(const char32_t *in, size_t sz)
{
  std::string rtv(sz*4, '\0');
  rtv.resize(utf32_to_utf8(in, sz, (char*)rtv.data()));
  return rtv.shrink_to_fit(), rtv;
}
//-------------------------------------------------------------------------------------------------

/** @brief Convert UTF-8 to [platform endianness] UTF-32.
 */
inline std::u32string utf8_to_utf32(const char* in, size_t sz, bool add_bom = false,
                                                               bool stop_at_inv = false)
{
  std::u32string rtv(sz, U'\0');
  rtv.resize(utf8_to_utf32(in, sz, (char32_t*)rtv.data(), add_bom, stop_at_inv));
  return rtv.shrink_to_fit(), rtv;
}
//-------------------------------------------------------------------------------------------------

/** @brief Convert UTF-8 to [platform endianness] UTF-32 in std::wstring.
 */
inline std::wstring utf8_to_utf32w(const char* in, size_t sz, bool add_bom = false,
                                                              bool stop_at_inv = false)
{
  if(sizeof(wchar_t) != sizeof(char32_t))
    throw std::logic_error("utf8_to_utf32w: wchar_t is not 32 bit type!");

  std::wstring rtv(sz, L'\0');
  rtv.resize(utf8_to_utf32(in, sz, (char32_t*)rtv.data(), add_bom, stop_at_inv));
  return rtv.shrink_to_fit(), rtv;
}
//-------------------------------------------------------------------------------------------------
#ifdef __cpp_lib_string_view
//-------------------------------------------------------------------------------------------------

/** @brief UTF-16 [with specific endianness] to UTF-8 conversion function.
 */
template<bool big_e=false> inline std::string utf16_to_utf8(std::string_view str)
{
  return utf16_to_utf8<big_e>(str.data(), str.size());
}
//-------------------------------------------------------------------------------------------------

/** @brief UTF-16 [with specific endianness] to UTF-8 conversion function.
 */
template<bool big_e=false, typename C, typename = std::enable_if_t<sizeof(C) == 2>>
inline std::string utf16_to_utf8(std::basic_string_view<C> str)
{
  return utf16_to_utf8<big_e>((const char16_t*)str.data(), str.size());
}
//-------------------------------------------------------------------------------------------------
/** @brief Convert UTF-8 to [platform endianness] UTF-16.
 */
inline std::u16string utf8_to_utf16(std::string_view str, bool add_bom = false,
                                                          bool stop_at_inv = false)
{
  return utf8_to_utf16(str.data(), str.size(), add_bom, stop_at_inv);
}
//-------------------------------------------------------------------------------------------------
/** @brief Convert UTF-8 to [platform endianness] UTF-16 in std::wstring.
 */
inline std::wstring utf8_to_utf16w(std::string_view str, bool add_bom = false,
                                                         bool stop_at_inv = false)
{
  return utf8_to_utf16w(str.data(), str.size(), add_bom, stop_at_inv);
}
//-------------------------------------------------------------------------------------------------

/** @brief UTF-32 [with platform endianness] to UTF-8 conversion function.
 */
inline std::string utf32_to_utf8(std::u32string_view str)
{
  return utf32_to_utf8(str.data(), str.size());
}
//-------------------------------------------------------------------------------------------------

/** @brief Convert UTF-8 to [platform endianness] UTF-32.
 */
inline std::u32string utf8_to_utf32(std::string_view str, bool add_bom = false,
                                                          bool stop_at_inv = false)
{
  return utf8_to_utf32(str.data(), str.size(), add_bom, stop_at_inv);
}
//-------------------------------------------------------------------------------------------------

/** @brief Convert UTF-8 to [platform endianness] UTF-32 in std::wstring.
 */
inline std::wstring utf8_to_utf32w(std::string_view str, bool add_bom = false,
                                                         bool stop_at_inv = false)
{
  return utf8_to_utf32w(str.data(), str.size(), add_bom, stop_at_inv);
}
//-------------------------------------------------------------------------------------------------
#endif // __cpp_lib_string_view
//-------------------------------------------------------------------------------------------------
#endif // KIRILLNOW_UCS_H_INCLUDED
