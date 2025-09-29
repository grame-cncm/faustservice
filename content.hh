#include <filesystem>
#include <string>

namespace fs = std::filesystem;

bool        isFaustFile(const fs::path& f);
bool        isAudioFile(const fs::path& f);
void        copyFaustOrAudioFiles(const fs::path& src, const fs::path& dst);
bool        isSecureFilename(const std::string& filename);
std::string generateFileSHA1(fs::path filepath);
void        create_basic_session(fs::path srcdir, fs::path sha1path, fs::path makefile_directory);
bool        create_target_directory(fs::path srcdir, fs::path sha1path, fs::path makefile_directory,
                                    const std::string& platform, const std::string& architecture);
bool        create_svg_zip(const fs::path& svg_dir, const fs::path& zip_path);
bool        create_webapp_zip(const fs::path& webapp_dir, const fs::path& zip_path);
