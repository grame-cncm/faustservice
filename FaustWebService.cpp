/************************************************************************
 FAUST Architecture File
 Copyright (C) 2025 GRAME, Centre National de Creation Musicale
 ---------------------------------------------------------------------
 FaustWeb MCP Service implementation
 ************************************************************************/

#include "FaustWebService.h"
#include "server.hh"
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <set>
#include <chrono>
#include <openssl/sha.h>

// Convert JSON targets to readable text format
static std::string JSONTargets2string(const std::string& jsonTargets) {
    std::string result;
    std::string currentPlatform;
    std::string currentValue;
    bool inString = false;
    bool inKey = false;
    bool inArray = false;
    bool firstPlatform = true;
    bool firstArch = true;
    bool escape = false;
    
    for (size_t i = 0; i < jsonTargets.size(); i++) {
        char c = jsonTargets[i];
        
        if (escape) {
            currentValue += c;
            escape = false;
            continue;
        }
        
        if (c == '\\') {
            escape = true;
            continue;
        }
        
        if (c == '"') {
            if (!inString) {
                inString = true;
                currentValue.clear();
                if (!inArray && currentPlatform.empty()) {
                    inKey = true;
                }
            } else {
                inString = false;
                if (inKey) {
                    currentPlatform = currentValue;
                    inKey = false;
                } else if (inArray) {
                    if (!firstArch) result += ", ";
                    result += currentValue;
                    firstArch = false;
                }
            }
        } else if (inString) {
            currentValue += c;
        } else if (c == ':' && !currentPlatform.empty() && !inArray) {
            if (!firstPlatform) result += "; ";
            result += "platform " + currentPlatform + ": architectures ";
            firstPlatform = false;
            firstArch = true;
        } else if (c == '[') {
            inArray = true;
        } else if (c == ']') {
            inArray = false;
            currentPlatform.clear();
        }
    }
    
    return result;
}

// Check if an architecture produces text output
static bool isTextOutput(const std::string& architecture) {
    static std::set<std::string> textArchs = {
        "c", "cpp", "cplusplus", "rust", "julia",
        "csound", "supercollider", "faust", 
        "jsfx", "cmajor", "cmajor-poly", "cmajor-poly-effect", 
        "rnbo", "rnbo-poly", "rnbo-poly-effect",
        "any", "api-android", "api-ios", "api-poly-android", "api-poly-ios",
        "juce", "juce-poly", "smartkeyb-android", "smartkeyb-ios",
        "csv", "mathdoc"
    };
    // Note: wasmjs and wasmjs-poly produce binary archives, not text
    return textArchs.count(architecture) > 0;
}

// Escape string for JSON output (single line)
static std::string escapeForJSON(const std::string& text) {
    std::string result;
    for (char c : text) {
        switch (c) {
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            default:   
                if (c >= 32 && c <= 126) {
                    result += c;
                } else {
                    // Skip other control characters
                }
        }
    }
    return result;
}

// Generate SHA1 from string content
static std::string generateSHA1(const std::string& content) {
    unsigned char obuf[20];
    SHA1(reinterpret_cast<const unsigned char*>(content.c_str()), content.length(), obuf);
    
    // Convert SHA1 key into hexadecimal string
    std::string sha1key;
    for (int i = 0; i < 20; i++) {
        const char* H = "0123456789abcdef";
        sha1key += H[(obuf[i] >> 4)];
        sha1key += H[(obuf[i] & 15)];
    }
    
    return sha1key;
}

// Read entire file content
static std::string readFile(const fs::path& filepath) {
    std::ifstream file(filepath.string());
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Execute system command and capture output
static std::string executeCommand(const std::string& cmd) {
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "Error executing command";
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int status = pclose(pipe);
    if (status != 0) {
        // Command failed, result contains error output
        return result;
    }
    
    return result;
}

// Implementation of ListTargetsTool
std::string ListTargetsTool::execute(const json& arguments) {
    // Simply return the formatted targets
    return JSONTargets2string(fTargets);
}

// Implementation of CompileDSPTool
std::string CompileDSPTool::execute(const json& arguments) {
    // Extract parameters from arguments
    std::string code = arguments.value("code", "");
    std::string platform = arguments.value("platform", "");
    std::string architecture = arguments.value("architecture", "");
    
    // Validate parameters
    if (code.empty()) {
        return "Error: No DSP code provided";
    }
    if (platform.empty()) {
        return "Error: No platform specified";
    }
    if (architecture.empty()) {
        return "Error: No architecture specified";
    }
    
    // Check if Makefile exists
    fs::path makefilePath = fMakefilesDir / platform / ("Makefile." + architecture);
    if (!fs::exists(makefilePath)) {
        return "Error: Invalid platform/architecture combination. Makefile not found: " + makefilePath.string();
    }
    
    // Generate unique session ID
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::string sessionContent = code + platform + architecture + std::to_string(timestamp);
    std::string sha1 = generateSHA1(sessionContent);
    
    // Create session directory
    fs::path sessionDir = fSessionsDir / sha1;
    if (!fs::exists(sessionDir)) {
        fs::create_directories(sessionDir);
    }
    
    // Write DSP code to file
    fs::path dspFile = sessionDir / "process.dsp";
    std::ofstream dspOut(dspFile.string());
    if (!dspOut.is_open()) {
        return "Error: Could not create DSP file";
    }
    dspOut << code;
    dspOut.close();
    
    // Copy Makefile to session directory
    fs::copy_file(makefilePath, sessionDir / "Makefile", fs::copy_option::overwrite_if_exists);
    
    // Execute make command
    std::string makeCmd = "cd " + sessionDir.string() + " && make DESTDIR=" + sha1 + " ARCH=" + architecture + " 2>&1";
    std::string makeOutput = executeCommand(makeCmd);
    
    // Check if compilation succeeded by looking for error patterns
    bool hasError = (makeOutput.find("error") != std::string::npos || 
                    makeOutput.find("Error") != std::string::npos ||
                    makeOutput.find("ERROR") != std::string::npos ||
                    makeOutput.find("fatal") != std::string::npos);
    
    // Also check if any output files were created
    bool hasOutput = false;
    if (fs::exists(sessionDir)) {
        for (fs::directory_iterator it(sessionDir); it != fs::directory_iterator(); ++it) {
            std::string filename = it->path().filename().string();
            // Check for typical output files
            if (filename != "Makefile" && filename != "process.dsp" && 
                filename != ".DS_Store" && filename.find(".o") == std::string::npos) {
                hasOutput = true;
                break;
            }
        }
    }
    
    if (hasError && !hasOutput) {
        // Compilation failed - return error message
        return "Compilation error:\\n" + escapeForJSON(makeOutput);
    }
    
    // Determine if output is text or binary
    if (isTextOutput(architecture)) {
        // Text output - find and read the main output file
        std::string outputContent;
        
        // Try common text output patterns
        std::vector<std::string> patterns = {
            "process.cpp", "process.c", "process.js", "process.rs", 
            "process.jl", "process.cs", "process.java", "process.sc",
            "process.cmajor", "process.rnbo", "process.jsfx",
            sha1 + ".cpp", sha1 + ".c", sha1 + ".js", sha1 + ".rs"
        };
        
        for (const auto& pattern : patterns) {
            fs::path outputFile = sessionDir / pattern;
            if (fs::exists(outputFile)) {
                outputContent = readFile(outputFile);
                if (!outputContent.empty()) {
                    break;
                }
            }
        }
        
        // If no specific file found, look for any text file
        if (outputContent.empty()) {
            for (fs::directory_iterator it(sessionDir); it != fs::directory_iterator(); ++it) {
                std::string ext = it->path().extension().string();
                if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp" ||
                    ext == ".js" || ext == ".rs" || ext == ".jl" || ext == ".java" ||
                    ext == ".cs" || ext == ".sc" || ext == ".cmajor" || ext == ".rnbo") {
                    outputContent = readFile(it->path());
                    if (!outputContent.empty()) {
                        break;
                    }
                }
            }
        }
        
        if (!outputContent.empty()) {
            return escapeForJSON(outputContent);
        } else {
            return "Compilation completed but no output file found.\\n" + escapeForJSON(makeOutput);
        }
    } else {
        // Binary output - check if archive already exists or create it
        fs::path archivePath = sessionDir / (sha1 + ".zip");
        fs::path binaryZip = sessionDir / "binary.zip";
        
        if (fs::exists(binaryZip)) {
            // Some makefiles create binary.zip directly
            fs::rename(binaryZip, archivePath);
        } else if (!fs::exists(archivePath)) {
            // Create archive from output files
            std::string archiveCmd = "cd " + sessionDir.string() + 
                                    " && zip -r " + sha1 + ".zip * -x Makefile process.dsp *.o .DS_Store 2>&1";
            executeCommand(archiveCmd);
        }
        
        // List files in the archive
        std::string listCmd = "cd " + sessionDir.string() + " && unzip -l " + sha1 + ".zip 2>&1 | tail -n +4 | head -n -2";
        std::string fileList = executeCommand(listCmd);
        
        // Format result message
        std::stringstream result;
        result << "Binary compilation successful!\\n\\n";
        result << "Download URL: http://localhost:8888/" << sha1 << "/" << sha1 << ".zip\\n";
        result << "Session: " << sha1 << "\\n\\n";
        result << "Files in archive:\\n" << escapeForJSON(fileList);
        
        return result.str();
    }
}

// Implementation of ValidateDSPTool
std::string ValidateDSPTool::execute(const json& arguments) {
    // Extract code from arguments
    std::string code = arguments.value("code", "");
    
    if (code.empty()) {
        return "Error: No DSP code provided";
    }
    
    // Create temporary file
    std::string tmpFile = "/tmp/validate_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".dsp";
    std::ofstream out(tmpFile);
    out << code;
    out.close();
    
    // Run faust with syntax checking only
    std::string cmd = "faust -lang c -o /dev/null " + tmpFile + " 2>&1";
    std::string output = executeCommand(cmd);
    
    // Clean up
    std::remove(tmpFile.c_str());
    
    // Check result
    if (output.empty() || output.find("error") == std::string::npos) {
        return "DSP code is valid";
    } else {
        return "Validation error:\\n" + escapeForJSON(output);
    }
}

// Implementation of GetDSPInfoTool
std::string GetDSPInfoTool::execute(const json& arguments) {
    // Extract code from arguments
    std::string code = arguments.value("code", "");
    
    if (code.empty()) {
        return "Error: No DSP code provided";
    }
    
    // Create temporary file
    std::string tmpFile = "/tmp/info_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".dsp";
    std::ofstream out(tmpFile);
    out << code;
    out.close();
    
    // Run faust with JSON output to get metadata
    std::string cmd = "faust -json -o /dev/null " + tmpFile + " 2>&1";
    std::string output = executeCommand(cmd);
    
    // Extract JSON file if created
    std::string jsonFile = tmpFile + ".json";
    std::string jsonContent;
    if (fs::exists(jsonFile)) {
        jsonContent = readFile(jsonFile);
        std::remove(jsonFile.c_str());
    }
    
    // Clean up
    std::remove(tmpFile.c_str());
    
    if (!jsonContent.empty()) {
        // Parse and format the JSON info
        try {
            json info = json::parse(jsonContent);
            std::stringstream result;
            
            result << "DSP Information:\\n\\n";
            
            if (info.contains("name")) {
                result << "Name: " << info["name"].get<std::string>() << "\\n";
            }
            if (info.contains("filename")) {
                result << "File: " << info["filename"].get<std::string>() << "\\n";
            }
            if (info.contains("version")) {
                result << "Version: " << info["version"].get<std::string>() << "\\n";
            }
            if (info.contains("compile_options")) {
                result << "Options: " << info["compile_options"].get<std::string>() << "\\n";
            }
            
            result << "\\n";
            
            if (info.contains("inputs")) {
                result << "Inputs: " << info["inputs"] << "\\n";
            }
            if (info.contains("outputs")) {
                result << "Outputs: " << info["outputs"] << "\\n";
            }
            
            if (info.contains("meta") && info["meta"].is_array()) {
                result << "\\nMetadata:\\n";
                for (const auto& meta : info["meta"]) {
                    for (auto it = meta.begin(); it != meta.end(); ++it) {
                        result << "  " << it.key() << ": " << it.value() << "\\n";
                    }
                }
            }
            
            if (info.contains("ui") && info["ui"].is_array() && !info["ui"].empty()) {
                result << "\\nUI Elements: " << info["ui"].size() << " items\\n";
            }
            
            return result.str();
        } catch (const std::exception& e) {
            return "Error parsing DSP metadata: " + std::string(e.what());
        }
    } else if (output.find("error") != std::string::npos) {
        return "Error analyzing DSP:\\n" + escapeForJSON(output);
    } else {
        return "No metadata available for this DSP";
    }
}

// Implementation of FaustWebService
void FaustWebService::initialize() {
    // Register ListTargets tool
    fMCPServer->registerTool("list_targets", 
        std::unique_ptr<MCPTool>(new ListTargetsTool(fTargets)));
    
    // Register CompileDSP tool
    fMCPServer->registerTool("compile_dsp",
        std::unique_ptr<MCPTool>(new CompileDSPTool(fFaustServer, fSessionsDir, fMakefilesDir)));
    
    // Register ValidateDSP tool
    fMCPServer->registerTool("validate_dsp",
        std::unique_ptr<MCPTool>(new ValidateDSPTool()));
    
    // Register GetDSPInfo tool
    fMCPServer->registerTool("get_dsp_info",
        std::unique_ptr<MCPTool>(new GetDSPInfoTool()));
}