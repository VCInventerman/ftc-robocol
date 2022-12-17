import sys
import os
import subprocess

def listfiles(folder):
    for root, folders, files in os.walk(folder):
        for filename in folders + files:
            yield os.path.join(root, filename)

def nop():
    return




rootDir =  sys.argv[1] if len(sys.argv) > 1 else "/opt/devkitpro"

allowed = [ str(chr(i)) + " operator new" for i in range(ord("A"), ord("Z")) if i != ord("U") ]

for path in listfiles(rootDir):
    if not (path.endswith(".a") or path.endswith(".so")): continue

    #listsyms = subprocess.run(["nm", "-gDC", path], stderr=subprocess.PIPE, stdout=subprocess.PIPE)

    listsyms = subprocess.Popen(["nm", "-aC", path], 
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=256*1024*1024)

    out, err = listsyms.communicate()
    outstr = str(out)
    lines = outstr.split("\\n")

    for i in allowed:
        for li in range(len(lines)):
            if (lines[li].find(i) != -1):
                nop()
                print(path)

                mangledProc = subprocess.Popen(["nm", "-a", path], 
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=256*1024*1024)

                out2, err2 = mangledProc.communicate();
                outstr2 = str(out2)
                lines2 = outstr2.split("\\n")

                check = lines2[li]
                print(check, lines[li])

