// ===========================================================================
//  anaconda/export/ExportTypes.hpp — export configuration & result (Month 5).
//
//  Host-testable value types for APK/AAB export. The on-device pipeline is
//  template-based (player.apk + project pack → re-sign → align). Docs in
//  ASSET_PIPELINE_AND_EXPORT.md Part B.
// ===========================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace anaconda::export_cfg {

enum class ExportTarget : int {
    Apk = 0, // sideload/share via template merge
    Aab = 1, // Play Store via bundletool/CI
    Pack = 2, // .anaconda project archive
};

enum class AbiSplit : int {
    Universal = 0,
    Arm64Only = 1,
    ArmV7Only = 2,
};

struct ExportConfig {
    ExportTarget target = ExportTarget::Apk;
    std::string packageName = "com.example.mygame"; // reverse-DNS
    std::string appName = "My Game";
    std::string versionName = "1.0.0";
    int versionCode = 1;
    std::string minSdk = "24";
    AbiSplit abi = AbiSplit::Universal;
    std::string iconPath;       // absolute path to PNG
    std::string keystorePath;   // null = generate debug keystore
    std::string keyAlias = "anaconda";
    std::string keystorePass = "anaconda";
    bool includeAtlas = true;
    bool compressScripts = true; // pre-compile Lua to bytecode
};

struct ExportResult {
    bool success = false;
    std::string outputPath;
    std::string error;
    std::int64_t outputBytes = 0;
    std::string packageName;
};

inline bool isValidPackageName(const std::string& pkg) {
    if (pkg.empty() || pkg.size() > 200) return false;
    if (pkg.find("com.anaconda") == 0) return false; // reserved
    bool expectAlpha = true;
    for (char c : pkg) {
        if (c == '.') {
            if (expectAlpha) return false;
            expectAlpha = true;
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
            expectAlpha = false;
        } else if (c >= 'A' && c <= 'Z') {
            expectAlpha = false;
        } else return false;
    }
    return !expectAlpha;
}

struct ProjectPackInfo {
    std::string packPath;
    std::string projectName;
    std::int64_t packBytes = 0;
    std::vector<std::string> scenes;
    std::vector<std::string> assets;
};

}  // namespace anaconda::export_cfg
