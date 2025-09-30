#include "faustOptions.hh"

#include <archive.h>
#include <archive_entry.h>
#include <openssl/sha.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

extern int gVerbosity;

// Global sets for intelligent option filtering (populated at startup)
static bool                  g_options_initialized = false;
static std::set<std::string> g_code_generation_options;
static std::set<std::string> g_safe_enum_options;
static std::set<std::string> g_safe_numeric_options;

/**
 * Initialize Faust options by parsing 'faust -h' output
 * Called once at server startup for future-proof option filtering
 */
bool initializeFaustOptions()
{
    if (g_options_initialized) {
        return true;
    }

    if (gVerbosity >= 2) {
        std::cerr << "Initializing Faust options from faust -h..." << std::endl;
    }

    // Run faust -h and capture output
    FILE* faust_help = popen("faust -h 2>/dev/null", "r");
    if (!faust_help) {
        std::cerr << "Warning: Cannot run 'faust -h' for option initialization" << std::endl;
        return false;
    }

    std::string line;
    char        buffer[1024];
    bool        in_code_generation_section = false;

    while (fgets(buffer, sizeof(buffer), faust_help)) {
        line = buffer;

        // Detect Code generation options section
        if (line.find("Code generation options:") != std::string::npos) {
            in_code_generation_section = true;
            continue;
        }

        // Stop when we hit the next section
        if (in_code_generation_section && line.find("options:") != std::string::npos &&
            line.find("Code generation") == std::string::npos) {
            in_code_generation_section = false;
        }

        // Parse options in Code generation section
        if (in_code_generation_section && line.find("  -") == 0) {
            // Extract option name (first word after spaces)
            std::istringstream iss(line);
            std::string        token;
            iss >> token;  // Skip leading spaces, get option

            if (!token.empty() && token[0] == '-') {
                // Handle options with parameters
                if (line.find(" <n>") != std::string::npos ||
                    line.find(" <sec>") != std::string::npos ||
                    line.find(" <file>") != std::string::npos) {
                    g_safe_numeric_options.insert(token);
                } else if (line.find(" <lang>") != std::string::npos) {
                    g_safe_enum_options.insert(token);
                } else if (line.find(" <name>") != std::string::npos) {
                    // These are potentially dangerous (class names, etc.)
                    // We'll handle them specially
                } else {
                    // Simple flag options - safe
                    g_code_generation_options.insert(token);
                }

                // Also handle long versions
                if (line.find("--") != std::string::npos) {
                    size_t long_start = line.find("--");
                    size_t long_end   = line.find_first_of(" \t", long_start);
                    if (long_end != std::string::npos) {
                        std::string long_option = line.substr(long_start, long_end - long_start);
                        if (line.find(" <n>") != std::string::npos ||
                            line.find(" <sec>") != std::string::npos ||
                            line.find(" <file>") != std::string::npos) {
                            g_safe_numeric_options.insert(long_option);
                        } else if (line.find(" <lang>") != std::string::npos) {
                            g_safe_enum_options.insert(long_option);
                        } else if (line.find(" <name>") == std::string::npos) {
                            g_code_generation_options.insert(long_option);
                        }
                    }
                }
            }
        }
    }

    pclose(faust_help);

    if (gVerbosity >= 2) {
        std::cerr << "Initialized " << g_code_generation_options.size()
                  << " code generation options, " << g_safe_numeric_options.size()
                  << " numeric options, " << g_safe_enum_options.size() << " enum options"
                  << std::endl;
    }

    g_options_initialized = true;
    return true;
}

/**
 * Intelligent Faust option filtering - future-proof and secure
 * Level 1: Block all shell injection attempts
 * Level 2: Allow only code generation options (parsed from faust -h)
 */
std::string filterFaustOptions(const std::string& options)
{
    // Initialize options from faust -h if not done yet
    if (!initializeFaustOptions()) {
        if (gVerbosity >= 1) {
            std::cerr << "Warning: Faust options not initialized, using fallback filtering"
                      << std::endl;
        }
    }

    if (gVerbosity >= 2) {
        std::cerr << "Intelligent filtering of Faust options: '" << options << "'" << std::endl;
    }

    std::istringstream       iss(options);
    std::string              token;
    std::vector<std::string> filtered_options;

    while (iss >> token) {
        // LEVEL 1: Check for shell injection in the token itself
        if (token.find(';') != std::string::npos || token.find('|') != std::string::npos ||
            token.find('&') != std::string::npos || token.find('`') != std::string::npos ||
            token.find('$') != std::string::npos || token.find('(') != std::string::npos ||
            token.find(')') != std::string::npos || token.find('<') != std::string::npos ||
            token.find('>') != std::string::npos || token.find('"') != std::string::npos ||
            token.find('\'') != std::string::npos || token.find('\\') != std::string::npos) {
            if (gVerbosity >= 1) {
                std::cerr << "SECURITY: Shell metacharacters detected in option: " << token
                          << std::endl;
            }
            continue;  // Skip this dangerous token
        }

        // LEVEL 2: Check if it's a safe code generation option
        if (g_code_generation_options.count(token)) {
            // Simple flag option - safe to include
            filtered_options.push_back(token);
            if (gVerbosity >= 2) {
                std::cerr << "Accepted safe flag option: " << token << std::endl;
            }

        } else if (g_safe_numeric_options.count(token)) {
            // Numeric parameter option - validate parameter
            filtered_options.push_back(token);

            std::string param;
            if (iss >> param) {
                // LEVEL 1: Check parameter for shell injection
                if (param.find(';') != std::string::npos || param.find('|') != std::string::npos ||
                    param.find('&') != std::string::npos || param.find('`') != std::string::npos ||
                    param.find('$') != std::string::npos || param.find('(') != std::string::npos) {
                    if (gVerbosity >= 1) {
                        std::cerr << "SECURITY: Dangerous parameter for " << token << ": " << param
                                  << std::endl;
                    }
                    filtered_options.pop_back();  // Remove the option too
                    continue;
                }

                // Validate numeric parameter
                bool valid_number = true;
                for (char c : param) {
                    if (!std::isdigit(c) && c != '.' && c != '-') {
                        valid_number = false;
                        break;
                    }
                }

                if (valid_number && param.length() <= 20) {
                    filtered_options.push_back(param);
                    if (gVerbosity >= 2) {
                        std::cerr << "Accepted numeric option: " << token << " " << param
                                  << std::endl;
                    }
                } else {
                    if (gVerbosity >= 1) {
                        std::cerr << "Invalid numeric parameter filtered: " << param << std::endl;
                    }
                    filtered_options.pop_back();  // Remove the option too
                }
            }

        } else if (g_safe_enum_options.count(token)) {
            // Enum parameter option (like -lang) - validate parameter
            filtered_options.push_back(token);

            std::string param;
            if (iss >> param) {
                // LEVEL 1: Check parameter for shell injection
                if (param.find(';') != std::string::npos || param.find('|') != std::string::npos ||
                    param.find('&') != std::string::npos || param.find('`') != std::string::npos ||
                    param.find('$') != std::string::npos) {
                    if (gVerbosity >= 1) {
                        std::cerr << "SECURITY: Dangerous parameter for " << token << ": " << param
                                  << std::endl;
                    }
                    filtered_options.pop_back();  // Remove the option too
                    continue;
                }

                // Validate enum parameter (alphanumeric only)
                bool valid_enum = true;
                for (char c : param) {
                    if (!std::isalnum(c) && c != '-' && c != '_') {
                        valid_enum = false;
                        break;
                    }
                }

                if (valid_enum && param.length() <= 50) {
                    filtered_options.push_back(param);
                    if (gVerbosity >= 2) {
                        std::cerr << "Accepted enum option: " << token << " " << param << std::endl;
                    }
                } else {
                    if (gVerbosity >= 1) {
                        std::cerr << "Invalid enum parameter filtered: " << param << std::endl;
                    }
                    filtered_options.pop_back();  // Remove the option too
                }
            }

        } else {
            // Unknown option - reject for security
            if (gVerbosity >= 2) {
                std::cerr << "Rejected unknown/unsafe option: " << token << std::endl;
            }
        }
    }

    // Join filtered options back into a string
    std::ostringstream result;
    for (size_t i = 0; i < filtered_options.size(); ++i) {
        if (i > 0) {
            result << " ";
        }
        result << filtered_options[i];
    }

    std::string final_result = result.str();
    if (gVerbosity >= 1) {
        std::cerr << "Intelligent filtering result: '" << final_result << "'" << std::endl;
    }

    return final_result;
}

std::string readFaustOptions(const fs::path& session_dir)
{
    fs::path options_file = session_dir / "faustoptions.txt";
    if (fs::exists(options_file)) {
        std::ifstream file(options_file);
        std::string   options;
        std::getline(file, options);
        if (!options.empty()) {
            if (gVerbosity >= 2) {
                std::cout << "Using faustoptions: '" << options << "'" << std::endl;
            }
            return options;
        }
    }
    return "";
}

/**
 * Extract Faust options from DSP source code
 * Looks for: declare faustoptions "options";
 * Enhanced security: limits file size, validates content, prevents malicious patterns
 */
std::string extractFaustOptions(const fs::path& dsp_file)
{
    // Security check: limit file size to prevent DoS
    const size_t MAX_FILE_SIZE = 1024 * 1024;  // 1MB max

    std::ifstream file(dsp_file, std::ios::binary);
    if (!file.is_open()) {
        if (gVerbosity >= 2) {
            std::cerr << "Cannot open DSP file for options extraction: " << dsp_file << std::endl;
        }
        return "";
    }

    // Check file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    if (file_size > MAX_FILE_SIZE) {
        if (gVerbosity >= 1) {
            std::cerr << "DSP file too large for options extraction: " << file_size << " bytes"
                      << std::endl;
        }
        return "";
    }
    file.seekg(0, std::ios::beg);

    std::string  line;
    size_t       line_count         = 0;
    const size_t MAX_LINES_TO_CHECK = 100;  // Only check first 100 lines for performance

    // More restrictive regex pattern to prevent injection
    std::regex  pattern(R"###(^\s*declare\s+faustoptions\s+"([^"]{0,500})"\s*;\s*$)###");
    std::smatch match;

    while (std::getline(file, line) && line_count < MAX_LINES_TO_CHECK) {
        line_count++;

        // Security: skip very long lines that could be malicious
        if (line.length() > 1000) {
            continue;
        }

        // Security: check for suspicious patterns in the line
        if (line.find("system(") != std::string::npos || line.find("popen(") != std::string::npos ||
            line.find("exec") != std::string::npos || line.find("sh -c") != std::string::npos) {
            if (gVerbosity >= 1) {
                std::cerr << "Suspicious pattern detected in DSP file, skipping line" << std::endl;
            }
            continue;
        }

        if (std::regex_match(line, match, pattern)) {
            std::string options = match[1].str();

            // LEVEL 1 SECURITY: Character-level filtering - keep only safe characters
            std::string safe_options;
            safe_options.reserve(options.length());

            for (char c : options) {
                // Allow only: alphanumeric, dot, space, dash, double quote
                if (std::isalnum(c) || c == '.' || c == ' ' || c == '-' || c == '"') {
                    safe_options += c;
                } else {
                    if (gVerbosity >= 2) {
                        std::cerr << "Filtered dangerous character: '" << c << "' (ASCII " << (int)c
                                  << ")" << std::endl;
                    }
                }
            }

            if (gVerbosity >= 2) {
                std::cerr << "Character-filtered faustoptions: '" << safe_options << "'"
                          << std::endl;
            }
            return safe_options;
        }
    }

    if (gVerbosity >= 2) {
        std::cerr << "No valid faustoptions declaration found in first " << line_count << " lines"
                  << std::endl;
    }
    return "";
}
