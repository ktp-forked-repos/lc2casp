#!/usr/bin/python
# converts answer set (only 1 answer must be given, preceded by answer) to flatzinc constraints

import sys



if len(sys.argv)!=1:
    print 'No arguments required'
    sys.exit(1)
f = sys.stdin #open(sys.argv[1],'r')
exit = 1
while(True):
   line = f.readline()
   if not line:
     break
   if (line=='\r\n'):
     continue
   if (line.find('Answer:')!=-1):
     line = f.readline()
     line = line.split()
     for atom in line:
        exit = 0
        if (atom.find('=')!=-1): # int var assignment
	  sys.stdout.write('constraint int_eq(' + atom.replace("=",",") + ");\n")
	else:
	  sys.stdout.write('constraint bool_clause([' +  atom + '],[]);\n')
        #sys.stdout.write('atom ' + atom + '\n')
#f.close()
sys.exit(exit)



