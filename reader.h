/**
 * @class manufacturer_line
 * @brief Parsed representation of one line in the `[Manufacturer]` section.
 *
 * Example line:
 *   `%MfgName% = ASUP, ntamd64.10.0...16299`
 *
 * Semantics:
 *  - `name` – expanded manufacturer name (left-hand key).
 *  - `models_section_name` – base models section name (e.g. `ASUP`).
 *  - `architectures` – optional target OS/platform qualifiers that combine
 *    with the base section via `base.arch` (dot) syntax.
 */
class manufacturer_line
{
public:
    key_name name;
    section_name models_section_name;
    std::vector<std::wstring> architectures;
};

/**
 * @class device_description_line
 * @brief Parsed representation of a device entry in a models section.
 *
 * Example line:
 *   `ASUS System Control Interface v3 = NO_DRV64, ACPI\ASUS2018`
 *
 * Semantics:
 *  - `device_description` — user-visible description (key, left side).
 *  - `install_section` — install section name.
 *  - `hardware_ids` — first item is the HWID; following items are compatible IDs.
 */
class device_description_line
{
public:
    key_name device_description;
    section_name install_section;
    std::vector<std::wstring> hardware_ids;
};

/**
 * @brief Extract all manufacturer lines from `[Manufacturer]`.
 *
 * Uses `inf_file::for_each_line` with case-insensitive matching.
 *
 * @param inf Open INF file wrapper.
 * @return Vector of parsed manufacturers in file order.
 * @throws std::runtime_error if the section is missing or Win32 APIs fail.
 */
std::vector<manufacturer_line> extract_manufacturers(const inf_file& inf)
{
    std::vector<manufacturer_line> result;

    inf.for_each_line(L"Manufacturer", [&result](line&& line)
        {
            manufacturer_line make;
            make.name = key_name{ line.key() };
            if (line.size() > 0)
            {
                make.models_section_name = section_name(std::from_range, line.field_at(0));
            }
            else
            {
                make.models_section_name = make.name;
            }

            if (line.size() > 1)
            {
                make.architectures.reserve(line.size() - 1);
                for (size_t i = 1; i < line.size(); ++i)
                {
                    make.architectures.emplace_back(line.field_at(i));
                }
            }

            result.push_back(std::move(make));
            return enumeration::move_next;
        });

    return result;
}

/**
 * @brief Enumerate all section names present in the INF.
 * @param inf Open INF file wrapper.
 * @return Unordered set of case-insensitive section names.
 */
std::unordered_set<section_name> extract_sections(const inf_file& inf)
{
    std::unordered_set<section_name> result;

    inf.for_each_section([&result](section_name_view raw_name)
        {
            result.emplace(raw_name);
            return enumeration::move_next;
        });

    return result;
}

/**
 * @brief Parse a models section into device-description entries.
 *
 * @param inf Open INF file wrapper.
 * @param models_section_name Name of a models section, with or without
 *        architecture suffix (e.g., `ASUP` or `ASUP.ntamd64.10.0...16299`).
 * @return Vector of device-description lines.
 * @throws std::runtime_error if the section exists but a line is malformed
 *         (e.g., missing install section name).
 */
std::vector<device_description_line> extract_device_descriptions(
    const inf_file& inf,
    section_name_view models_section_name)
{
    std::vector<device_description_line> result;

    inf.for_each_line(models_section_name, [&result](line&& device_entry)
        {
            if (device_entry.size() == 0)
            {
                throw std::runtime_error("install-section-name field is missing");
            }

            device_description_line desc;
            desc.device_description = device_entry.key();
            desc.install_section = section_name(std::from_range, device_entry.field_at(0));

            if (device_entry.size() > 1)
            {
                desc.hardware_ids.reserve(device_entry.size() - 1);
                for (size_t i = 1; i < device_entry.size(); ++i)
                {
                    desc.hardware_ids.emplace_back(device_entry.field_at(i));
                }
            }

            result.push_back(std::move(desc));
            return enumeration::move_next;
        });

    return result;
}
