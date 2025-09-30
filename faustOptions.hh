#include <filesystem>
#include <string>

namespace fs = std::filesystem;

bool        initializeFaustOptions();
std::string filterFaustOptions(const std::string& options);
std::string readFaustOptions(const fs::path& session_dir);
std::string extractFaustOptions(const fs::path& dsp_file);
