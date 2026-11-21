"""Run actual dispatcher bodies with C++17 platform/expected fixtures.

This verifies scheduling/ownership/admission; full JSON serialization remains
covered by test_mcp_server on a C++23 host and target builds.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DeferredDispatcherTests(unittest.TestCase):
    def test_owned_arguments_epoch_gate_and_bounded_async_replies(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("requires a host C++17 compiler")
        source = (ROOT / "main/mcp_server.cc").read_text(encoding="utf-8")
        production = source[source.index("void McpServer::DoToolCall("):]
        stub = r'''
#include <atomic>
#include <climits>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#define CONFIG_ASYNC_MCP_TOOLS 1
#include "deferred_reply_gate.h"
// Minimal test-only value/error carrier, not a substitute for C++23 conformance.
namespace std {
template<class T,class E> struct expected {
    bool valid=true; T value{}; E problem{};
    expected()=default; expected(T v):value(std::move(v)) {}
    explicit operator bool() const { return valid; }
    const T& operator*() const { return value; }
    const E& error() const { return problem; }
};
template<class E> struct expected<void,E> {
    bool valid=true; E problem{};
    explicit operator bool() const { return valid; }
    const E& error() const { return problem; }
};
}
enum Kind { kPropertyTypeBoolean,kPropertyTypeInteger,kPropertyTypeString };
struct cJSON { int type=0,valueint=0; double valuedouble=0; char* valuestring=nullptr; cJSON* child=nullptr; };
bool cJSON_IsObject(const cJSON* p) { return p && p->type==1; }
bool cJSON_IsBool(const cJSON* p) { return p && p->type==2; }
bool cJSON_IsNumber(const cJSON* p) { return p && p->type==3; }
bool cJSON_IsString(const cJSON* p) { return p && p->type==4; }
cJSON* cJSON_GetObjectItemCaseSensitive(const cJSON* p,const char*) { return p ? p->child : nullptr; }
cJSON* cJSON_GetObjectItem(const cJSON* p,const char* key) { return cJSON_GetObjectItemCaseSensitive(p,key); }
struct Property {
    std::string text;
    const std::string& name() const { static std::string name="text";return name; }
    Kind type() const { return kPropertyTypeString; }
    bool has_default_value() const { return false; }
    template<class T> std::expected<void,std::string> set_value(T value) {
        (void)value;
        if constexpr(std::is_same_v<T,std::string>) text=std::move(value);
        return {};
    }
};
using PropertyList=std::vector<Property>;
struct McpRequestContext { std::string id_json;bool modern=false;std::shared_ptr<int> owner; };
using ToolResult=std::expected<std::string,std::string>;
using AsyncToolResult=ToolResult;
using ToolCompletion=std::function<void(ToolResult)>;
struct McpTool {
    bool async=true;PropertyList props;
    struct Invocation { const McpRequestContext* context;std::string argument;ToolCompletion complete; };
    std::vector<Invocation> calls;
    const PropertyList& properties() const { return props; }
    bool IsAsync() const { return async; }
    ToolResult Call(const PropertyList&,const McpRequestContext&) { return std::string("sync-ok"); }
    void CallAsync(const PropertyList& p,const McpRequestContext& c,ToolCompletion done) {
        calls.push_back({&c,p.empty()?"":p[0].text,std::move(done)});
    }
    static ToolResult SerializeResult(ToolResult result,const McpRequestContext&) { return result; }
    static ToolResult SerializeAsyncResult(ToolResult result,const McpRequestContext&) { return result; }
};
struct Application {
    uint32_t epoch=1;std::vector<std::function<void()>> tasks;
    static Application& GetInstance() { static Application app;return app; }
    uint32_t GetMcpSessionEpoch() const { return epoch; }
    void Schedule(std::function<void()> task) { tasks.push_back(std::move(task)); }
    void Drain() { while(!tasks.empty()) { auto work=std::move(tasks);tasks.clear();for(auto& task:work)task(); } }
};
std::string JsonString(const char* value) { return std::string("\"")+value+"\""; }
struct McpServer {
    using ResponseSender=std::function<void(const std::string&)>;
    unsigned pending_async_tools_=0;
    struct Reply { std::string id,body;bool modern; };
    std::vector<Reply> replies;
    void DoToolCall(McpRequestContext,McpTool*,const cJSON*,ResponseSender);
    void FinishToolCall(const McpRequestContext&,const ToolResult&,const ResponseSender&);
    void ReplyResult(const McpRequestContext& c,const std::string& body,const ResponseSender& sender) {
        replies.push_back({c.id_json,body,c.modern});if(sender)sender(body);
    }
    void ReplyError(const McpRequestContext& c,int,const std::string& body,const ResponseSender& sender) {
        ReplyResult(c,body,sender);
    }
    void ReplyError(const McpRequestContext& c,const std::string& body,const ResponseSender& sender) {
        ReplyResult(c,body,sender);
    }
};
'''
        driver = r'''
#define check(value) do { if(!(value)) { std::fprintf(stderr,"check line %d: %s\n",__LINE__,#value);return 1; } } while(0)
int main() {
    auto& app=Application::GetInstance();McpServer server;McpTool tool;
    tool.props.push_back({});
    std::string argument="kup";
    cJSON child;child.type=4;child.valuestring=argument.data();
    cJSON object;object.type=1;object.child=&child;
    auto owner=std::make_shared<int>(42);std::weak_ptr<int> weak=owner;
    server.DoToolCall({"owned",true,owner},&tool,&object,nullptr);
    owner.reset();argument="xxx";app.Drain();
    check(tool.calls.size()==1 && tool.calls[0].argument=="kup");
    check(tool.calls[0].context->id_json=="owned" && !weak.expired());
    check(server.replies.empty() && server.pending_async_tools_==1);
    tool.calls[0].complete(std::string("worker-ok"));
    tool.calls[0].complete(std::string("duplicate"));
    app.Drain();check(server.pending_async_tools_==0 && server.replies.size()==1);
    check(server.replies.back().id=="owned" && server.replies.back().modern);
    tool.calls.clear();check(weak.expired());
    tool.props.clear();server.replies.clear();
    for(int i=0;i<5;++i)server.DoToolCall({std::to_string(i),false,{}},&tool,nullptr,nullptr);
    app.Drain();check(tool.calls.size()==4 && server.pending_async_tools_==4);
    check(server.replies.size()==1 && server.replies.back().body.find("Too many")!=std::string::npos);
    ++app.epoch;
    for(auto& call:tool.calls)call.complete(std::string("stale"));
    app.Drain();check(server.pending_async_tools_==0 && server.replies.size()==1);
    tool.calls.clear();server.replies.clear();
    server.DoToolCall({"queued-old",false,{}},&tool,nullptr,nullptr);
    ++app.epoch;app.Drain();check(tool.calls.empty()); // no stale action dispatch
    unsigned direct_replies=0;
    server.DoToolCall({"direct",true,{}},&tool,nullptr,[&](const std::string&){++direct_replies;});
    app.Drain();check(tool.calls.size()==1);
    ++app.epoch;tool.calls[0].complete(std::string("direct-ok"));app.Drain();
    check(direct_replies==1 && server.pending_async_tools_==0);
    tool.calls.clear();tool.async=false;
    server.DoToolCall({"legacy",false,{}},&tool,nullptr,nullptr);app.Drain();
    check(server.replies.back().id=="legacy" && !server.replies.back().modern);
    check(server.replies.back().body=="sync-ok");
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            source_file,binary=path/"test.cc",path/"test.exe"
            source_file.write_text(stub+production+driver,encoding="utf-8")
            result=subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror","-I",
                                   str(ROOT/"main"),str(source_file),"-o",str(binary)],
                                  capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(binary)],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)


if __name__ == "__main__":
    unittest.main()
