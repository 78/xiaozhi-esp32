#ifndef MCP_REMOTE_CLIENT_H
#define MCP_REMOTE_CLIENT_H

// Optional MCP *client* tools, letting the model reach MCP servers other than
// the device itself (compiled in only when CONFIG_USE_MCP_CLIENT is enabled):
//
//   self.mcp.discover(query)
//       Searches the public MCP registry (registry.modelcontextprotocol.io)
//       and returns a short list of matching remote servers.
//   self.mcp.connect(url, auth_token?)
//       Opens a Streamable-HTTP MCP session with a server and returns its
//       tools/list result.
//   self.mcp.call_tool(url, tool_name, arguments?, auth_token?)
//       Calls one tool on a server (tools/call) and returns the result.
//
// Every call is stateless: connect/call_tool each run their own
// initialize -> notifications/initialized handshake instead of keeping a
// session open between calls.
void AddMcpRemoteClientTools();

#endif  // MCP_REMOTE_CLIENT_H
