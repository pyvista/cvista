from vtkmodules.vtkIONanoVDB import vtkNanoVDBWriter
from vtkmodules.vtkImagingCore import vtkRTAnalyticSource

from vtkmodules.util.misc import vtkGetTempDir

import os
import sys

VTK_TEMP_DIR = vtkGetTempDir()

source = vtkRTAnalyticSource()
writer = vtkNanoVDBWriter()
writer.SetInputConnection(source.GetOutputPort())

fileName = VTK_TEMP_DIR + '/NanoVDBWriter.nvdb'
if os.path.exists(fileName):
    os.remove(fileName)
writer.SetFileName(fileName)
writer.Update()

if not os.path.exists(fileName):
    print('failure: NanoVDB output file was not created: ' + fileName)
    sys.exit(1)

if os.path.getsize(fileName) == 0:
    print('failure: NanoVDB output file is empty: ' + fileName)
    sys.exit(1)

if '--NANOVDB_PRINT_EXE' in sys.argv:
    print("checking the NanoVDB output file")
    nanovdb_print_index = sys.argv.index('--NANOVDB_PRINT_EXE')+1
    stream = os.popen(sys.argv[nanovdb_print_index]+' '+fileName)

    # the output of the stream should look something like:
    # The file "/Users/cory.quammen/build/paraview-master-debug/Testing/Temporary/NanoVDBWriter.nvdb" contains the following grid:
    # #  Name    Type   Class  Version  Codec  Size      File      Scale    # Voxels  Resolution
    # 1  RTData  float  FOG    32.4.2   NONE   2.453 MB  2.453 MB  (1,1,1)  8000      20 x 20 x 20

    stream.readline() # skip the first line
    stream.readline() # skip the second line
    stream.readline() # skip the third line
    data = stream.readline()

    pieces = data.split()
    # we don't compare the file sizes since that could be system or compile dependent
    if pieces[0] != '1' or pieces[1] != 'RTData' or pieces[2] != 'float' or pieces[3] != 'FOG' or pieces[5] != 'NONE' or pieces[10] != '(1,1,1)' or pieces[11] != '9261' or pieces[12] != '21' or pieces[14] != '21' or pieces[16] != '21':
        print("failure: result should look like '1  RTData  float  FOG    32.4.2   NONE   2.453 MB  2.453 MB  (1,1,1)  9261      21 x 21 x 21' " +
              " but is " + data)
        sys.exit(1)

print('success')
