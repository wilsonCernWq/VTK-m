//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//
//  Copyright 2014 Sandia Corporation.
//  Copyright 2014 UT-Battelle, LLC.
//  Copyright 2014 Los Alamos National Security.
//
//  Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
//  the U.S. Government retains certain rights in this software.
//
//  Under the terms of Contract DE-AC52-06NA25396 with Los Alamos National
//  Laboratory (LANL), the U.S. Government retains certain rights in
//  this software.
//============================================================================
#ifndef vtkm_m_worklet_Tetrahedralize_h
#define vtkm_m_worklet_Tetrahedralize_h

#include <vtkm/worklet/tetrahedralize/TetrahedralizeExplicit.h>
#include <vtkm/worklet/tetrahedralize/TetrahedralizeStructured.h>

namespace vtkm {
namespace worklet {

//
// Distribute multiple copies of cell data depending on cells create from original
//
struct DistributeCellData : public vtkm::worklet::WorkletMapField
{
  typedef void ControlSignature(FieldIn<> inIndices,
                                FieldOut<> outIndices);
  typedef void ExecutionSignature(_1, _2);

  typedef vtkm::worklet::ScatterCounting ScatterType;

  VTKM_CONT
  ScatterType GetScatter() const { return this->Scatter; }

  template <typename CountArrayType, typename DeviceAdapter>
  VTKM_CONT
  DistributeCellData(const CountArrayType &countArray,
                     DeviceAdapter device) :
                         Scatter(countArray, device) {  }

  template <typename T>
  VTKM_EXEC
  void operator()(T inputIndex,
                  T &outputIndex) const
  {
    outputIndex = inputIndex;
  }
private:
  ScatterType Scatter;
};

class Tetrahedralize
{
public:
  Tetrahedralize() : OutCellsPerCell() {}

  // Tetrahedralize explicit data set, save number of tetra cells per input
  template <typename DeviceAdapter>
  vtkm::cont::CellSetSingleType<> Run(const vtkm::cont::CellSetExplicit<> &cellSet,
                                      const DeviceAdapter&)
  {
    TetrahedralizeExplicit<DeviceAdapter> worklet;
    return worklet.Run(cellSet, this->OutCellsPerCell); 
  }

  // Tetrahedralize structured data set, save number of tetra cells per input
  template <typename DeviceAdapter>
  vtkm::cont::CellSetSingleType<> Run(const vtkm::cont::CellSetStructured<3> &cellSet,
                                      const DeviceAdapter&)
  {
    TetrahedralizeStructured<DeviceAdapter> worklet;
    return worklet.Run(cellSet, this->OutCellsPerCell); 
  }

  // Using the saved input to output cells, expand cell data
  template <typename T,
            typename StorageType,
            typename DeviceAdapter>
  vtkm::cont::ArrayHandle<T, StorageType> ProcessField(
                                              const vtkm::cont::ArrayHandle<T, StorageType> &input,
                                              const DeviceAdapter& device)
  {
    vtkm::cont::ArrayHandle<T, StorageType> output;

    DistributeCellData distribute(this->OutCellsPerCell, device);
    vtkm::worklet::DispatcherMapField<DistributeCellData, DeviceAdapter> dispatcher(distribute);
    dispatcher.Invoke(input, output);

    return output;
  }

private:
  vtkm::cont::ArrayHandle<vtkm::IdComponent> OutCellsPerCell;
};

}
} // namespace vtkm::worklet

#endif // vtkm_m_worklet_Tetrahedralize_h