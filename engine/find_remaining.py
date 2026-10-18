import json
import urllib.request
import re

files = json.load(open('lib_files.json'))
unresolved_lines = [
    "-SUR+UoLS6c#libSceNpTus", "-YTW+qXc3CQ#libkernel", "0cBgduPRR+M#libSceNetCtl",
    "1gL5PwYzrrw#libSceNpScore", "1n-dGukBgnY#libSceNpTus", "3bh2aBvvmvM#libSceNpTus",
    "4VLlu7EIjzk#libSceNpTus", "4u58d6g6uwU#libSceNpTus", "8mql9OcQnd4#libkernel",
    "9XAxK2PMwk8#libSceErrorDialog", "ANJssPz3mY0#libSceNpScore", "BpFoboUJoZU#libkernel",
    "Bq+LRV-N6Hk#libScePosix", "CFPx3eyaT34#libSceNpTus", "CcIH40dYS88#libSceNpTus",
    "EPeq43CQKxY#libSceNpTus", "GWnWQNXZH5M#libSceNpScore", "H3uq7x0sZOI#libSceNpTus",
    "How7B8Oet6k#libkernel", "I88KChlynSs#libSceErrorDialog", "IKP8typ0QUk#libScePosix",
    "IOnSvHzqu6A#libkernel", "Iw2h0Jrrb5U#libSceNpMatching2", "JTvBflhYazQ#libkernel",
    "KjNeZ-29ysQ#libSceNpWebApi", "M2ZF-ClLhgY#libSceErrorDialog", "PP9jx8s0574#libSceNpScore",
    "V6KSpKv9XJE#libSceNpMatching2", "WWiGuh9XfgQ#libSceErrorDialog", "YCV5dGGBcCo#libScePosix",
    "Z4wwCFiBELQ#libSceNetCtl", "ajvzc8e2upo#libSceNpMatching2", "cDW233RAwWo#libScePosix",
    "cf-WMA0jYCc#libSceNpTus", "dRszNNyGWkw#libSceNpScore", "fcCwKpi4CbU#libSceNpTus",
    "gQ6cUriNpgs#libSceNpMatching2", "lBtrk+7lk14#libSceNpTus", "m1DfNRstkSQ#libSceNpScore",
    "m9XZnxw9AmE#libSceNpTus", "pDuPEf3m4fI#libScePosix", "pFzhpCMlJXQ#libSceNpMatching2",
    "r42bWcQbtZY#libSceNpCommerce", "t7b6dmpQNiI#libSceNpTus", "uBESzz4CQws#libSceNpMatching2",
    "y5ja7WI05rs#libSceNpScore", "zk6c65xoyO0#libSceNpWebApi"
]
targets = {l.split('#')[0]: l for l in unresolved_lines}

# Select relevant files
keywords = ['np', 'kernel', 'error_dialog', 'net', 'system']
candidate_files = [f for f in files if any(k in f for k in keywords) and f.endswith('.cpp')]
print(f"Checking {len(candidate_files)} candidate files for {len(targets)} remaining NIDs...")

found = {}
for path in candidate_files:
    raw_url = f"https://raw.githubusercontent.com/shadps4-emu/shadPS4/main/{path}"
    try:
        content = urllib.request.urlopen(urllib.request.Request(raw_url, headers={'User-Agent': 'Mozilla/5.0'})).read().decode('utf-8', errors='ignore')
        matches = re.findall(r'LIB_FUNCTION\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*[^,]+,\s*[^,]+,\s*([^\s,\)]+)', content)
        for nid, lib, fn in matches:
            if nid in targets:
                found[nid] = (fn, lib, targets[nid])
    except Exception as e:
        pass

print(f"Found {len(found)} of {len(targets)} remaining:")
for nid, (fn, lib, full) in sorted(found.items()):
    print(f"  {full:40s} -> {fn}")

still_unresolved = set(targets.keys()) - set(found.keys())
print(f"\nStill unresolved: {len(still_unresolved)}")
for nid in sorted(still_unresolved):
    print(f"  {targets[nid]}")
