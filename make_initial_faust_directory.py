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

# to do...basically code dup of validate_faust.py
# perhaps merge?

_SUCCESS = 0
# ERROR CODES
_BAD_FILE_NAME = 1
_UNWELCOME_FILE_IN_ARCHIVE = 2
_MULTIPLE_DSP_FILES_IN_ARCHIVE = 3
_FILE_DOES_NOT_EXIST = 4
_SHA1_ALREADY_PRESENT = 5

FILENAME = sys.argv[1]
SHA1 = sys.argv[2]
ORIGINAL_FILENAME = sys.argv[3]
PATH_TO_SHA1 = ""
try :
  PATH_TO_SHA1 = dsys.argv[4]
except : pass

def _copy_contents_to_real(filename, realdir, original_filename) :
  '''
  Extracts files if need be and copies them to the tmp directory.
  Exits with error code if this doesn't work.
  '''
  # Is it a file?
  if not os.path.exists(filename) :
    sys.exit(_FILE_DOES_NOT_EXIST)
  # Is it a DSP file?
  elif filename.rpartition('.')[2] == 'dsp' :
    os.rename(os.path.join(realdir,filename), os.path.join(realdir,original_filename))
  # Is it an archive?
  elif tarfile.is_tarfile(filename) | zipfile.is_zipfile(filename) :
    is_tarfile = tarfile.is_tarfile(filename)
    archive = (tarfile if is_tarfile else zipfile).open(filename)
    archive.extractall(path=realdir)
    dsp_file = ''
    for subfile in getattr(archive, 'getnames' if is_tarfile else 'namelist')() :
      ext = subfile.rpartition('.')[2]
      if ext not in ['dsp','lib'] :
        sys.exit(_UNWELCOME_FILE_IN_ARCHIVE)
      if (ext == 'dsp') & (dsp_file != '') :
        sys.exit(_MULTIPLE_DSP_FILES_IN_ARCHIVE)
      if (ext == 'dsp') :
        dsp_file = subfile
    archive.close()
  else :
    sys.exit(_BAD_FILE_NAME)

if __name__ == '__main__' :
  try :
    print os.path.join(PATH_TO_SHA1, SHA1)
    os.mkdir(os.path.join(PATH_TO_SHA1, SHA1))
  except :
    sys.exit(_SHA1_ALREADY_PRESENT)
  _copy_contents_to_real(FILENAME, os.path.join(PATH_TO_SHA1, SHA1), ORIGINAL_FILENAME)
  sys.exit(_SUCCESS)
