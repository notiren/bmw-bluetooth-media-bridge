# PlatformIO pre-script for the *_ota environments: passes OTA_PASSWORD from
# include/secrets.h to espota, so the password lives in one place only.
import os
import re

Import("env")  # noqa: F821

path = os.path.join(env.subst("$PROJECT_DIR"), "include", "secrets.h")  # noqa: F821
if os.path.isfile(path):
    with open(path, encoding="utf-8") as f:
        m = re.search(r'^\s*#define\s+OTA_PASSWORD\s+"([^"]*)"', f.read(), re.M)
    if m:
        env.Append(UPLOADERFLAGS=["--auth=" + m.group(1)])  # noqa: F821
    else:
        print("ota_auth.py: OTA_PASSWORD not found in include/secrets.h")
else:
    print("ota_auth.py: include/secrets.h missing, OTA upload will be rejected")
