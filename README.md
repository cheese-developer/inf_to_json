# Windows Drivers INF Files Inspector

Converts Windows driver INF files into clean, structured JSON.

This tool parses INF files to extract device models, hardware IDs, and target architectures, organizing the output by manufacturer for inspection or automation.

## Usage

`inf_to_json <path_to_driver_file.inf>`

**Output:** JSON grouped by manufacturer, then by models and hardware IDs, listing target architectures.

Example output:

```json
[
  {
    "devices": [
      {
        "architectures": [
          "NTamd64"
        ],
        "description": "ACPI Devices driver",
        "hardware_ids": [
          "*ACPI0013"
        ]
      }
    ],
    "name": "(Standard system devices)"
  }
]
```

## Why This Exists

Windows driver packages use INF files to declare what devices they support. But:
- Windows Plug and Play API only works for already connected devices.
- Tools like WMIC and PowerShell offer no easy way to peek inside standalone .inf files.

That’s a gap — especially when analyzing:
- Drivers downloaded directly from OEM websites
- Packages in the DriverStore
- Preinstalled software on prebuilts or laptops

This tool bridges that gap. Use it to:
- Audit OEM driver packages before installation
- Prune unneeded drivers from custom Windows builds
- Analyze and de-bloat vendor setups
- Create flexible, selective setup pipelines

## Build

> **Platform:** Windows x64 only (uses Win32 **SetupAPI**).

### Prerequisites

* **Visual Studio 2022** with C++ toolset (MSVC)
* **CMake 3.28+**
* **Ninja**
* **Vcpkg** (manifest mode) with `VCPKG_ROOT` set
* Internet access to fetch **nlohmann/json**

### Configure & build

```powershell
# from the repo root
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug
```

The executable will appear under:

```
out/build/windows-x64-debug/inf_to_json.exe
```

### Running

When launched without parameters, prints usage info.

To inspect an INF file:

```powershell
.\inf_to_json.exe "C:\Path\To\driver.inf"
```

It prints formatted JSON to stdout. Redirect to a file if desired:

```powershell
.\inf_to_json.exe driver.inf > report.json
```

The process returns `0` (zero) if the report is generated successfully. Otherwise, a non-zero value is returned.
JSON in format `{ "error" : "<error text" }` is displayed in case of an error, except for cases of out of memory and other critical runtime errors. Error code is nonzero in these cases though.

## Project layout

```
.
├── setup_api.cppm          # C++ module: thin Win32 SetupAPI wrappers + traits + UTF-8 conversion
├── reader.h                # High-level extraction: manufacturers, sections, device descriptions
├── report.h                # Correlation + report assembly
├── json.h                  # nlohmann::json serializers
├── main.cpp                # CLI entry point
├── CMakeLists.txt          # Targets + C++23 modules file set
├── CMakePresets.json       # Windows presets (Windows is required to build)
├── vcpkg.json              # Dependencies (nlohmann-json)
└── vcpkg-configuration.json# Registry & baseline pin
```

---

## Key design choices

* **Keep Win32 in one place.** `setup_api.cppm` isolates `windows.h`/`setupapi.h` and returns safe C++ types (`std::basic_string_view`, custom traits). Downstream code stays clean and testable.
* **Enumerator style API.** `for_each_section` / `for_each_line` wrap the `SetupFind*` pattern with clear error handling, exposing a minimal `line` object whose fields are lazily fetched. This mirrors how SetupAPI iterates `INFCONTEXT`.
* **Case-insensitive containers.** `section_name`, `key_name`, and their `*_view` aliases use the custom traits and dedicated hash so lookups match how INF parsing works in Windows.
* **Ordinal semantics for safety.** Cultural collation is not appropriate for identifiers like section names and hardware IDs. The code uses ordinal‑style folding and comparisons, in line with Microsoft guidance to prefer ordinal for non‑linguistic data.
* **Strict conversion to UTF‑8.** Fails fast on malformed input.

---

## Limitations & notes

* **Windows‑only.** Uses SetupAPI and Win32 casing APIs.
* **No locale-aware collation.** By design; identifiers are matched with ordinal semantics. Consider this if you plan to search human‑readable descriptions linguistically.
* **Error handling.** The tool surfaces Windows errors as C++ exceptions with concise messages. For production pipelines, you may want richer diagnostics (file/section/line location).
* **Not a full INF validator.** It trusts SetupAPI for expansion and syntax; it doesn’t perform independent schema validation.
* Returns error on some non-driver INF files, line `errata.inf`.

## Roadmap

* Tests with sample INFs

## License
This project is licensed under the MIT License.
