#!/usr/bin/env python
"""Image-based test for vtkPTemporalPathLineFilter.

Uses pre-computed particle positions from 4 streamlines in the office dataset,
distributed across 4 quadrant-based partitions in a vtkPartitionedDataSet.
Particles migrate between partitions over time.
"""
from vtkmodules.vtkCommonCore import vtkIdTypeArray, vtkPoints
from vtkmodules.vtkCommonDataModel import (
    vtkCellArray,
    vtkPartitionedDataSet,
    vtkPolyData,
)
from vtkmodules.vtkCommonExecutionModel import vtkStreamingDemandDrivenPipeline
from vtkmodules.vtkFiltersCore import vtkGlyph3D, vtkTubeFilter
from vtkmodules.vtkFiltersParallel import vtkPTemporalPathLineFilter
from vtkmodules.vtkFiltersSources import vtkSphereSource
from vtkmodules.vtkRenderingCore import (
    vtkActor,
    vtkPolyDataMapper,
    vtkRenderWindow,
    vtkRenderWindowInteractor,
    vtkRenderer,
)
from vtkmodules.util.vtkAlgorithm import VTKPythonAlgorithmBase
from vtkmodules.test import Testing
import vtkmodules.vtkInteractionStyle
import vtkmodules.vtkRenderingFreeType
import vtkmodules.vtkRenderingOpenGL2

NUM_TIMESTEPS = 20

# Pre-computed particle data: (time, [(quadrant, seed_id, x, y, z), ...])
# Generated from office.binary.vtk with 4 seed points across 4 quadrants.
TIMESTEP_DATA = [
  (0.020000, [(0, 0, 0.249988, 0.250187, 0.499963), (0, 1, 0.249883, 2.250054, 1.750301), (0, 2, 2.250026, 0.25008, 1.249976), (1, 3, 2.249491, 3.999471, 0.75001)]),
  (15.985967, [(0, 0, 0.240161, 0.402506, 0.47176), (1, 1, 0.126112, 2.288081, 2.090087), (2, 2, 2.27068, 0.314211, 1.23082), (1, 3, 1.865062, 3.567682, 0.75706)]),
  (31.951933, [(0, 0, 0.215952, 0.619281, 0.462576), (2, 2, 2.291335, 0.378341, 1.211665), (1, 3, 1.517594, 3.054329, 0.749021)]),
  (47.917900, [(0, 0, 0.170435, 0.906569, 0.454046), (2, 2, 2.311989, 0.442472, 1.192509), (1, 3, 1.308351, 2.438746, 0.686241)]),
  (63.883867, [(0, 0, 0.115618, 1.342499, 0.447474), (2, 2, 2.277121, 0.563051, 1.176527), (0, 3, 1.652158, 2.229656, 0.542647)]),
  (79.849834, [(0, 0, 0.165237, 1.83181, 0.373514), (0, 2, 2.219339, 0.706927, 1.161854), (1, 3, 2.063523, 2.276386, 0.385156)]),
  (95.815800, [(0, 0, 0.460969, 1.617364, 0.197918), (0, 2, 2.13339, 0.885818, 1.146199), (3, 3, 2.753573, 2.439463, 0.303883)]),
  (111.781767, [(0, 0, 0.974257, 0.753291, 0.110515), (0, 2, 2.082995, 1.056843, 1.138275), (3, 3, 3.203947, 2.62968, 0.272227)]),
  (127.747734, [(0, 0, 1.451381, 0.074565, 0.329082), (0, 2, 2.128282, 1.185926, 1.147443), (3, 3, 3.563196, 2.932818, 0.2662)]),
  (143.713701, [(0, 0, 1.746713, 0.183396, 0.412151), (0, 2, 2.179507, 1.303138, 1.182198), (3, 3, 3.581585, 3.266462, 0.628699)]),
  (159.679667, [(0, 0, 2.010102, 0.337652, 0.36602), (2, 2, 2.26408, 1.353694, 1.360639), (3, 3, 3.676598, 3.349328, 0.826154)]),
  (175.645634, [(2, 0, 2.271677, 0.467658, 0.329135), (0, 2, 2.232497, 1.357261, 1.857922), (3, 3, 3.656643, 3.27088, 0.861641)]),
  (191.611601, [(2, 0, 2.530424, 0.606346, 0.30729), (0, 2, 2.00066, 1.328097, 2.352807), (3, 3, 3.60958, 3.188387, 0.889442)]),
  (207.577568, [(2, 0, 2.80797, 0.788436, 0.292383), (0, 2, 1.135573, 1.448002, 2.450928), (3, 3, 3.464508, 3.091269, 0.889452)]),
  (223.543534, [(2, 0, 3.309843, 0.77278, 0.398864), (3, 3, 3.332795, 2.995307, 0.889456)]),
  (239.509501, [(2, 0, 3.136272, 0.861986, 0.72802), (3, 3, 3.206312, 2.899797, 0.889456)]),
  (255.475468, [(2, 0, 2.621202, 1.170389, 0.795189), (3, 3, 3.062125, 2.776109, 0.889462)]),
  (271.441435, [(2, 0, 2.300486, 1.495774, 1.377464), (3, 3, 2.899651, 2.628343, 0.890021)]),
  (287.407401, [(0, 0, 1.988891, 1.501024, 2.447383), (3, 3, 2.705544, 2.442532, 0.953002)]),
  (303.373368, [(1, 0, 0.039058, 2.433374, 2.449759), (2, 3, 2.623855, 2.149351, 2.158051)]),
]

TIME_VALUES = [t for t, _ in TIMESTEP_DATA]


def build_timestep(particles):
    """Build a vtkPartitionedDataSet with 4 partitions from particle list."""
    partition_data = [[] for _ in range(4)]
    for quadrant, seed_id, x, y, z in particles:
        partition_data[quadrant].append((seed_id, x, y, z))

    pds = vtkPartitionedDataSet()
    pds.SetNumberOfPartitions(4)
    for q in range(4):
        pd = vtkPolyData()
        pts = vtkPoints()
        ids = vtkIdTypeArray()
        ids.SetName("GlobalIds")
        cells = vtkCellArray()
        for seed_id, x, y, z in partition_data[q]:
            idx = pts.InsertNextPoint(x, y, z)
            ids.InsertNextValue(seed_id)
            cells.InsertNextCell(1)
            cells.InsertCellPoint(idx)
        pd.SetPoints(pts)
        pd.SetVerts(cells)
        pd.GetPointData().SetGlobalIds(ids)
        pds.SetPartition(q, pd)
    return pds


ALL_TIMESTEPS = [build_timestep(particles) for _, particles in TIMESTEP_DATA]


class TemporalParticleSource(VTKPythonAlgorithmBase):
    """Temporal source that outputs pre-computed vtkPartitionedDataSet."""

    def __init__(self):
        VTKPythonAlgorithmBase.__init__(
            self, nInputPorts=0, nOutputPorts=1,
            outputType="vtkPartitionedDataSet",
        )

    def RequestInformation(self, request, inInfo, outInfo):
        info = outInfo.GetInformationObject(0)
        info.Remove(vtkStreamingDemandDrivenPipeline.TIME_STEPS())
        for t in TIME_VALUES:
            info.Append(vtkStreamingDemandDrivenPipeline.TIME_STEPS(), t)
        info.Set(
            vtkStreamingDemandDrivenPipeline.TIME_RANGE(),
            [TIME_VALUES[0], TIME_VALUES[-1]], 2,
        )
        return 1

    def RequestData(self, request, inInfo, outInfo):
        info = outInfo.GetInformationObject(0)
        t = 0.0
        if info.Has(vtkStreamingDemandDrivenPipeline.UPDATE_TIME_STEP()):
            t = info.Get(vtkStreamingDemandDrivenPipeline.UPDATE_TIME_STEP())
        idx = min(range(len(TIME_VALUES)), key=lambda i: abs(TIME_VALUES[i] - t))
        output = self.GetOutputData(outInfo, 0)
        output.ShallowCopy(ALL_TIMESTEPS[idx])
        output.GetInformation().Set(output.DATA_TIME_STEP(), TIME_VALUES[idx])
        return 1


class TestPTemporalPathLineFilter(Testing.vtkTest):
    def testPathLines(self):
        source = TemporalParticleSource()

        filt = vtkPTemporalPathLineFilter()
        filt.SetInputConnection(source.GetOutputPort())
        filt.SetMaxTrackLength(NUM_TIMESTEPS)
        filt.SetMaskPoints(1)
        filt.SetMaxStepDistance(10.0, 10.0, 10.0)
        filt.SetKeepDeadTrails(True)
        filt.Update()

        pathlines = filt.GetOutputDataObject(0)
        particles = filt.GetOutputDataObject(1)

        # Validate output
        self.assertTrue(pathlines.GetNumberOfLines() > 0,
                        "Expected at least one pathline")
        self.assertTrue(particles.GetNumberOfPoints() > 0,
                        "Expected at least one particle")
        self.assertIsNotNone(pathlines.GetPointData().GetArray("TrailId"),
                             "Expected TrailId array")

        # Rendering
        renderer = vtkRenderer()
        renderer.SetBackground(0.1, 0.1, 0.15)

        tubes = vtkTubeFilter()
        tubes.SetInputData(pathlines)
        tubes.SetRadius(0.03)
        tubes.SetNumberOfSides(8)

        pathMapper = vtkPolyDataMapper()
        pathMapper.SetInputConnection(tubes.GetOutputPort())
        pathMapper.SetScalarModeToUsePointFieldData()
        pathMapper.SelectColorArray("TrailId")
        pathMapper.SetScalarRange(0, 3)
        pathMapper.ScalarVisibilityOn()
        pathActor = vtkActor()
        pathActor.SetMapper(pathMapper)
        renderer.AddActor(pathActor)

        sphere = vtkSphereSource()
        sphere.SetRadius(0.06)
        sphere.SetThetaResolution(12)
        sphere.SetPhiResolution(12)

        glyph = vtkGlyph3D()
        glyph.SetInputData(particles)
        glyph.SetSourceConnection(sphere.GetOutputPort())
        glyph.ScalingOff()

        particleMapper = vtkPolyDataMapper()
        particleMapper.SetInputConnection(glyph.GetOutputPort())
        particleMapper.ScalarVisibilityOff()
        particleActor = vtkActor()
        particleActor.SetMapper(particleMapper)
        particleActor.GetProperty().SetColor(1.0, 1.0, 0.0)
        renderer.AddActor(particleActor)

        renWin = vtkRenderWindow()
        renWin.SetMultiSamples(0)
        renWin.AddRenderer(renderer)
        renWin.SetSize(400, 400)

        cam = renderer.GetActiveCamera()
        cam.SetPosition(6.5, 6.5, 6.5)
        cam.SetFocalPoint(2.0, 2.0, 1.0)
        cam.SetViewUp(0, 0, 1)
        renderer.ResetCameraClippingRange()

        renWin.Render()

        img_file = "TestPTemporalPathLineFilter.png"
        Testing.compareImage(renWin, Testing.getAbsImagePath(img_file))
        Testing.interact()

    def testInSitu(self):
        """Test in-situ mode (NO_PRIOR_TEMPORAL_ACCESS) and compare to temporal result."""

        # First run the temporal pipeline to get the reference result
        source = TemporalParticleSource()
        filt = vtkPTemporalPathLineFilter()
        filt.SetInputConnection(source.GetOutputPort())
        filt.SetMaxTrackLength(NUM_TIMESTEPS)
        filt.SetMaskPoints(1)
        filt.SetMaxStepDistance(10.0, 10.0, 10.0)
        filt.SetKeepDeadTrails(True)
        filt.Update()

        refPathlines = vtkPolyData()
        refPathlines.DeepCopy(filt.GetOutputDataObject(0))
        refParticles = vtkPolyData()
        refParticles.DeepCopy(filt.GetOutputDataObject(1))

        # Now run in-situ mode
        insituSource = InSituParticleSource()
        insituSource.SetNoPriorTemporalAccessInformationKey()

        insituFilt = vtkPTemporalPathLineFilter()
        insituFilt.SetInputConnection(insituSource.GetOutputPort())
        insituFilt.SetMaxTrackLength(NUM_TIMESTEPS)
        insituFilt.SetMaskPoints(1)
        insituFilt.SetMaxStepDistance(10.0, 10.0, 10.0)
        insituFilt.SetKeepDeadTrails(True)

        for step in range(NUM_TIMESTEPS):
            insituSource.SetTimestepIndex(step)
            insituSource.Modified()
            insituSource.UpdateInformation()
            insituFilt.UpdateTimeStep(TIME_VALUES[step])

        insituPathlines = insituFilt.GetOutputDataObject(0)
        insituParticles = insituFilt.GetOutputDataObject(1)

        # Compare numerically
        self.assertEqual(insituPathlines.GetNumberOfPoints(),
                         refPathlines.GetNumberOfPoints(),
                         "Point count mismatch")
        self.assertEqual(insituPathlines.GetNumberOfLines(),
                         refPathlines.GetNumberOfLines(),
                         "Line count mismatch")
        self.assertEqual(insituParticles.GetNumberOfPoints(),
                         refParticles.GetNumberOfPoints(),
                         "Particle count mismatch")

        # Compare trail IDs
        refIds = refPathlines.GetPointData().GetArray("TrailId")
        insituIds = insituPathlines.GetPointData().GetArray("TrailId")
        self.assertIsNotNone(insituIds, "Missing TrailId in in-situ output")
        self.assertEqual(refIds.GetNumberOfTuples(),
                         insituIds.GetNumberOfTuples(),
                         "TrailId tuple count mismatch")
        for i in range(refIds.GetNumberOfTuples()):
            self.assertAlmostEqual(refIds.GetTuple1(i), insituIds.GetTuple1(i),
                                   places=5, msg=f"TrailId mismatch at index {i}")

        # Compare point positions
        for i in range(refPathlines.GetNumberOfPoints()):
            rp = refPathlines.GetPoint(i)
            ip = insituPathlines.GetPoint(i)
            for c in range(3):
                self.assertAlmostEqual(rp[c], ip[c], places=5,
                                       msg=f"Point {i} coord {c} mismatch")


class InSituParticleSource(VTKPythonAlgorithmBase):
    """In-situ source: single timestep, advances manually."""

    def __init__(self):
        VTKPythonAlgorithmBase.__init__(
            self, nInputPorts=0, nOutputPorts=1,
            outputType="vtkPartitionedDataSet",
        )
        self.current_index = 0

    def SetTimestepIndex(self, idx):
        self.current_index = idx

    def RequestInformation(self, request, inInfo, outInfo):
        info = outInfo.GetInformationObject(0)
        t = TIME_VALUES[self.current_index]
        info.Set(vtkStreamingDemandDrivenPipeline.TIME_STEPS(), [t], 1)
        info.Set(vtkStreamingDemandDrivenPipeline.TIME_RANGE(), [t, t], 2)
        return 1

    def RequestData(self, request, inInfo, outInfo):
        output = self.GetOutputData(outInfo, 0)
        output.ShallowCopy(ALL_TIMESTEPS[self.current_index])
        output.GetInformation().Set(
            output.DATA_TIME_STEP(), TIME_VALUES[self.current_index]
        )
        return 1


if __name__ == "__main__":
    Testing.main([(TestPTemporalPathLineFilter, 'test')])
