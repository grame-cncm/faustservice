#include <filesystem>

namespace fs = std::filesystem;
bool isFaustFile(const fs::path& f);
bool isAudioFile(const fs::path& f);
void copyFaustOrAudioFiles(const fs::path& src, const fs::path& dst);
