/************************************************************************
 FAUST Architecture File
 Copyright (C) 2025 GRAME, Centre National de Creation Musicale
 ---------------------------------------------------------------------
 MCP Server implementation
 ************************************************************************/

#include "MCPServer.h"
#include <algorithm>
#include <exception>

void MCPServer::sendResponse(const json& response) {
    std::string responseStr = response.dump();
    fOutput << responseStr << std::endl;
    fOutput.flush();
    fLog << "Sent: " << responseStr << std::endl;
}

void MCPServer::sendError(int code, const std::string& message, const json& id) {
    json response = {
        {"jsonrpc", "2.0"},
        {"id", id.is_null() ? nullptr : id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
    sendResponse(response);
}

void MCPServer::handleInitialize(const json& request) {
    json response = {
        {"jsonrpc", "2.0"},
        {"id", request["id"]},
        {"result", {
            {"protocolVersion", "2025-06-18"},
            {"capabilities", {
                {"tools", json::object()}
            }},
            {"serverInfo", {
                {"name", fServerName},
                {"version", fServerVersion}
            }}
        }}
    };
    
    sendResponse(response);
}

void MCPServer::handleToolsList(const json& request) {
    json tools = json::array();
    
    for (const auto& tool : fTools) {
        tools.push_back({
            {"name", tool.first},
            {"description", tool.second->getDescription()},
            {"inputSchema", tool.second->getInputSchema()}
        });
    }
    
    json response = {
        {"jsonrpc", "2.0"},
        {"id", request["id"]},
        {"result", {
            {"tools", tools}
        }}
    };
    
    sendResponse(response);
}

void MCPServer::handleToolCall(const json& request) {
    json params = request["params"];
    std::string toolName = params["name"];
    json arguments = params.contains("arguments") ? params["arguments"] : json::object();
    
    fLog << "Tool call: " << toolName << " with args: " << arguments.dump() << std::endl;
    
    auto it = fTools.find(toolName);
    if (it != fTools.end()) {
        try {
            std::string result = it->second->execute(arguments);
            
            json response = {
                {"jsonrpc", "2.0"},
                {"id", request["id"]},
                {"result", {
                    {"content", json::array({
                        {
                            {"type", "text"},
                            {"text", result}
                        }
                    })}
                }}
            };
            
            sendResponse(response);
        } catch (const std::exception& e) {
            sendError(-32603, std::string("Tool execution error: ") + e.what(), request["id"]);
        }
    } else {
        sendError(-32602, "Unknown tool: " + toolName, request["id"]);
    }
}

void MCPServer::handleNotification(const json& request) {
    std::string method = request["method"];
    fLog << "Received notification: " << method << std::endl;
    // Notifications don't require a response
}

void MCPServer::processRequest(const std::string& requestStr) {
    fLog << "Received: " << requestStr << std::endl;
    
    try {
        json request = json::parse(requestStr);
        
        std::string method = request.value("method", "");
        
        // Check if it's a notification (no id or specific notification methods)
        bool isNotification = !request.contains("id") || 
                             method.find("notifications/") == 0 || 
                             method.find("$/") == 0;
        
        if (isNotification) {
            handleNotification(request);
            return;
        }
        
        // Handle request methods
        if (method == "initialize") {
            handleInitialize(request);
        } else if (method == "tools/list") {
            handleToolsList(request);
        } else if (method == "tools/call") {
            handleToolCall(request);
        } else {
            sendError(-32601, "Method not found", request["id"]);
        }
    } catch (const json::parse_error& e) {
        fLog << "JSON parse error: " << e.what() << std::endl;
        sendError(-32700, "Parse error");
    } catch (const std::exception& e) {
        fLog << "Error processing request: " << e.what() << std::endl;
        sendError(-32603, std::string("Internal error: ") + e.what());
    }
}

void MCPServer::run() {
    fRunning = true;
    fLog << "MCP Server started: " << fServerName << " v" << fServerVersion << std::endl;
    
    std::string line;
    while (fRunning && std::getline(fInput, line)) {
        if (!line.empty()) {
            processRequest(line);
        }
    }
    
    fLog << "MCP Server stopped" << std::endl;
}