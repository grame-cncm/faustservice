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

using json = nlohmann::json;

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
    
public:
    CompileDSPTool(FaustServer* server) : fServer(server) {}
    
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
    
public:
    /**
     * Constructor
     * @param mcpServer The MCP server to register tools with
     * @param faustServer The Faust server for compilation services
     * @param targets JSON string with available targets
     */
    FaustWebService(MCPServer* mcpServer, FaustServer* faustServer, const std::string& targets)
        : fMCPServer(mcpServer), fFaustServer(faustServer), fTargets(targets) {}
    
    /**
     * Initialize and register all Faust tools with the MCP server
     */
    void initialize();
};