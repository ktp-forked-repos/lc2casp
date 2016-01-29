#!/usr/bin/python
# converts answer set (only 1 answer must be given, preceded by answer) to flatzinc constraints

import sys
import subprocess
import tempfile
import os
import signal


timelimit =3600
memlimit =6000

def handler(signum, frame):
  try:
    finstance.close()
    fsolverout.close()
    fcheck.close()
  finally:
    a = 0 
    os.remove(instance)
    os.remove(watcher)
    os.remove(solverout)
    os.remove(check)
    sys.stdout.flush()



def flatzincUNSAT(testit):
  res = subprocess.Popen(["./runsolver", "-w "+watcher, "-M"+str(memlimit), "-W"+str(timelimit), "./flatzinc", testit],stdout=subprocess.PIPE)
  res = res.communicate()[0]
  for s in res:
    if (s.find("UNSATISFIABLE")!=-1):
      return True
  return False
  

if len(sys.argv)<2:
    print 'One file and several parameters for clingcon expected'
    sys.exit(1)

flatzinc = sys.argv[1]
print("Currently processing file: " + flatzinc)
(fd, instance) = tempfile.mkstemp()
finstance = open(instance, "w")
subprocess.call(["./fz", sys.argv[1]], stdout=finstance)
finstance.close()
(fd, watcher) = tempfile.mkstemp()
(fd, solverout) = tempfile.mkstemp()
#print(watcher)
#print(solverout)
#cant use -o of runsolver, interferes with python subprocess pipe
#commandline = ["./runsolver", "-w "+watcher, "-o "+solverout, "-M"+str(memlimit), "-W"+str(timelimit), "./clingcon", instance]
commandline = ["./runsolver", "-w "+watcher, "-M"+str(memlimit), "-W"+str(timelimit), "./clingcon", "--quiet=1,0,2", instance]
#print(" ".join(commandline))
ret = subprocess.Popen(commandline,stdout=subprocess.PIPE)
ret = ret.communicate()[0]
fsolverout = open(solverout,"w")
if ret.find("Models       : 0+")!=-1:
  print("Cant solve anything, sorry")
  fsolverout.close()
  os.remove(instance)
  os.remove(watcher)
  os.remove(solverout)
  sys.exit()
else:
  fsolverout.write(ret)
fsolverout.close()
(fd, check) = tempfile.mkstemp(suffix='.fzn')
fcheck = open(check,"w")
subprocess.call(["head","-n","-1", flatzinc],stdout=fcheck)
fsolverout = open(solverout,"r")
ret = subprocess.call(["./converter.py"],stdin=fsolverout,stdout=fcheck)
if ret==0:
  fsolverout.close()
  subprocess.call(["tail","-n 1", flatzinc],stdout=fcheck)
  fcheck.close()
  x = flatzincUNSAT(check)
  if x:
    print("Problem " + flatzinc + " was SAT, but solution was incorrect")
  else:
    print("Correct")
else:
  x = flatzincUNSAT(flatzinc)
  if x:
    print("Correct")
  else:
    print("Problem " + flatzinc + " was UNSAT, but it should have been SAT")
 
handler(0,0)

