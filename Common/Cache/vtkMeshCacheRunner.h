// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @class vtkMeshCacheRunner
 * @brief An RAII class to easily use vtkDataObjectMeshCache.
 *
 * At construction, it tries to copy the cache to the output, with sanity checks.
 * At destruction, it cleans up temporary data arrays that may have been used
 * and optionally update the cache with output.
 *
 * @sa vtkDataObjectMeshCache
 */
#ifndef vtkMeshCacheRunner_h
#define vtkMeshCacheRunner_h

#include "vtkCommonCacheModule.h" // Export macro

VTK_ABI_NAMESPACE_BEGIN
class vtkDataObject;
class vtkDataObjectMeshCache;

class VTKCOMMONCACHE_EXPORT vtkMeshCacheRunner
{
public:
  /**
   * Initialize the runner.
   * Do sanity checks and try to copy the cache into output.
   * If output was updated from cache, GetCacheLoaded will return true.
   */
  vtkMeshCacheRunner(
    vtkDataObjectMeshCache* cache, vtkDataObject* input, vtkDataObject* output, bool alwaysUpdate);

  /**
   * Cleanup temporary arrays, and update cache if AlwaysUpdateCache is true.
   */
  ~vtkMeshCacheRunner();

  /**
   * Update the cache with output.
   */
  void UpdateCache();

  /**
   * Return true if the cache was loaded into output at construction.
   */
  bool GetCacheLoaded();

private:
  vtkDataObjectMeshCache* MeshCache;
  vtkDataObject* Input;
  vtkDataObject* Output;
  bool AlwaysUpdateCache = false;
  bool CacheLoaded = false;
};

VTK_ABI_NAMESPACE_END
#endif
// VTK-HeaderTest-Exclude: vtkMeshCacheRunner.h
