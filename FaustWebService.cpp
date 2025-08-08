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
    
    // For now, return a placeholder
    // TODO: Implement actual compilation using FaustServer
    
    std::stringstream result;
    result << "Compilation service will be implemented. ";
    result << "Platform: " << platform << ", ";
    result << "Architecture: " << architecture << ", ";
    result << "Code length: " << code.length() << " characters";
    
    return result.str();
}

// Implementation of ValidateDSPTool
std::string ValidateDSPTool::execute(const json& arguments) {
    // Extract code from arguments
    std::string code = arguments.value("code", "");
    
    // For basic validation, we can try to compile with faust -lang c -o /dev/null
    // TODO: Create temp file with code
    // TODO: Run faust validation
    
    std::stringstream result;
    result << "Validation service will be implemented. ";
    result << "Code length: " << code.length() << " characters";
    
    return result.str();
}

// Implementation of GetDSPInfoTool
std::string GetDSPInfoTool::execute(const json& arguments) {
    // Extract code from arguments
    std::string code = arguments.value("code", "");
    
    // Can use faust -json to get metadata
    // TODO: Implement analysis
    
    std::stringstream result;
    result << "DSP info service will be implemented. ";
    result << "Code length: " << code.length() << " characters";
    
    return result.str();
}

// Implementation of FaustWebService
void FaustWebService::initialize() {
    // Register ListTargets tool
    fMCPServer->registerTool("list_targets", 
        std::unique_ptr<MCPTool>(new ListTargetsTool(fTargets)));
    
    // Register CompileDSP tool
    fMCPServer->registerTool("compile_dsp",
        std::unique_ptr<MCPTool>(new CompileDSPTool(fFaustServer)));
    
    // Register ValidateDSP tool
    fMCPServer->registerTool("validate_dsp",
        std::unique_ptr<MCPTool>(new ValidateDSPTool()));
    
    // Register GetDSPInfo tool
    fMCPServer->registerTool("get_dsp_info",
        std::unique_ptr<MCPTool>(new GetDSPInfoTool()));
}