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
- Rendering/Core/vtkRenderer.cxx/.h     — persistent PropArrayStorage buffer (avoids per-frame new[]/delete[]
                                          of the prop-traversal array). Traversal order IDENTICAL -> parity-safe.
                                          Found by the drop-detector audit (census PropArrayStorage 4->0); 9.7 did
                                          NOT add it. DEFERRED (perf-only). Re-apply onto 9.7's vtkRenderer.

## Re-audit 2026-08-21 (independent drop-detection re-run) — items the original ledger missed
# Re-ran cvista delta = diff(v9.6.2 -> main) vs refork/vtk-9.7.0-trim. All 218 added files
# still present; 305/306 modified carry the change; vtkSurfaceNets3D correctness fix confirmed
# present (zero-init re-applied). Surfaced 5 flagged files NOT in the lists above — all
# parity-safe (Bucket-1 / build-only), none correctness/parity drops, so release-safe; logged
# here for an accurate record and post-release re-apply:
- Filters/Core/vtkThresholdPoints.cxx        — UseFixedSizeDefaultStorage(1) (skip offsets array; byte-identical)
- Filters/General/vtkImageMarchingCubes.cxx  — UseFixedSizeDefaultStorage(3) (byte-identical)
- Filters/General/vtkVertexGlyphFilter.cxx   — UseFixedSizeDefaultStorage(1) (byte-identical)
- ThirdParty/xdmf2/.../XdmfObject.h          — <strstream>-> <sstream> portability fix; stock 9.7 still
                                               uses <strstream> (builds on the current Linux/macOS/Windows
                                               matrix, so build-safe now; reversion only loses future-proofing)
- ThirdParty/xdmf2/.../XdmfValuesBinary.cxx  — rdbuf()->str() -> .str() (same <sstream> migration)

# ------------------------------------------------------------------------------
# Drop-detector audit (structural verification of the re-fork, layer 1+2)
# ------------------------------------------------------------------------------
# Method: cvista delta = diff(v9.6.2 -> main); re-fork must = v9.7.0 + delta.
#   Layer 1: for each of 297 cvista-modified files, flag where refork == stock v9.7.0.
#   Layer 2: idiom-token census (cvistaCellConnectivity/EnableFast/RunSafeFilterParallel/
#            SortEpoch/cvistaFast/...) main vs refork must match modulo genuine 9.7 additions.
# Result: 297 modified -> 285 clean, 12 flagged, 1 deleted; all 158 added files present.
#   - 9 flagged = intentional (this file's lists above).
#   - vtkShaderProgram.cxx/.h: FALSE POSITIVE -- 9.7 UPSTREAMED the uniform-value cache
#     (stock v9.7.0 has UniformValueCache); cvista's feature is present via 9.7.
#   - cvistaCellConnectivity census -5: all in vtkExtractEdges' functor 9.7 deleted (accounted).
#   - vtkRenderer: real perf drop -> deferred above.
#   - vtkSurfaceNets3D.cxx: real CORRECTNESS drop (PR #176 SMP-determinism zero-init;
#     9.7 did NOT re-fix; bitexact is blind to it -- corpus exercises SurfaceNets2D only).
#     RE-APPLIED to 9.7's rewritten structure (ConfigureOutput newScalars + BoundaryLabels).
#   - vtkGenericDataArrayValueRangeInstantiate.cxx.in: 9.7 deleted; verify obsolete.
# Layer 4 (symbol scan, DONE): nm -D on the 67 built kit .so's from the abi3 release wheel.
#   - 0 unresolved VTK/cvista vtable/typeinfo/New symbols across the kit set (no dropped class
#     referenced-but-undefined; corroborated by the wheel importing + 13024 pyvista tests).
#   - cvista runtime API exported: cvista::FastModeEnabled, cvista::GetSafeFilterThreadingConfig.
#   - cvistaCellConnectivity/cvistaInformationFlatMap and the inline cvista::Fast* templates are
#     header-only (no linkage symbols) -> correctly invisible to nm; covered by layers 2-3.
