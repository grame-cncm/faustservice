/************************************************************************
 FAUST Architecture File
 Copyright (C) 2025 GRAME, Centre National de Creation Musicale
 ---------------------------------------------------------------------
 FaustWeb MCP Service - Provides Faust compilation tools via MCP
 ************************************************************************/

#pragma once

#include "MCPServer.h"
#include "json.hpp"
#include <string>
#include <memory>
#include <boost/filesystem.hpp>

using json = nlohmann::json;
namespace fs = boost::filesystem;

// Forward declaration
class FaustServer;

/**
 * Tool to list available compilation targets
 */
class ListTargetsTool : public MCPTool {
private:
    std::string fTargets;  // JSON string with targets
    
public:
    ListTargetsTool(const std::string& targets) : fTargets(targets) {}
    
    std::string execute(const json& arguments) override;
    std::string getDescription() const override {
        return "List all available Faust compilation targets grouped by platform";
    }
};

/**
 * Tool to compile a Faust DSP code
 */
class CompileDSPTool : public MCPTool {
private:
    FaustServer* fServer;
    fs::path fSessionsDir;
    fs::path fMakefilesDir;
    
public:
    CompileDSPTool(FaustServer* server, const fs::path& sessionsDir, const fs::path& makefilesDir) 
        : fServer(server), fSessionsDir(sessionsDir), fMakefilesDir(makefilesDir) {}
    
    std::string execute(const json& arguments) override;
    std::string getDescription() const override {
        return "Compile Faust DSP code to various targets";
    }
    
    json getInputSchema() const override {
        return json{
            {"type", "object"},
            {"properties", {
                {"code", {{"type", "string"}, {"description", "Faust DSP code to compile"}}},
                {"platform", {{"type", "string"}, {"description", "Target platform"}}},
                {"architecture", {{"type", "string"}, {"description", "Target architecture"}}}
            }},
            {"required", json::array({"code", "platform", "architecture"})}
        };
    }
};

/**
 * Tool to validate Faust DSP code
 */
class ValidateDSPTool : public MCPTool {
public:
    std::string execute(const json& arguments) override;
    std::string getDescription() const override {
        return "Validate Faust DSP code syntax and semantics";
    }
    
    json getInputSchema() const override {
        return json{
            {"type", "object"},
            {"properties", {
                {"code", {{"type", "string"}, {"description", "Faust DSP code to validate"}}}
            }},
            {"required", json::array({"code"})}
        };
    }
};

/**
 * Tool to get information about a Faust DSP
 */
class GetDSPInfoTool : public MCPTool {
public:
    std::string execute(const json& arguments) override;
    std::string getDescription() const override {
        return "Get metadata and information about Faust DSP code";
    }
    
    json getInputSchema() const override {
        return json{
            {"type", "object"},
            {"properties", {
                {"code", {{"type", "string"}, {"description", "Faust DSP code to analyze"}}}
            }},
            {"required", json::array({"code"})}
        };
    }
};

/**
 * FaustWebService - Creates and manages Faust-specific MCP tools
 * Following the design pattern from McpUI
 */
class FaustWebService {
private:
    MCPServer* fMCPServer;
    FaustServer* fFaustServer;
    std::string fTargets;
    fs::path fSessionsDir;
    fs::path fMakefilesDir;
    
public:
    /**
     * Constructor
     * @param mcpServer The MCP server to register tools with
     * @param faustServer The Faust server for compilation services
     * @param targets JSON string with available targets
     * @param sessionsDir Directory for compilation sessions
     * @param makefilesDir Directory containing makefiles
     */
    FaustWebService(MCPServer* mcpServer, FaustServer* faustServer, const std::string& targets,
                   const fs::path& sessionsDir, const fs::path& makefilesDir)
        : fMCPServer(mcpServer), fFaustServer(faustServer), fTargets(targets),
          fSessionsDir(sessionsDir), fMakefilesDir(makefilesDir) {}
    
    /**
     * Initialize and register all Faust tools with the MCP server
     */
    void initialize();
};