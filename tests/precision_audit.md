# Output point precision audit (Filters/*)

Goal: a filter with a point-based input (vtkPointSet/PolyData/UnstructuredGrid/StructuredGrid)
must, by DEFAULT, emit output points of the SAME dtype as the input points; honor
`SetOutputPointsPrecision(SINGLE/DOUBLE)` when set. Reference correct impl:
vtkTransformFilter, vtkCleanPolyData, vtkClipClosedSurface, vtkTrimmedExtrusionFilter.

Method: 10 parallel agents read every Filters/*.cxx that creates vtkPoints (184 files).
Filters WITH the OutputPointsPrecision API (77) all use it correctly.

## CONFIRMED BUGS (30) — downcast/mismatch vs input precision

| Filter | Module | Site | Note |
|---|---|---|---|
| vtkBinnedDecimation | Core | 754,1434 | hardcodes VTK_FLOAT (BIN_POINTS/CENTERS/AVERAGES); raw float* write path |
| vtkQuadricClustering | Core | 924,1234 | EndAppend/EndAppendUsingPoints default float |
| vtkStructuredGridOutlineFilter | Core | 57 | copies input grid pts into default-float |
| vtkExtractCellsByType | Extraction | 136 | SetPoint from input into default-float |
| vtkExtractPolyDataGeometry | Extraction | 118 | non-PassPoints branch default-float |
| vtkExtractUnstructuredGrid | Extraction | 169 | default-float; locator init must follow SetDataType |
| vtkApproximatingSubdivisionFilter | General | 73 | base class; weighted input combos to float |
| vtkAxisAlignedReflectionFilter | General | 408,482,522 | 3 branches (ESG/SG/PolyData) default-float |
| vtkBooleanOperationPolyDataFilter | General | 275 | CopyCells default-float |
| vtkBoxClipDataSet | General | 215 | default-float; guard input as vtkPointSet |
| vtkClipConvexPolyData | General | 225 | default-float |
| vtkDataSetTriangleFilter | General | 71 | StructuredExecute default-float (UG path OK) |
| vtkExtractSelectedFrustum | General | 305 | non-PreserveTopology branch default-float |
| vtkIntersectionPolyDataFilter | General | 469 | SplitMesh outputs default-float |
| vtkReflectionUtilities | General | 489 | ProcessUnstructuredGrid default-float (used by vtkReflectionFilter) |
| vtkShrinkFilter | General | 75 | canonical example; default-float |
| vtkSplitByCellScalarFilter | General | 100 | forces DOUBLE (float input -> double out) — reverse mismatch |
| vtkTessellatorFilter | General | 360,434 | default-float (partial: also adds interp pts) |
| vtkStructuredGridGeometryFilter | Geometry | 151,178,252,338 | default-float; SG has explicit double pts |
| vtkUnstructuredGridGeometryFilter | Geometry | 951 | default-float |
| vtkProjectedTerrainPath | Hybrid | 146 | forces DOUBLE; polyline x/y verbatim (reverse) |
| vtkWeightedTransformFilter | Hybrid | 428 | default-float |
| vtkBandedPolyDataContourFilter | Modeling | 441 | default-float; copies input pts verbatim |
| vtkDijkstraGraphGeodesicPath | Modeling | 225 | default-float |
| vtkLinearExtrusionFilter | Modeling | 158 | sweep of input pts to float |
| vtkRibbonFilter | Modeling | 96 | default-float |
| vtkRotationalExtrusionFilter | Modeling | 111 | sweep of input pts to float |
| vtkRuledSurfaceFilter | Modeling | 83 | RESAMPLE branch default-float |
| vtkSubdivideTetra | Modeling | 61 | default-float; keeps original input pts |
| vtkRectilinearGridOutlineFilter (Parallel) | Parallel | 61 | default-float + float bounds[] truncation |

## BORDERLINE / LATENT (note, lower priority)
- vtkExtractEdges (Core): pointset path preserves; locator + non-pointset copy paths emit float.
- vtkContourLoopExtraction (Modeling): CleanPoints=true rewrite uses default-float newPts.

## NOT A BUG
- ~40 SOURCE_NO_POINT_INPUT: image/rectilinear/hypertree/table sources + glyphs-from-source
  + computed geometry (flying edges, marching cubes, synchronized templates, surface nets,
  streamlines, voronoi, sphere tree, cell centers, OBB, terrain-from-image, HTG filters).
  No input vtkPoints to match; precision is a standalone choice (many already SetDataTypeToDouble).
- ~25 OK_PRESERVES (ShallowCopy/DeepCopy/SetData/SetPoints(input)/SetDataType-from-input) or
  HAS_API_OK or SCRATCH_ONLY.

## Fix plan
1. Add full `OutputPointsPrecision` API (ivar default DEFAULT_PRECISION + Set/Get + PrintSelf +
   ctor init) to each buggy filter lacking it; DEFAULT -> SetDataType(inputPoints->GetDataType()),
   SINGLE -> VTK_FLOAT, DOUBLE -> VTK_DOUBLE. Reverse-mismatch filters (Split/ProjectedTerrain)
   replace the hardcoded SetDataTypeToDouble with the same 3-way branch.
2. Bitexact gate: `corrects_stock` op annotation — affected cases assert output dtype == input
   dtype (the CORRECT behavior) instead of byte-matching stock; rest stays byte-exact vs stock.
3. Add float64-input regression ops asserting float64 output for representative fixed filters.
