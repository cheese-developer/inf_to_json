#include <iostream>
#include <vector>
#include <unordered_set>
#include <ranges>
#include <tuple>
#include <generator>

#include <nlohmann/json.hpp>

import setup_api;

#include "reader.h"
#include "report.h"
#include "json.h"

enum class exit_codes : int
{
    success,
    invalid_arguments,
    error,
    unspecified_error,
    out_of_memory
};

/**
 * @brief Worker entry that performs parsing and JSON emission.
 * @param argc Count of command-line arguments.
 * @param argv Wide-character argv; expects `argv[1]` to be a path to an INF.
 * @return Exit code as `exit_codes` enum.
 */
exit_codes main_with_code(int argc, wchar_t* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: inf_to_json <inf-file-path>" << std::endl;
        return exit_codes::invalid_arguments;
    }

    try
    {
        inf_file file(argv[1]);
        auto r = select_report_data(file);
        std::cout << nlohmann::json(r).dump(2) << std::endl;
    }
    catch (const std::bad_alloc&)
    {
        return exit_codes::out_of_memory;
    }
    catch (const std::exception& e)
    {
        try
        {
            nlohmann::json error;
            error["error"] = e.what();
            std::cerr << error.dump(2) << std::endl;
            return exit_codes::error;
        }
        catch (...)
        {
            return exit_codes::unspecified_error;
        }
    }
    catch (...)
    {
        std::cerr << "{\"error\": \"Unexpected error\"}" << std::endl;
        return exit_codes::unspecified_error;
    }

    return exit_codes::success;
}

/**
 * @brief Windows wide main entry point.
 * Delegates to `main_with_code` and returns its integral value.
 */
int wmain(int argc, wchar_t* argv[])
{
    return static_cast<int>(main_with_code(argc, argv));
}
