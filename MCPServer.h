/************************************************************************
 FAUST Architecture File
 Copyright (C) 2025 GRAME, Centre National de Creation Musicale
 ---------------------------------------------------------------------
 MCP Server - Generic Model Context Protocol server implementation
 Based on the design from McpUI project
 ************************************************************************/

#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <sstream>
#include <functional>
#include "json.hpp"

using json = nlohmann::json;

/**
 * Abstract base class for MCP tools
 */
class MCPTool {
public:
    virtual ~MCPTool() = default;
    
    /**
     * Execute the tool with given arguments
     * @param arguments JSON object with tool arguments
     * @return Result string to be sent as text content
     */
    virtual std::string execute(const json& arguments) = 0;
    
    /**
     * Get tool description for tools/list
     */
    virtual std::string getDescription() const = 0;
    
    /**
     * Get input schema for the tool
     */
    virtual json getInputSchema() const {
        return json{{"type", "object"}, {"properties", json::object()}};
    }
};

/**
 * Generic MCP Server that handles JSON-RPC protocol
 * Based on SimpleMCPServer from mcp-protocol.h
 */
class MCPServer {
private:
    std::istream& fInput;
    std::ostream& fOutput;
    std::ostream& fLog;
    std::map<std::string, std::unique_ptr<MCPTool>> fTools;
    bool fRunning;
    std::string fServerName;
    std::string fServerVersion;
    
    // Message handlers
    void handleInitialize(const json& request);
    void handleToolsList(const json& request);
    void handleToolCall(const json& request);
    void handleNotification(const json& request);
    
    // Send response to stdout
    void sendResponse(const json& response);
    void sendError(int code, const std::string& message, const json& id = nullptr);
    
    // Process a single request
    void processRequest(const std::string& request);
    
public:
    /**
     * Constructor
     * @param input Input stream (usually std::cin)
     * @param output Output stream (usually std::cout)
     * @param log Log stream (usually std::cerr or a file stream)
     * @param name Server name
     * @param version Server version
     */
    MCPServer(std::istream& input = std::cin, 
              std::ostream& output = std::cout,
              std::ostream& log = std::cerr,
              const std::string& name = "MCP Server",
              const std::string& version = "1.0.0")
        : fInput(input), fOutput(output), fLog(log), 
          fRunning(false), fServerName(name), fServerVersion(version) {}
    
    /**
     * Register a tool
     * @param name Tool name (must be unique)
     * @param tool Tool implementation
     */
    void registerTool(const std::string& name, std::unique_ptr<MCPTool> tool) {
        fTools[name] = std::move(tool);
        fLog << "Registered tool: " << name << std::endl;
    }
    
    /**
     * Run the MCP server (blocking)
     * Reads from input stream and writes to output stream
     */
    void run();
    
    /**
     * Stop the server
     */
    void stop() {
        fRunning = false;
    }
};