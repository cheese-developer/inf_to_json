/**
 * @class model
 * @brief High-level report entry for a single device model.
 *
 * All strings are UTF-8 for easy JSON serialization.
 */
class model
{
public:
    std::string description;
    std::vector<std::string> hardware_ids;
    std::vector<std::string> architectures;
};

/**
 * @class manufacturer
 * @brief High-level report entry for a single manufacturer with its models.
 */
class manufacturer
{
public:
    std::string name;
    std::vector<model> devices;
};

/**
 * @typedef report
 * @brief Final report type: list of manufacturers with their devices.
 */
using report = std::vector<manufacturer>;

/**
 * @class models_sections_correlation
 * @brief Internal helper that ties a resolved models section to the
 *        architecture string used to form it. For the base section (no
 *        architecture suffix), `architecture` is empty.
 */
class models_sections_correlation
{
public:
    std::wstring_view architecture;
    section_name models_section;
};

/**
 * @brief Produce all existing models-section names for a manufacturer.
 *
 * The function yields the base section if present, and then for each
 * architecture qualifier, yields `base.arch` if such a section exists.
 *
 * @return `std::generator` yielding correlations lazily.
 */
std::generator<models_sections_correlation> correlate_models_sections(
    const manufacturer_line& manufacturer,
    const std::unordered_set<section_name>& all_sections)
{
    static constexpr wchar_t delimiter = '.';

    if (std::unordered_set<section_name>::const_iterator section = all_sections.find(manufacturer.models_section_name)
        ; section != all_sections.end())
    {
        co_yield models_sections_correlation{ .architecture{}, .models_section{ *section} };
    }

    for (const auto& architecture : manufacturer.architectures)
    {
        std::unordered_set<section_name>::const_iterator section;
        {
            section_name composed;
            size_t length = manufacturer.models_section_name.size() + 1 + architecture.size();
            composed.reserve(length);
            composed = manufacturer.models_section_name;
            composed += delimiter;
            composed.append_range(architecture);
            section = all_sections.find(composed);
        }

        if (section != all_sections.end())
        {
            co_yield models_sections_correlation{ .architecture { architecture }, .models_section{ *section} };
        }
    }
}

/**
 * @class model_key
 * @brief Key used to deduplicate models across multiple sections: a pair of
 *        (description, list of hardware IDs). Case-insensitive comparison is
 *        used for the description via `key_name`.
 */
class model_key
{
public:
    key_name description;
    std::vector<std::wstring> hardware_ids;
};

/**
 * @brief Hash functor for `model_key`.
 *
 * Combines a hash of the case-insensitive description with hashes of all
 * hardware IDs. Order-sensitive.
 */
template <>
class std::hash<model_key>
{
public:
    size_t operator()(const model_key& key) const noexcept
    {
        size_t result{ std::hash<key_name>{}(key.description) };
        std::hash<std::wstring> hasher;
        for (const auto& id : key.hardware_ids)
        {
            result = result * 131 + hasher(id);
        }

        return result;
    }
};

/**
 * @brief Equality for `model_key` — description and the ordered list of
 *        hardware IDs must match.
 */
bool operator==(const model_key& left, const model_key& right) noexcept
{
    return left.description == right.description
        && std::ranges::equal(left.hardware_ids, right.hardware_ids);
}

/**
 * @brief Build the final JSON-ready report from an INF file.
 *
 * Steps:
 *  1. Extract manufacturers.
 *  2. Enumerate all section names.
 *  3. For each manufacturer, resolve actual architecture-specific sections that exist.
 *  4. Parse devices from each architecture-specific section.
 *  5. Group devices by description and hardware_ids, gathering a list of
 *     architectures where they appear.
 *  6. Convert everything to UTF-8 and produce a `report`.
 *
 * @throws std::exception on Win32 or parsing failures.
 */
report select_report_data(const inf_file& inf)
{
    report output;

    const std::unordered_set<section_name> all_sections = extract_sections(inf);
    for (auto& inf_manufacturer : extract_manufacturers(inf))
    {
        std::unordered_map<model_key, std::vector<std::wstring>> model_data{};
        for (models_sections_correlation correlation : correlate_models_sections(inf_manufacturer, all_sections))
        {
            for (auto&& inf_device : extract_device_descriptions(inf, correlation.models_section))
            {
                model_key key{ .description = std::move(inf_device.device_description), .hardware_ids = std::move(inf_device.hardware_ids) };

                if (auto found = model_data.find(key)
                    ; found != model_data.end())
                {
                    auto& architectures = found->second;
                    architectures.emplace_back(correlation.architecture);
                }
                else
                {
                    model_data.insert(
                        std::pair{
                            std::move(key),
                            std::vector{ std::wstring{correlation.architecture} } });
                }
            }
        }

        manufacturer report_entry{ .name = to_utf8(inf_manufacturer.name) };
        report_entry.devices.reserve(model_data.size());
        for (const auto& [key, architectures] : model_data)
        {
            model model{ .description = to_utf8(key.description) };

            model.hardware_ids.reserve(key.hardware_ids.size());
            for (const std::wstring& hardware_id : key.hardware_ids)
            {
                model.hardware_ids.push_back(to_utf8(hardware_id));
            }

            model.architectures.reserve(architectures.size());
            for (const std::wstring& architecture : architectures)
            {
                model.architectures.push_back(to_utf8(architecture));
            }

            report_entry.devices.push_back(std::move(model));
        }

        output.emplace_back(std::move(report_entry));
    }

    return output;
}
