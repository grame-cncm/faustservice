'''
Generates an SHA-1 hash from a file.
'''

import sys
import hashlib

def sha1_for_file(f, block_size=2**20):
  sha1 = hashlib.sha1()
  while True:
    data = f.read(block_size)
    if not data:
      break
    sha1.update(data)
  return sha1.hexdigest()

if __name__ == '__main__' :
  try : 
    f = file(sys.argv[1],'r')
    res = sha1_for_file(f)
    f.close()
    sys.stdout.write(res)
    sys.exit(0)
  except Exception as e :
    print e
    sys.exit(1)
