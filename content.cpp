#include "content.hh"

#include <archive.h>
#include <archive_entry.h>
#include <openssl/sha.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>

extern int gVerbosity;

/*
 * True if it is a .dsp or a .lib source file
 */

bool isFaustFile(const fs::path& f)
{
    fs::path x = f.extension();
    bool     a = (x == ".dsp") || (x == ".lib");
    return a;
}

/*
 * True if it is a .wav or .flac audio file
 */

bool isAudioFile(const fs::path& f)
{
    fs::path x = f.extension();
    bool     a = (x == ".wav") || (x == ".flac");
    return a;
}

/*
 * Copy all Faust source files and additional resources (libraries and audio files) from src
 * directory to destination directory
 */

void copyFaustOrAudioFiles(const fs::path& src, const fs::path& dst)
{
    assert(is_directory(src));
    assert(is_directory(dst));
    for (const auto& entry : fs::directory_iterator(src)) {
        if (isFaustFile(entry.path())) {
            fs::copy_file(entry.path(), dst / entry.path().filename());
        } else if (isAudioFile(entry.path())) {
            fs::copy_file(entry.path(), dst / entry.path().filename());
        }
    }
}

/**
 * Validate filename for security
 * Only allows: alphanumeric, dot, dash
 * Rejects: path traversal, shell metacharacters, Unicode, spaces
 * Maximum length: 100 characters
 * Required extensions: .dsp or .zip
 */
bool isSecureFilename(const std::string& filename)
{
    // Check length limits
    if (filename.empty() || filename.length() > 100) {
        if (gVerbosity >= 2) {
            std::cerr << "Filename length invalid: " << filename.length() << " characters"
                      << std::endl;
        }
        return false;
    }

    // Check for hidden files (starting with .)
    if (filename[0] == '.') {
        if (gVerbosity >= 2) {
            std::cerr << "Hidden files not allowed: " << filename << std::endl;
        }
        return false;
    }

    // Check valid extensions
    bool has_valid_ext = false;
    if (filename.length() >= 4) {
        std::string ext = filename.substr(filename.length() - 4);
        if (ext == ".dsp" || ext == ".lib" || ext == ".zip" || ext == ".wav") {
            has_valid_ext = true;
        }
    }
    if (!has_valid_ext) {
        if (gVerbosity >= 2) {
            std::cerr << "Invalid file extension: " << filename << std::endl;
        }
        return false;
    }

    // Check characters - only alphanumeric, dot, dash allowed
    for (char c : filename) {
        if (!std::isalnum(c) && c != '_' && c != '.' && c != '-') {
            if (gVerbosity >= 1) {
                std::cerr << "Invalid character in filename '" << filename << "': '" << c
                          << "' (ASCII " << (int)c << ")" << std::endl;
            }
            return false;
        }
    }

    // Additional security checks
    if (filename.find("..") != std::string::npos) {
        if (gVerbosity >= 1) {
            std::cerr << "Path traversal attempt in filename: " << filename << std::endl;
        }
        return false;
    }

    if (gVerbosity >= 2) {
        std::cerr << "Filename validation passed: " << filename << std::endl;
    }
    return true;
}

/**
 * Generates an SHA-1 key for a file.
 */

std::string generateFileSHA1(fs::path filepath)
{
    // read file content
    std::ifstream myFile(filepath.string().c_str(), std::ios::in | std::ios::binary);
    myFile.seekg(0, std::ios::end);
    int length = myFile.tellg();
    myFile.seekg(0, std::ios::beg);

    // char content[length];
    char* content = (char*)malloc(length);
    if (content == 0) {
        return "malloc-error";
    }
    myFile.read(content, length);
    myFile.close();

    // compute SHA1 key
    unsigned char obuf[20];
    SHA1((const unsigned char*)content, length, obuf);

    // convert SHA1 key into hexadecimal string
    std::string sha1key;
    for (int i = 0; i < 20; i++) {
        const char* H  = "0123456789ABCDEF";
        char        c1 = H[(obuf[i] >> 4)];
        char        c2 = H[(obuf[i] & 15)];
        sha1key += tolower(c1);
        sha1key += tolower(c2);
    }
    free(content);

    return sha1key;
}

/*
 * Creates an arboreal structure in root with the appropriate makefiles.
 */

// Create a specific target directory on demand
bool create_target_directory(fs::path srcdir, fs::path sha1path, fs::path makefile_directory,
                             const std::string& platform, const std::string& architecture)
{
    if (gVerbosity >= 2) {
        std::cerr << "ENTER create_target_directory(" << sha1path << ", platform=" << platform
                  << ", arch=" << architecture << ")" << std::endl;
    }

    // First, ensure the base Makefile.none is copied for non-architecture-specific targets
    // Only if it doesn't already exist
    fs::path base_makefile        = fs::path(makefile_directory) / "Makefile.none";
    fs::path target_base_makefile = sha1path / "Makefile";
    if (fs::exists(base_makefile) && !fs::exists(target_base_makefile)) {
        fs::copy_file(base_makefile, target_base_makefile);
        copyFaustOrAudioFiles(srcdir, sha1path);
    }

    // Look for the specific makefile
    fs::path platform_dir = makefile_directory / platform;
    if (!fs::exists(platform_dir) || !fs::is_directory(platform_dir)) {
        if (gVerbosity >= 1) {
            std::cerr << "Platform directory not found: " << platform_dir << std::endl;
        }
        return false;
    }

    fs::path makefile_path = platform_dir / ("Makefile." + architecture);
    if (!fs::exists(makefile_path)) {
        if (gVerbosity >= 1) {
            std::cerr << "Makefile not found: " << makefile_path << std::endl;
        }
        return false;
    }

    // Create the target directory
    fs::path target_dir = sha1path / "targets" / platform / architecture;
    try {
        fs::create_directories(target_dir);
        fs::copy_file(makefile_path, target_dir / "Makefile", fs::copy_options::overwrite_existing);
        copyFaustOrAudioFiles(srcdir / "sourcecode", target_dir);

        if (gVerbosity >= 2) {
            std::cerr << "Created target directory: " << target_dir << std::endl;
        }
        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating target directory " << target_dir << ": " << e.what()
                  << std::endl;
        return false;
    }
}

// Create only basic session structure (no platform-specific directories)
void create_basic_session(fs::path srcdir, fs::path sha1path, fs::path makefile_directory)
{
    if (gVerbosity >= 2) {
        std::cerr << "ENTER create_basic_session(" << sha1path << ", " << makefile_directory << ")"
                  << std::endl;
    }

    // Only copy makefile.none to handle non-architecture-specific targets like mdoc.zip, etc
    fs::path base_makefile = fs::path(makefile_directory) / "Makefile.none";
    if (fs::exists(base_makefile)) {
        fs::copy_file(base_makefile, sha1path / "Makefile", fs::copy_options::overwrite_existing);
        copyFaustOrAudioFiles(srcdir, sha1path);
    }

    if (gVerbosity >= 2) {
        std::cerr << "Created basic session structure in: " << sha1path << std::endl;
    }
}

/*
 * Creates a ZIP file containing all SVG files from a directory
 */
bool create_svg_zip(const fs::path& svg_dir, const fs::path& zip_path)
{
    if (gVerbosity >= 2) {
        std::cerr << "Creating SVG ZIP: " << svg_dir << " -> " << zip_path << std::endl;
    }

    struct archive* archive = archive_write_new();
    if (!archive) {
        std::cerr << "Error: Could not create archive" << std::endl;
        return false;
    }

    // Set ZIP format
    archive_write_set_format_zip(archive);
    archive_write_add_filter_none(archive);

    // Open the output file
    if (archive_write_open_filename(archive, zip_path.string().c_str()) != ARCHIVE_OK) {
        std::cerr << "Error: Could not open ZIP file: " << archive_error_string(archive)
                  << std::endl;
        archive_write_free(archive);
        return false;
    }

    try {
        // Iterate through all SVG files in the directory
        for (const auto& entry : fs::directory_iterator(svg_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".svg") {
                fs::path    svg_file = entry.path();
                std::string filename = svg_file.filename().string();

                // Read the SVG file
                std::ifstream file(svg_file, std::ios::binary);
                if (!file) {
                    std::cerr << "Warning: Could not read SVG file: " << svg_file << std::endl;
                    continue;
                }

                // Get file size
                file.seekg(0, std::ios::end);
                size_t file_size = file.tellg();
                file.seekg(0, std::ios::beg);

                // Read file content
                std::vector<char> content(file_size);
                file.read(content.data(), file_size);
                file.close();

                // Create archive entry
                struct archive_entry* entry_hdr = archive_entry_new();
                archive_entry_set_pathname(entry_hdr, filename.c_str());
                archive_entry_set_size(entry_hdr, file_size);
                archive_entry_set_filetype(entry_hdr, AE_IFREG);
                archive_entry_set_perm(entry_hdr, 0644);

                // Write header and data
                if (archive_write_header(archive, entry_hdr) == ARCHIVE_OK) {
                    archive_write_data(archive, content.data(), file_size);
                }

                archive_entry_free(entry_hdr);

                if (gVerbosity >= 2) {
                    std::cerr << "Added to ZIP: " << filename << " (" << file_size << " bytes)"
                              << std::endl;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error accessing SVG directory: " << e.what() << std::endl;
        archive_write_free(archive);
        return false;
    }

    // Close the archive
    archive_write_close(archive);
    archive_write_free(archive);

    if (gVerbosity >= 2) {
        std::cerr << "SVG ZIP created successfully: " << zip_path << std::endl;
    }

    return true;
}

/*
 * Creates a ZIP file containing all files from a webapp directory recursively
 */
bool create_webapp_zip(const fs::path& webapp_dir, const fs::path& zip_path)
{
    if (gVerbosity >= 2) {
        std::cerr << "Creating webapp ZIP: " << webapp_dir << " -> " << zip_path << std::endl;
    }

    struct archive* archive = archive_write_new();
    if (!archive) {
        std::cerr << "Error: Could not create archive" << std::endl;
        return false;
    }

    // Set ZIP format
    archive_write_set_format_zip(archive);
    archive_write_add_filter_none(archive);

    // Open the output file
    if (archive_write_open_filename(archive, zip_path.string().c_str()) != ARCHIVE_OK) {
        std::cerr << "Error: Could not open ZIP file: " << archive_error_string(archive)
                  << std::endl;
        archive_write_free(archive);
        return false;
    }

    try {
        // Recursively iterate through all files in the webapp directory
        for (const auto& entry : fs::recursive_directory_iterator(webapp_dir)) {
            if (entry.is_regular_file()) {
                fs::path file_path = entry.path();

                // Get relative path from webapp_dir for ZIP entry name
                fs::path    relative_path  = fs::relative(file_path, webapp_dir);
                std::string zip_entry_name = relative_path.string();

                if (gVerbosity >= 2) {
                    std::cerr << "Adding to webapp ZIP: " << zip_entry_name << std::endl;
                }

                // Create archive entry
                struct archive_entry* entry_archive = archive_entry_new();
                archive_entry_set_pathname(entry_archive, zip_entry_name.c_str());
                archive_entry_set_filetype(entry_archive, AE_IFREG);
                archive_entry_set_perm(entry_archive, 0644);

                // Get file size
                std::uintmax_t file_size = fs::file_size(file_path);
                archive_entry_set_size(entry_archive, file_size);

                // Write header
                if (archive_write_header(archive, entry_archive) != ARCHIVE_OK) {
                    std::cerr << "Error writing header for " << zip_entry_name << ": "
                              << archive_error_string(archive) << std::endl;
                    archive_entry_free(entry_archive);
                    continue;
                }

                // Read and write file content
                std::ifstream file(file_path, std::ios::binary);
                if (file.is_open()) {
                    char buffer[8192];
                    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
                        ssize_t bytes_written = archive_write_data(archive, buffer, file.gcount());
                        if (bytes_written < 0) {
                            std::cerr << "Error writing data for " << zip_entry_name << ": "
                                      << archive_error_string(archive) << std::endl;
                            break;
                        }
                    }
                    file.close();
                } else {
                    std::cerr << "Error: Could not open file " << file_path << std::endl;
                }

                archive_entry_free(entry_archive);
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error while creating webapp ZIP: " << e.what() << std::endl;
        archive_write_close(archive);
        archive_write_free(archive);
        return false;
    }

    archive_write_close(archive);
    archive_write_free(archive);

    if (gVerbosity >= 2) {
        std::cerr << "Webapp ZIP created successfully: " << zip_path << std::endl;
    }

    return true;
}