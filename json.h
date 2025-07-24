/**
 * @file json.h
 * @brief ADL specializations for `nlohmann::json` to serialize report types.
 *
 * Only `to_json` is provided; `from_json` is intentionally deleted to signal
 * one-way serialization semantics.
 */

template <>
struct nlohmann::adl_serializer<model> {
    static void to_json(json& j, const model& m) {
        j = json{
            {"description", m.description},
            {"hardware_ids", m.hardware_ids},
            {"architectures", m.architectures}
        };
    }

    static void from_json(const nlohmann::json&, model&) = delete;
};

template <>
struct nlohmann::adl_serializer<manufacturer> {
    static void to_json(json& j, const manufacturer& m) {
        j = json{
            {"name", m.name},
            {"devices", m.devices}
        };
    }

    static void from_json(const nlohmann::json&, manufacturer&) = delete;
};
