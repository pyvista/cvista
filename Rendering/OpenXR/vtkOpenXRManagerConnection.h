// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/**
 * @class   vtkOpenXRManagerConnection

 * @brief   OpenXR manager connection no-op implementation
 *
 * Base class defining the connection strategy used by vtkOpenXRManager.
 * It does not perform any operation and will result in vtkOpenXRManager
 * initializing a regular Xr session without any connection.
 *
 * @sa
 * vtkOpenXRManager
 */

#ifndef vtkOpenXRManagerConnection_h
#define vtkOpenXRManagerConnection_h

#include "vtkObject.h"
#include "vtkRenderingOpenXRModule.h" // For export macro

namespace vtk::detail
{
VTK_ABI_NAMESPACE_BEGIN
class vtkOpenXRManager;
VTK_ABI_NAMESPACE_END
}

VTK_ABI_NAMESPACE_BEGIN
class VTKRENDERINGOPENXR_EXPORT vtkOpenXRManagerConnection : public vtkObject
{
public:
  static vtkOpenXRManagerConnection* New();
  vtkTypeMacro(vtkOpenXRManagerConnection, vtkObject);

  /**
   * Function called by vtkOpenXRManager before OpenXR initialization
   */
  virtual bool Initialize() { return true; }

  /**
   * Function called by vtkOpenXRManager after OpenXR initialization
   */
  virtual bool EndInitialize() { return true; }

  /**
   * Return the OpenXR extension name that corresponds to this connection strategy.
   */
  virtual const char* GetExtensionName() { return ""; }

  ///@{
  /**
   * Specify the address to connect to.
   */
  void SetIPAddress(std::string ip) { this->IPAddress = std::move(ip); }
  std::string const& GetIPAddress() const { return this->IPAddress; }
  ///@}

protected:
  vtkOpenXRManagerConnection() = default;
  ~vtkOpenXRManagerConnection() override = default;

  // IP Address to connect to
  std::string IPAddress;

  virtual bool ConnectToRemote(vtk::detail::vtkOpenXRManager& vtkNotUsed(manager)) { return true; }

  friend class vtk::detail::vtkOpenXRManager; // for vtkOpenXRManager::Initialize

private:
  vtkOpenXRManagerConnection(const vtkOpenXRManagerConnection&) = delete;
  void operator=(const vtkOpenXRManagerConnection&) = delete;
};

VTK_ABI_NAMESPACE_END
#endif
