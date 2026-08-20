# cvista perf opts superseded/dropped in the 9.7.0 re-fork (bit-identical; re-apply post-release if profiling warrants)

These are Bucket-1 (byte-identical) perf micro-opts. Taking 9.7's version is parity-safe
(bit-exact with stock 9.7); cvista only loses a speed tweak on that filter. NOT correctness/parity drops.

## Algorithm replaced by 9.7 rewrite — cvista opt no longer applies (leave as 9.7)
- Filters/Core/vtkExtractEdges.cxx      — int32 read lived in the functor 9.7 deleted
- Filters/Core/vtkContour3DLinearGrid.cxx  — 9.7 rewrote (always-merge + Lopez)
- Filters/Core/vtk3DLinearGridPlaneCutter.cxx — 9.7 rewrote (delegates to Contour3DLinearGrid)
- Common/DataModel/vtkTetra.cxx::Clip   — 9.7 shared vtkMarchingCellsClipCases rewrite

## Independent bit-identical perf opt, re-appliable onto 9.7 later (currently take-9.7)
- Filters/Core/vtkContourHelper.cxx/.h  — reusable scratch buffers (OutTriTemp/OutTriDataTemp/PolyCollection)
- Filters/Core/vtkCutter.cxx            — devirtualized double* scalar gather
- Rendering/OpenGL2/vtkOpenGLPointGaussianMapper.cxx — devirtualized color/opacity worker
- Common/DataModel/vtkCellLocator.cxx   — InsideCellBoundsFast now unused (FindCell took 9.7 tol-aware path)
