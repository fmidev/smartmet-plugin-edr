#!/usr/bin/env python3
"""
Summarizes the TestNG results of an OGC executable test suite run.

Usage: ets-summary.py <testng-results.xml>
"""

import sys
import xml.etree.ElementTree as ET


def main():
    root = ET.parse(sys.argv[1]).getroot()
    print("total {total}, passed {passed}, failed {failed}, skipped {skipped}".format(**root.attrib))
    for test in root.iter("test"):
        for cls in test.iter("class"):
            for method in cls.iter("test-method"):
                if method.get("is-config") == "true" or method.get("status") == "PASS":
                    continue
                message = method.find("exception/message")
                text = (message.text or "").strip() if message is not None else ""
                print(f"\n{method.get('status')}  {test.get('name')}: "
                      f"{cls.get('name').split('.')[-1]}.{method.get('name')}")
                for line in [l.strip() for l in text.splitlines() if l.strip()][:20]:
                    print("    " + line[:300])
    return 1 if int(root.get("failed", "0")) else 0


if __name__ == "__main__":
    sys.exit(main())
