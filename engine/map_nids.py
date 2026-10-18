import urllib.request
import json
import re

missing_lines = open('missing_135.txt').read().splitlines()
missing_nids = {}
for l in missing_lines:
    token = l.split()[1] # nid#lib
    nid, lib = token.split('#')
    missing_nids[nid] = (lib, token)

print(f"Total missing NIDs to map: {len(missing_nids)}")

lib_folders = [
    'hmd', 'kernel', 'camera', 'fiber', 'vr_tracker', 'signin_dialog', 
    'screenshot', 'save_data', 'share_play', 'playgo', 'app_content', 
    'net', 'system', 'ajm', 'libc_internal', 'disc_map', 'sysmodule', 'np'
]

found_map = {}
for folder in lib_folders:
    url = f"https://api.github.com/repos/shadps4-emu/shadPS4/contents/src/core/libraries/{folder}"
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    try:
        with urllib.request.urlopen(req) as resp:
            files = json.loads(resp.read().decode())
            for f in files:
                fname = f['name']
                if fname.endswith('.cpp'):
                    download_url = f.get('download_url')
                    if not download_url:
                        continue
                    try:
                        code = urllib.request.urlopen(urllib.request.Request(download_url, headers={'User-Agent': 'Mozilla/5.0'})).read().decode('utf-8', errors='ignore')
                        matches = re.findall(r'LIB_FUNCTION\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*[^,]+,\s*[^,]+,\s*([^\s,\)]+)', code)
                        for nid, lib, fn in matches:
                            if nid in missing_nids:
                                found_map[nid] = (fn, lib, missing_nids[nid][1])
                    except Exception as e:
                        pass
    except Exception as e:
        print(f"Failed folder {folder}: {e}")

print(f"\nSuccessfully mapped {len(found_map)} out of {len(missing_nids)} NIDs directly!")
for nid, (fn, lib, full) in sorted(found_map.items()):
    print(f"  {full:45s} -> {fn}")
unresolved = set(missing_nids.keys()) - set(found_map.keys())
print(f"\nUnresolved: {len(unresolved)}")
for nid in sorted(unresolved):
    print(f"  UNRESOLVED: {missing_nids[nid][1]}")
