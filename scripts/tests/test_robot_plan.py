"""Sequence validation is atomic and completion waits remain bounded."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class RobotPlanTests(unittest.TestCase):
    def test_validation_timeouts_and_completion_gate(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("requires a host C++17 compiler")
        driver = r'''
#include <cassert>
#include <cstdio>
#include "boards/petoi/bittle-ai-c3/robot_plan.h"
#include "boards/petoi/bittle-ai-c3/robot_language.h"
#include "deferred_reply_gate.h"
#undef assert
#define assert(value) do { if (!(value)) { std::fprintf(stderr,"check line %d: %s\n",__LINE__,#value); return 1; } } while (0)
int main() {
    petoi::RobotPlan plan;
    assert(petoi::BuildRobotPlan(" kup, ksit ,d",plan));
    assert(plan.count==3 && petoi::PlanText(plan)=="kup,ksit,d");
    assert(plan.frames[0].data[0]=='X' && plan.frames[1].data[2]==0x15);
    for (const auto& text: {"kup,",",kup","kup,,ksit","kup;ksit","kup\nksit",
                           "kup,ksit,d,khi,kstr","坐下"}) {
        assert(!petoi::BuildRobotPlan(text,plan)); assert(plan.count==0);
    }
    assert(petoi::BuildRobotPlan("ksit,ksit",plan));
    assert(petoi::SameAction(plan.frames[0],plan.frames[1]));
    assert(petoi::CommandTimeoutMs(plan.frames[0])==12000);
    assert(petoi::BuildRobotPlan("kpu,kvtL 90,kwkF 3,kwkF 99999999999999999",plan));
    assert(petoi::CommandTimeoutMs(plan.frames[0])==8000);
    assert(petoi::CommandTimeoutMs(plan.frames[1])==8500);
    assert(petoi::CommandTimeoutMs(plan.frames[2])>=4000);
    assert(petoi::CommandTimeoutMs(plan.frames[3])<=120000);
    // OpenCat's 200 boundary is already a duration, not 200 gait cycles.
    assert(petoi::BuildRobotPlan("kwkF 199,kwkF 200,kwkF 201",plan));
    assert(petoi::CommandTimeoutMs(plan.frames[0])==20000);
    assert(petoi::CommandTimeoutMs(plan.frames[1])==12000);
    assert(petoi::CommandTimeoutMs(plan.frames[2])==12000);
    DeferredReplyGate voice(4,true);
    assert(voice.CanReply(4) && !voice.CanReply(5));
    assert(voice.Claim() && !voice.Claim());
    DeferredReplyGate direct(4,false);
    assert(direct.CanReply(999) && direct.Claim() && !direct.Claim());
    std::string commands;
    assert(petoi::BuildLocalActionText("先站起来再坐下然后休息",commands));
    assert(commands=="kup,ksit,d");
    assert(petoi::BuildLocalActionText("先站起来，再坐下，最后休息。",commands));
    assert(commands=="kup,ksit,d");
    assert(petoi::BuildLocalActionText("先站起来再坐下最后休息。",commands));
    assert(commands=="kup,ksit,d");
    assert(petoi::BuildLocalActionText("先坐下再站起来再坐下",commands));
    assert(commands=="ksit,kup,ksit");
    assert(petoi::BuildLocalActionText("向前走三步，然后左转30度",commands));
    assert(commands=="kwkF 3,kvtL 30");
    assert(petoi::BuildLocalActionText("Sit down then stand up",commands));
    assert(commands=="ksit,kup");
    for (const auto& text:{"不要坐下","为什么要坐下","讲个坐下的故事","visit a friend",
                           "向前走二十步","前进999999999999999999999999999步",
                           "坐下然后讲个故事","坐下然后站起来然后休息然后坐下然后站起来",
                           "先站起来再做下最后休息。","先站起来再做下，最后休息。"}) {
        assert(!petoi::BuildLocalActionText(text,commands)); assert(commands.empty());
    }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            source, binary = path / "test.cc", path / "test.exe"
            source.write_text(driver, encoding="utf-8")
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I",
                            str(ROOT / "main"), str(source), "-o", str(binary)],
                           check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
