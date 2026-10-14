#!/usr/bin/env python3
"""Local syntax check for the samp C/C++ tree.

Runs g++ -fsyntax-only per translation unit. Classifies each diagnostic line:
  - missing-header / include failures  -> ENV (not a syntax defect, deps absent locally)
  - everything else emitted by the parser -> SUSPECT (potential real syntax error)

The goal: distinguish "cannot build here because NDK/3rd-party libs are absent"
from "the code itself does not parse".
"""
import os, re, subprocess, sys, json

ROOT = os.path.dirname(os.path.abspath(__file__))
CPP = os.path.join(ROOT, "app/src/main/cpp")
SAMP = os.path.join(CPP, "samp")

INCLUDE_DIRS = [
    SAMP,
    os.path.join(SAMP, "util"),
    os.path.join(SAMP, "vendor"),
    os.path.join(SAMP, "vendor/shadowhook"),
    os.path.join(SAMP, "vendor/imgui"),
    os.path.join(SAMP, "game/RW"),
    os.path.join(SAMP, "game/Core"),
    os.path.join(CPP, "opus/include"),
]

EXTS = (".c", ".cc", ".cpp")
SKIP_PARTS = ("/vendor/", "/opus/")

def collect():
    out = []
    for dirpath, _dirs, files in os.walk(SAMP):
        for f in files:
            if f.endswith(EXTS):
                p = os.path.join(dirpath, f)
                if any(s in p for s in SKIP_PARTS):
                    continue
                out.append(p)
    return sorted(out)

MISSING = re.compile(r"(fatal error|error):.*(No such file or directory|file not found)")
# Any other 'error:' is a parser-level complaint.
ANYERR = re.compile(r":\d+:\d*:?\s*(fatal error|error):")

def check(path):
    pfx = os.path.relpath(path, CPP)
    args = ["g++", "-fsyntax-only", "-std=c++20", "-pthread", "-w",
            "-DVER_x32=false", "-DANDROID", "-fexceptions"]
    for d in INCLUDE_DIRS:
        args += ["-I", d]
    args.append(path)
    r = subprocess.run(args, capture_output=True, text=True, timeout=120)
    lines = (r.stderr or "").splitlines()
    missing, other = [], []
    for ln in lines:
        if not ANYERR.search(ln):
            continue
        if MISSING.search(ln):
            missing.append(ln)
        else:
            other.append(ln)
    return {"file": pfx, "rc": r.returncode,
            "missing_headers": missing, "syntax_errors": other}

def main():
    files = collect()
    results = []
    for i, f in enumerate(files):
        try:
            res = check(f)
        except subprocess.TimeoutExpired:
            res = {"file": os.path.relpath(f, CPP), "rc": -9,
                   "missing_headers": ["<timeout>"], "syntax_errors": []}
        results.append(res)
        sys.stderr.write("[%3d/%3d] %s  rc=%s  syn=%d  miss=%d\n" % (
            i+1, len(files), res["file"], res["rc"],
            len(res["syntax_errors"]), len(res["missing_headers"])))
        sys.stderr.flush()
    with open(os.path.join(ROOT, ".syntax_report.json"), "w") as fh:
        json.dump(results, fh, indent=1)
    # summary
    clean = [r for r in results if not r["syntax_errors"]]
    dirty = [r for r in results if r["syntax_errors"]]
    print("TOTAL=%d  PARSE_OK=%d  PARSE_ERRORS=%d" % (len(results), len(clean), len(dirty)))
    for r in dirty:
        print("---- %s" % r["file"])
        for e in r["syntax_errors"][:12]:
            print("      " + e)

if __name__ == "__main__":
    main()
