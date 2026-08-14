// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause

#include "vtkMeshCacheRunner.h"

#include "vtkDataObject.h"
#include "vtkDataObjectMeshCache.h"

//------------------------------------------------------------------------------
vtkMeshCacheRunner::vtkMeshCacheRunner(
  vtkDataObjectMeshCache* cache, vtkDataObject* input, vtkDataObject* output, bool alwaysUpdate)
  : MeshCache(cache)
  , Input(input)
  , Output(output)
  , AlwaysUpdateCache(alwaysUpdate)
{
  if (!this->MeshCache->IsSupportedData(input) || !this->MeshCache->IsSupportedData(output))
  {
    return;
  }

  // ensure input has original ids arrays
  vtkDataObjectMeshCache::CreateTemporaryOriginalIdsArrays(input);
  this->MeshCache->SetOriginalDataObject(input);

  vtkDataObjectMeshCache::Status status = this->MeshCache->GetStatus();
  if (status.enabled())
  {
    this->MeshCache->CopyCacheToDataObject(output);

    this->CacheLoaded = true;
  }
}

//------------------------------------------------------------------------------
void vtkMeshCacheRunner::UpdateCache()
{
  if (!this->CacheLoaded && this->MeshCache->IsSupportedData(this->Output))
  {
    this->MeshCache->UpdateCache(this->Output);
  }
}

//------------------------------------------------------------------------------
bool vtkMeshCacheRunner::GetCacheLoaded()
{
  return this->CacheLoaded;
}

//------------------------------------------------------------------------------
vtkMeshCacheRunner::~vtkMeshCacheRunner()
{
  if (this->AlwaysUpdateCache)
  {
    this->UpdateCache();
  }
  vtkDataObjectMeshCache::CleanupTemporaryOriginalIds(this->Input);
  vtkDataObjectMeshCache::CleanupTemporaryOriginalIds(this->Output);
}
