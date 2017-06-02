//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2017 Sandia Corporation.
//  Copyright 2017 UT-Battelle, LLC.
//  Copyright 2017 Los Alamos National Security.
//
//  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
//  the U.S. Government retains certain rights in this software.
//
//  Under the terms of Contract DE-AC52-06NA25396 with Los Alamos National
//  Laboratory (LANL), the U.S. Government retains certain rights in
//  this software.
//============================================================================
#ifndef vtk_m_cont_tbb_internal_VirtualObjectTransferTBB_h
#define vtk_m_cont_tbb_internal_VirtualObjectTransferTBB_h

#include <vtkm/cont/internal/VirtualObjectTransfer.h>
#include <vtkm/cont/internal/VirtualObjectTransferShareWithControl.h>
#include <vtkm/cont/tbb/internal/DeviceAdapterTagTBB.h>

namespace vtkm
{
namespace cont
{
namespace internal
{

template <typename VirtualObject, typename TargetClass>
struct VirtualObjectTransfer<VirtualObject, TargetClass, vtkm::cont::DeviceAdapterTagTBB>
  : public VirtualObjectTransferShareWithControl<VirtualObject, TargetClass>
{
};
}
}
} // vtkm::cont::internal

#endif // vtk_m_cont_tbb_internal_VirtualObjectTransferTBB_h