'''
Validates that an upload is actaully a FAUST document or archive.
Returns 0 for success and other error codes for failure (see below).
'''

import sys
import os
import random
import string
import shutil
import tarfile
import zipfile
import subprocess

_SUCCESS = 0
# ERROR CODES
_BAD_FILE_NAME = 1
_UNWELCOME_FILE_IN_ARCHIVE = 2
_MULTIPLE_DSP_FILES_IN_ARCHIVE = 3
_FILE_DOES_NOT_EXIST = 4

# I <3 you, _randString
def _randString(length=16, chars=string.letters):
  first = random.choice(string.letters[26:])
  return first+''.join([random.choice(chars) for i in range(length-1)])

def _cleanup(tmpdir) :
  '''
  Removes the temporary directory created by the script.
  '''
  for x in os.listdir(tmpdir) :
    os.remove(os.path.join(tmpdir,x))
  os.rmdir(tmpdir)

def _copy_contents_to_tmp(filename, tmpdir) :
  '''
  Extracts files if need be and copies them to the tmp directory.
  Exits with error code if this doesn't work.
  '''
  # Is it a file?
  if not os.path.exists(filename) :
    _cleanup(tmpdir)
    sys.exit(_FILE_DOES_NOT_EXIST)
  # Is it a DSP file?
  elif filename.rpartition('.')[2] == 'dsp' :
    shutil.copy(filename, tmpdir)
    return filename
  # Is it an archive?
  elif tarfile.is_tarfile(filename) | zipfile.is_zipfile(filename) :
    is_tarfile = tarfile.is_tarfile(filename)
    archive = (tarfile if is_tarfile else zipfile).open(filename)
    archive.extractall(path=tmpdir)
    dsp_file = ''
    for subfile in getattr(archive, 'getnames' if is_tarfile else 'namelist')() :
      ext = subfile.rpartition('.')[2]
      if ext not in ['dsp','lib'] :
        _cleanup(tmpdir)
        sys.exit(_UNWELCOME_FILE_IN_ARCHIVE)
      if (ext == 'dsp') & (dsp_file != '') :
        _cleanup(tmpdir)
        sys.exit(_MULTIPLE_DSP_FILES_IN_ARCHIVE)
      if (ext == 'dsp') :
        dsp_file = subfile
    archive.close()
    ### TEST
    fff = file(os.path.join(tmpdir,dsp_file),'r')
    aaa = fff.read()
    fff.close()
    return dsp_file
  else :
    _cleanup(tmpdir)
    sys.exit(_BAD_FILE_NAME)

if __name__ == '__main__' :
  filename = sys.argv[1]
  dirname = _randString()
  tmpdir = os.path.join('/tmp',dirname)
  os.mkdir(tmpdir)
  filename = _copy_contents_to_tmp(filename, tmpdir)
  p = subprocess.Popen(["faust", "-a","plot.cpp", filename], cwd=tmpdir, stdout=subprocess.PIPE)
  p.wait()
  #_cleanup(tmpdir)
  sys.exit(p.returncode)
