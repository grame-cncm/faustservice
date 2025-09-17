/************************************************************************
 FAUST Architecture File
 Copyright (C) 2017 GRAME, Centre National de Creation Musicale
 ---------------------------------------------------------------------
 This Architecture section is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 3 of
 the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; If not, see <http://www.gnu.org/licenses/>.

 EXCEPTION : As a special exception, you may create a larger work
 that contains this FAUST architecture section and distribute
 that work under terms of your choice, so long as this FAUST
 architecture section is not modified.

 ************************************************************************
 ************************************************************************/

#include "utilities.hh"
#include <openssl/sha.h>
#include <archive.h>
#include <archive_entry.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static std::string toHexString(const unsigned char* data, size_t length)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

static std::string normalizePath(const std::string& path)
{
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    // Remove leading ./
    if (normalized.size() >= 2 && normalized[0] == '.' && normalized[1] == '/') {
        normalized = normalized.substr(2);
    }

    // Remove leading /
    if (!normalized.empty() && normalized[0] == '/') {
        normalized = normalized.substr(1);
    }

    return normalized;
}

static bool isZipFile(const char* data, size_t size)
{
    // Check ZIP magic number (PK\x03\x04 or PK\x05\x06 for empty archives)
    if (size < 4) return false;
    return (data[0] == 'P' && data[1] == 'K' &&
            ((data[2] == 0x03 && data[3] == 0x04) ||
             (data[2] == 0x05 && data[3] == 0x06)));
}

static std::map<std::string, std::string> extractZipToMemory(const char* data, size_t size)
{
    std::map<std::string, std::string> files;

    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);

    if (archive_read_open_memory(a, data, size) != ARCHIVE_OK) {
        archive_read_free(a);
        return files;
    }

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char* pathname = archive_entry_pathname(entry);

        // Skip directories
        if (archive_entry_filetype(entry) != AE_IFREG) {
            archive_read_data_skip(a);
            continue;
        }

        // Read file content
        size_t file_size = archive_entry_size(entry);
        std::string content(file_size, '\0');
        archive_read_data(a, &content[0], file_size);

        // Store with normalized path
        files[normalizePath(pathname)] = content;
    }

    archive_read_free(a);
    return files;
}

std::string calculateContentSHA(const char* data, size_t size)
{
    if (isZipFile(data, size)) {
        // Extract and sort files from ZIP
        std::map<std::string, std::string> files = extractZipToMemory(data, size);

        // Calculate SHA1 of normalized content
        SHA_CTX ctx;
        SHA1_Init(&ctx);

        // Map is already sorted by key (file path)
        for (const auto& [path, content] : files) {
            // Hash the normalized path
            SHA1_Update(&ctx, path.c_str(), path.size());
            // Hash a separator to avoid collisions
            SHA1_Update(&ctx, "\0", 1);
            // Hash the file content
            SHA1_Update(&ctx, content.c_str(), content.size());
        }

        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1_Final(hash, &ctx);
        return toHexString(hash, SHA_DIGEST_LENGTH);

    } else {
        // Single file: SHA1 of content
        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char*>(data), size, hash);
        return toHexString(hash, SHA_DIGEST_LENGTH);
    }
}

std::string calculateFileSHA(const std::string& filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    // Read file content
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content(size, '\0');
    file.read(&content[0], size);
    file.close();

    return calculateContentSHA(content.c_str(), size);
}