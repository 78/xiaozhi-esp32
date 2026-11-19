"""Run production tools pagination with small pages, including modern metadata."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class McpPaginationTests(unittest.TestCase):
    def test_small_pages_preserve_tools_and_reject_oversized_definition(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('requires a host C++17 compiler')
        source = (ROOT / 'main/mcp_server.cc').read_text(encoding='utf-8')
        method = source[source.index('void McpServer::GetToolsList('):
                        source.index('void McpServer::DoToolCall(')]
        driver = r'''
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#define CONFIG_MCP_TOOLS_LIST_PAYLOAD_LIMIT 2048
struct McpRequestContext { bool modern; };
using ResponseSender = std::function<void(const std::string&)>;
std::string JsonString(const char* s) { return "\"" + std::string(s) + "\""; }
struct Tool {
    std::string id; size_t bytes; bool hidden=false;
    const std::string& name() const { return id; }
    bool user_only() const { return hidden; }
    std::string to_json() const {
        return "{\"name\":" + JsonString(id.c_str()) + ",\"description\":\"" +
            std::string(bytes,'x') + "\"}";
    }
};
struct McpServer {
    std::vector<std::unique_ptr<Tool>> tools_;
    std::string reply;
    std::string ResultPrefix(const McpRequestContext&) { return "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":"; }
    void ReplyResult(const McpRequestContext& c,const std::string& value,const ResponseSender&) {
        reply=ResultPrefix(c)+value+"}";
    }
    void ReplyError(const McpRequestContext&,const std::string&,const ResponseSender&) { reply="ERROR"; }
    void ReplyError(const McpRequestContext& c,int,const std::string& s,const ResponseSender& f) { ReplyError(c,s,f); }
    void GetToolsList(const McpRequestContext&,const std::string&,bool,const ResponseSender&);
};
''' + method + r'''
int main() {
    McpServer s;
    for(int i=0;i<13;++i) s.tools_.emplace_back(new Tool{"tool"+std::to_string(i),400,i==4});
    for(bool modern:{false,true}) {
        std::string cursor;
        for(int page=0;page<20;++page) {
            s.GetToolsList({modern},cursor,false,{});
            std::cout<<s.reply<<"\n";
            auto pos=s.reply.find("\"nextCursor\":\"");
            if(pos==std::string::npos) break;
            pos+=14; auto end=s.reply.find('"',pos); cursor=s.reply.substr(pos,end-pos);
        }
    }
    s.tools_.clear(); s.tools_.emplace_back(new Tool{"too-large",2200});
    s.GetToolsList({false},"",false,{});
    if(s.reply!="ERROR") return 1;
    s.GetToolsList({false},"missing",false,{});
    return s.reply=="ERROR" ? 0 : 2;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            cpp, binary = path / 'test.cc', path / 'test.exe'
            cpp.write_text(driver, encoding='utf-8')
            subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            str(cpp), '-o', str(binary)], check=True, capture_output=True, text=True)
            run = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
        groups = {False: [], True: []}
        for line in run.stdout.splitlines():
            self.assertLessEqual(len(line.encode()), 2048)
            result = json.loads(line)['result']
            groups['ttlMs' in result].extend(tool['name'] for tool in result['tools'])
        expected = ['tool' + str(i) for i in range(13) if i != 4]
        self.assertEqual(groups[False], expected)
        self.assertEqual(groups[True], expected)


if __name__ == '__main__':
    unittest.main()
