#!/usr/bin/env python3
"""
parse_timing.py — 解析 choreo_task 中 ESP_LOGI("CHOREO_TIME", ...) 的输出
用法:
    python parse_timing.py log.txt
    idf.py monitor | python parse_timing.py

输入格式（choreo.c 中的打点）:
    I (123456) CHOREO_TIME: idx=0 type=0 dur_ms=460
"""
import re, sys, argparse
from collections import defaultdict

STEP_NAMES = {
    0: "walk",
    1: "sway_fb",
    2: "sway_lr",
    3: "sway_twist",
    4: "sway_updown",
    5: "sway_side_l",
    6: "sway_side_r",
    7: "sway_wave",
    8: "sway_march",
    9: "sway_nod",
    10: "sway_tremble",
    11: "pause",
}

def parse_log(path=None):
    """从文件或 stdin 读取日志，提取 CHOREO_TIME 行"""
    pattern = re.compile(
        r"CHOREO_TIME:\s*idx=(\d+)\s*type=(\d+)\s*dur_ms=(\d+)"
    )
    results = []
    source = sys.stdin if path is None else open(path, "r", encoding="utf-8", errors="replace")
    try:
        for line in source:
            m = pattern.search(line)
            if not m:
                continue
            results.append({
                "idx":    int(m.group(1)),
                "type":   int(m.group(2)),
                "dur_ms": int(m.group(3)),
            })
    finally:
        if path is not None:
            source.close()
    return results


def analyze(results, json_path=None):
    """按 (type) 分组统计，输出表格"""
    groups = defaultdict(list)
    for r in results:
        groups[r["type"]].append(r["dur_ms"])

    print(f"\n{'='*60}")
    print(f"  动作耗时测量报告  (样本数={len(results)})")
    print(f"{'='*60}\n")
    print(f"  {'动作':<18} {'样本数':>6} {'平均(ms)':>10} {'最小(ms)':>10} {'最大(ms)':>10}")
    print(f"  {'-'*17} {'-'*6} {'-'*10} {'-'*10} {'-'*10}")

    for t in sorted(groups.keys()):
        vals = groups[t]
        name = STEP_NAMES.get(t, f"type_{t}")
        avg  = sum(vals) / len(vals)
        print(f"  {name:<18} {len(vals):>6} {avg:>10.0f} {min(vals):>10} {max(vals):>10}")

    print(f"\n{'='*60}")

    # 建议修正系数
    print("\n[修正建议]")
    for t in sorted(groups.keys()):
        vals = groups[t]
        avg = sum(vals) / len(vals)
        name = STEP_NAMES.get(t, f"type_{t}")
        if name == "walk":
            print(f"  {name}: 实测 {avg:.0f}ms/步  (预期 1500ms/步)")
        elif name == "pause":
            continue
        else:
            print(f"  {name}: 实测 {avg:.0f}ms  (需要在 jiaqiwu.json 中调整 cycles 或 half_ms)")

    print("")


def main():
    parser = argparse.ArgumentParser(description="解析 CHOREO_TIME 日志")
    parser.add_argument("logfile", nargs="?", help="日志文件路径（不填则读 stdin）")
    parser.add_argument("--json", help="对照的 JSON 文件路径（可选，用于显示 cycles/half_ms）")
    args = parser.parse_args()

    results = parse_log(args.logfile)
    if not results:
        print("[错误] 未找到任何 CHOREO_TIME 行，请确认日志格式正确。")
        sys.exit(1)

    analyze(results, args.json)


if __name__ == "__main__":
    main()
