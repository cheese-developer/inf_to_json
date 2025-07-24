/**
 * @file setup_api.cppm
 * @brief Thin, low-level C++23 module wrapping Win32 SetupAPI INF parsing
 *        primitives with safe, RAII-style helpers.
 *
 * Design goals:
 *  - Keep all Win32/`windows.h` exposure inside this module.
 *  - Export modern C++ API to read sections and lines.
 *  - Use case-insensitive semantics for *section names* and *keys* to match
 *    Windows INF rules; field values remain case-sensitive.
 *  - Throw exceptions on errors instead of returning error codes.
 *  - Rely on SetupAPI behavior: **%strKey% tokens are expanded by SetupAPI**
 *    before strings are returned.
 *
 * Platform: Windows only. Includes directive to link with `setupapi.lib`.
 */

module;

#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <optional>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#undef WIN32_LEAN_AND_MEAN
#undef NOMINMAX

#pragma comment(lib, "setupapi.lib")

export module setup_api;

/**
 * @class winapi_case_insensitive_wchar_traits
 * @brief `char_traits` that implements case-insensitive comparisons using
 *        Win32 case folding (`CharLowerW`).
 *
 * Rationale: INF grammar treats identifiers (section names, keys) as
 * case-insensitive. Using Windows-provided folding avoids divergence from
 * the OS behavior. Hashing also folds characters to lower case.
 *
 * @note Uses `CharLowerW` which performs Windows case folding; this is not
 *       locale-dependent in the usual sense but follows Windows rules.
 *       The parts of the implementation that rely on WinAPI are not `constexpr` by design.
 *       All methods are `noexcept` as MSVC STL requires.
 */
class winapi_case_insensitive_wchar_traits
{
private:

    static wchar_t char_lower(wchar_t value) noexcept
    {
        return static_cast<wchar_t>(std::bit_cast<std::intptr_t>(
            CharLowerW(
                std::bit_cast<LPWSTR>(std::intptr_t{ value }))));
    }

public:
    using char_type = typename std::char_traits<wchar_t>::char_type;
    using int_type = typename std::char_traits<wchar_t>::int_type;
    using off_type = typename std::char_traits<wchar_t>::off_type;
    using pos_type = typename std::char_traits<wchar_t>::pos_type;
    using state_type = typename std::char_traits<wchar_t>::state_type;

    static constexpr void assign(char_type& char_to, const char_type& char_from) noexcept
    {
        std::char_traits<wchar_t>::assign(char_to, char_from);
    }

    static constexpr char_type* assign(
        char_type* str_to,
        size_t number,
        char_type char_from) noexcept
    {
        return std::char_traits<wchar_t>::assign(
            str_to,
            number,
            char_from);
    }

    static bool eq(char_type a, char_type b) noexcept
    {
        return char_lower(a) == char_lower(b);
    }

    static bool lt(char_type a, char_type b) noexcept
    {
        return char_lower(a) < char_lower(b);
    }

    static constexpr char_type* move(
        char_type* dest,
        const char_type* src,
        std::size_t count) noexcept
    {
        return std::char_traits<wchar_t>::move(
            dest,
            src,
            count);
    }

    static constexpr char_type* copy(
        char_type* dest,
        const char_type* src,
        std::size_t count) noexcept
    {
        return std::char_traits<wchar_t>::copy(
            dest,
            src,
            count);
    }

    static int compare(
        const char_type* left,
        const char_type* right,
        std::size_t count) noexcept
    {
        size_t processed{ 0 };
        while (processed < count)
        {
            wchar_t delta = char_lower(*left) - char_lower(*right);
            if (delta < L'\0')
            {
                return -1;
            }
            else if (delta > 0)
            {
                return 1;
            }

            ++left;
            ++right;
            ++processed;
        }

        return 0;
    }

    static constexpr std::size_t length(const char_type* string) noexcept
    {
        return std::char_traits<wchar_t>::length(string);
    }

    static const char_type* find(
        const char_type* string,
        std::size_t count,
        const char_type& value) noexcept
    {
        size_t processed{ 0 };
        wchar_t lowercase_value = char_lower(value);
        while (processed < count)
        {
            if (char_lower(*string) == lowercase_value)
            {
                return string;
            }

            ++string;
            ++processed;
        }

        return nullptr;
    }

    static constexpr char_type to_char_type(const int_type& value) noexcept
    {
        return std::char_traits<wchar_t>::to_char_type(value);
    }

    static constexpr int_type to_int_type(const char_type& value) noexcept
    {
        return std::char_traits<wchar_t>::to_int_type(value);
    }

    static bool eq_int_type(int_type left, int_type right) noexcept
    {
        return eq(to_char_type(left), to_char_type(right));
    }

    static constexpr int_type eof() noexcept
    {
        return std::char_traits<wchar_t>::eof();
    }

    static constexpr int_type not_eof(int_type value) noexcept
    {
        return std::char_traits<wchar_t>::not_eof(value);
    }

    template <typename range>
    static size_t hash(const range& string)
    {
        size_t result{ 0 };
        for (wchar_t ch : string)
        {
            // simple prime-based hash
            result = result * 131 + char_lower(ch);
        }
        return result;
    }
};

using winapi_case_insensitive_wstring_view = std::basic_string_view<wchar_t, winapi_case_insensitive_wchar_traits>;

template <typename allocator>
using winapi_case_insensitive_wstring = std::basic_string<
    wchar_t,
    winapi_case_insensitive_wchar_traits,
    allocator>;

template <typename allocator>
class std::hash<winapi_case_insensitive_wstring<allocator>>
{
public:
    using is_transparent = std::true_type;
    using string_view = winapi_case_insensitive_wstring_view;
    using string = winapi_case_insensitive_wstring<allocator>;

    std::size_t operator()(string_view value) const noexcept
    {
        return winapi_case_insensitive_wchar_traits::hash(value);
    }

    std::size_t operator()(string value) const noexcept
    {
        return winapi_case_insensitive_wchar_traits::hash(value);
    }
};

template<>
class std::hash<winapi_case_insensitive_wstring_view>
{
public:
    using string_view = winapi_case_insensitive_wstring_view;

    std::size_t operator()(string_view value) const noexcept
    {
        return winapi_case_insensitive_wchar_traits::hash(value);
    }
};

/**
 * @typedef section_name
 * @brief Case-insensitive string for INF section names.
 */
export using section_name = std::basic_string<
    wchar_t,
    winapi_case_insensitive_wchar_traits,
    std::allocator<wchar_t>>;

/**
 * @typedef section_name_view
 * @brief Case-insensitive string view for INF section names.
 */
export using section_name_view = std::basic_string_view<wchar_t, winapi_case_insensitive_wchar_traits>;

/**
 * @typedef key_name
 * @brief Case-insensitive string for INF key identifiers (left side before '=').
 */
export using key_name = std::basic_string<
    wchar_t,
    winapi_case_insensitive_wchar_traits,
    std::allocator<wchar_t>>;

/**
 * @typedef key_name_view
 * @brief Case-insensitive string view for INF keys.
 */
export using key_name_view = std::basic_string_view<wchar_t, winapi_case_insensitive_wchar_traits>;

/**
 * @class section_search
 * @brief Enumerator over all section names in an opened INF file.
 *
 * Usage pattern:
 * ```cpp
 * section_search it(handle);
 * while (it.move_next()) {
 *     auto name = it.value();
 * }
 * ```
 *
 * @throws std::runtime_error on Win32 errors during enumeration.
 */
class section_search
{
private:
    HINF file; // non-owning
    UINT next_index;
    section_name buffer;
    bool done;

    void update_state()
    {
        UINT size_needed;
        BOOL found = SetupEnumInfSectionsW(
            file,
            next_index,
            NULL,
            0,
            &size_needed);

        if (found == TRUE)
        {
            buffer.resize(size_needed);
            found = SetupEnumInfSectionsW(
                file,
                next_index,
                buffer.data(),
                size_needed,
                NULL);

            if (found == FALSE)
            {
                throw std::runtime_error("Failed to retrieve an INF section name");
            }

            buffer.resize(size_needed - 1);
        }
        else if (GetLastError() == ERROR_NO_MORE_ITEMS)
        {
            done = true;
        }
        else
        {
            throw std::runtime_error("Failed to enumerate INF sections");
        }
    }

public:
    section_search(HINF file)
        : file{ file },
        next_index{0},
        buffer{},
        done{false}
    {
    }

    bool move_next()
    {
        update_state();
        ++next_index;
        return !done;
    }

    bool has_value() const noexcept
    {
        return next_index != 0 && !done;
    }

    section_name_view value() const
    {
        if (!has_value())
        {
            throw std::logic_error("INF sections enumeration either not started or already finished");
        }

        return buffer;
    }
};

/**
 * @class line
 * @brief Represents a single line inside an INF section.
 *
 * The object owns a copy of `INFCONTEXT` and provides:
 *  - `key()` — case-insensitive key (field index 0 in SetupAPI terms).
 *  - `size()` — count of *value* fields (number of comma-separated items),
 *               reported via `SetupGetFieldCount`.
 *  - `field_at(i)` — 0-based accessor to value fields. Uses
 *                    `SetupGetStringFieldW` and throws on failure.
 *
 * @note Values are returned exactly as SetupAPI expands them; %strkeys% are
 *       already substituted.
 * @throws std::runtime_error on Win32 failures, `std::out_of_range` for bad
 *         indices.
 */
export class line
{
private:
    mutable INFCONTEXT context;
    key_name key_buffer;
    mutable std::optional<DWORD> count; // lazily initialized
    mutable std::wstring field_buffer;

    template <typename char_traits>
    void get_field(DWORD field, std::basic_string<wchar_t, char_traits, std::allocator<wchar_t>>& target) const
    {
        DWORD target_size;
        if (SetupGetStringFieldW(
                &context,
                field,
                NULL,
                0,
                &target_size)
            == FALSE)
        {
            throw std::runtime_error("Failed to determine a string field length");
        }

        target.resize(target_size);
        if (SetupGetStringFieldW(
                &context,
                field,
                target.data(),
                target_size,
                NULL)
            == FALSE)
        {
            throw std::runtime_error("Failed to retrieve a string field");
        }

        target.resize(target_size - 1);
    }

    constexpr static DWORD key_index{0};

    explicit line(const INFCONTEXT& context)
        : context{context},
        key_buffer{},
        count{ std::nullopt },
        field_buffer{}
    {
        get_field(key_index, key_buffer);
    }

    friend class inf_file;
    friend class line_search;
public:

    key_name_view key() const noexcept
    {
        return key_buffer;
    }

    size_t size() const
    {
        if (!count.has_value())
        {
            SetLastError(NO_ERROR);
            DWORD field_count = SetupGetFieldCount(&context);
            if (field_count == 0 && GetLastError() != NO_ERROR)
            {
                throw std::runtime_error("Failed to retrieve field count");
            }

            count = field_count;
        }

        return *count;
    }

    std::wstring_view field_at(ptrdiff_t index) const
    {
        if (index < 0
            || static_cast<size_t>(index) >= size()
            || index > std::numeric_limits<DWORD>::max() - 1)
        {
            throw std::out_of_range("INF field index out of range");
        }

        get_field(
            static_cast<DWORD>(index) + 1, // INF fields indexes are 1-based
            field_buffer);

        return field_buffer;
    }
};

/**
 * @class line_search
 * @brief Linear enumerator across all lines within a given section.
 *
 * Implements the usual `move_next()` + `current_line()` pattern on top of
 * `SetupFindFirstLineW` / `SetupFindNextLine`.
 *
 * @throws std::runtime_error on Win32 failures; throws `std::logic_error` if
 *         misused (e.g., reading after end).
 */
class line_search
{
private:
    enum class position
    {
        start,
        middle,
        end
    };

    HINF file; // non-owning
    section_name_view section;
    INFCONTEXT context;
    position current;

public:
    line_search(HINF file, section_name_view section)
        : file{ file },
        section{ section },
        context{},
        current{position::start}
    {
    }

    bool move_next()
    {
        switch (current)
        {
        case position::start:
        {
            BOOL found = SetupFindFirstLineW(
                file,
                section.data(),
                NULL,
                &context);
            if (found == FALSE)
            {
                switch (GetLastError())
                {
                case ERROR_SECTION_NOT_FOUND:
                    throw std::runtime_error("Could not find the specified INF section");

                case ERROR_LINE_NOT_FOUND:
                    current = position::end;
                    return false;

                default:
                    throw std::runtime_error("Fatal error while retrieving the first line of an INF section");
                }
            }
            else
            {
                current = position::middle;
            }
        }
        break;

        case position::middle:
        {
            BOOL advanced = SetupFindNextLine(&context, &context);
            if (!advanced)
            {
                if (GetLastError() == ERROR_LINE_NOT_FOUND)
                {
                    current = position::end;
                    return false;
                }
                else
                {
                    throw std::runtime_error("Fatal error while retrieving a next line of an INF section");
                }
            }
        }
        break;

        case position::end:
            throw std::logic_error("Enumeration has already been closed");

        default:
            throw std::logic_error("Unexpected position value");
        }

        return true;
    }

    bool has_line() const noexcept
    {
        return current == position::middle;
    }

    line current_line() const
    {
        if (!has_line())
        {
            throw std::logic_error("INF line enumeration either not started or already finished");
        }

        return line(context);
    }
};

/**
 * @enum enumeration
 * @brief Control flow for visitors: continue enumeration or stop early.
 */
export enum class enumeration
{
    move_next,
    stop
};

/**
 * @class inf_file
 * @brief RAII wrapper for an INF handle with high-level enumeration helpers.
 *
 * Responsibilities:
 *  - Open/close the INF (`SetupOpenInfFileW` / `SetupCloseInfFile`).
 *  - `for_each_section(F)` — visits every section, stops early if the visitor
 *    returns `enumeration::stop`.
 *  - `for_each_line(section, F)` — visits each line within a section.
 *  - `get_line(section, key)` — returns the first matching line or `nullopt` if
 *    the key is absent; throws if the section does not exist.
 *
 * @throws std::runtime_error for Win32 failures.
 */
export class inf_file
{
private:

    static constexpr const HINF empty = INVALID_HANDLE_VALUE;
    HINF handle;

public:

    bool is_opened() const noexcept
    {
        return handle != empty;
    }

private:

    void ensure_open() const
    {
        if (!is_opened())
        {
            throw std::logic_error("INF file has been unexpectedly closed");
        }
    }

    void close() noexcept
    {
        if (!is_opened())
        {
            return;
        }

        SetupCloseInfFile(handle);
        handle = empty;
    }

public:

    explicit inf_file(const std::filesystem::path& inf_path)
        : handle{ SetupOpenInfFileW(
            inf_path.native().c_str(),
            NULL,
            INF_STYLE_WIN4,
            NULL)}
    {
        if (handle == empty) {
            throw std::runtime_error("Failed to open the requested INF file");
        }
    }

    ~inf_file()
    {
        close();
    }

    inf_file(inf_file&) = delete;
    inf_file& operator=(inf_file&) = delete;

    inf_file(inf_file&& other) noexcept
    : handle { std::exchange(other.handle, empty) }
    {
    }

    inf_file& operator=(inf_file&& other) noexcept
    {
        if (this == &other)
        { 
            return *this;
        }

        close();
        handle = std::exchange(other.handle, empty);

        return *this;
    }

    template <typename F>
    requires std::is_invocable_r_v<enumeration, F, section_name_view>
    void for_each_section(F&& section_name_handler) const
    {
        ensure_open();

        section_search search(handle);
        while (search.move_next())
        {
            if (section_name_handler(search.value()) == enumeration::stop)
            {
                return;
            }
        }
    }

    template <typename F>
    requires std::is_invocable_r_v<enumeration, F, line&&>
    void for_each_line(section_name_view section_name, F&& key_value_handler) const
    {
        ensure_open();

        line_search search(handle, section_name);
        while (search.move_next())
        {
            if (key_value_handler(search.current_line()) == enumeration::stop)
            {
                return;
            }
        }
    }

    std::optional<line> get_line(section_name_view section, key_name_view key) const
    {
        ensure_open();

        INFCONTEXT context;
        BOOL found = SetupFindFirstLineW(
            handle,
            section.data(),
            key.data(),
            &context);

        if (!found)
        {
            switch (GetLastError())
            {
            case ERROR_SECTION_NOT_FOUND:
                throw std::runtime_error("Could not find the specified INF section");

            case ERROR_LINE_NOT_FOUND:
                return std::nullopt;

            default:
                throw std::runtime_error("Fatal error while searching for a line of an INF section");
            }
        }

        return line(context);
    }
};

/**
 * @brief Convert UTF-16 text to UTF-8 using `WideCharToMultiByte`.
 *
 * @param unicode Source UTF-16 string view.
 * @return UTF-8 encoded `std::string`.
 * @throws std::range_error if the input length exceeds Win32 conversion limits.
 * @throws std::runtime_error on conversion failures.
 */
export template <typename traits>
std::string to_utf8(std::basic_string_view<wchar_t, traits> unicode)
{
    if (unicode.empty())
    {
        return std::string{};
    }

    if (unicode.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        throw std::range_error("Input too long for UTF-8 conversion");
    }

    int size_bytes = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        unicode.data(),
        static_cast<int>(unicode.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size_bytes == 0)
    {
        throw std::runtime_error("Failed to determine UTF-8 output size");
    }

    std::string result(size_bytes, '\0');

    int actual_converted = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        unicode.data(),
        static_cast<int>(unicode.size()),
        result.data(),
        size_bytes,
        nullptr,
        nullptr);
    if (actual_converted == 0)
    {
        throw std::runtime_error("Failed to perform UTF-8 conversion");
    }

    result.resize(actual_converted);
    return result;
}

/**
 * @brief Convert UTF-16 text to UTF-8 using `WideCharToMultiByte`.
 *
 * @param unicode Source UTF-16 string.
 * @return UTF-8 encoded `std::string`.
 * @throws std::range_error if the input length exceeds Win32 conversion limits.
 * @throws std::runtime_error on conversion failures.
 */
export template <typename traits, typename allocator>
std::string to_utf8(std::basic_string<wchar_t, traits, allocator> unicode)
{
    std::basic_string_view<wchar_t, traits> view{ unicode };
    return to_utf8(view);
}
