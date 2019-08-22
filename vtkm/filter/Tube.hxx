//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#include <vtkm/cont/ArrayCopy.h>
#include <vtkm/cont/ArrayHandleIndex.h>
#include <vtkm/cont/ErrorFilterExecution.h>

namespace vtkm
{
namespace filter
{

//-----------------------------------------------------------------------------
inline VTKM_CONT Tube::Tube()
  : vtkm::filter::FilterDataSet<Tube>()
  , Worklet()
{
}

//-----------------------------------------------------------------------------
template <typename Policy>
inline VTKM_CONT vtkm::cont::DataSet Tube::DoExecute(const vtkm::cont::DataSet& input,
                                                     vtkm::filter::PolicyBase<Policy>)
{
  this->Worklet.SetCapping(this->Capping);
  this->Worklet.SetNumberOfSides(this->NumberOfSides);
  this->Worklet.SetRadius(this->Radius);

  vtkm::cont::ArrayHandle<vtkm::Vec3f> newPoints;
  vtkm::cont::CellSetSingleType<> newCells;
  this->Worklet.Run(input.GetCoordinateSystem(this->GetActiveCoordinateSystemIndex()),
                    input.GetCellSet(this->GetActiveCellSetIndex()),
                    newPoints,
                    newCells);

  vtkm::cont::DataSet outData;
  vtkm::cont::CoordinateSystem outCoords("coordinates", newPoints);
  outData.AddCellSet(newCells);
  outData.AddCoordinateSystem(outCoords);
  return outData;
}

//-----------------------------------------------------------------------------
template <typename T, typename StorageType, typename DerivedPolicy>
inline VTKM_CONT bool Tube::DoMapField(vtkm::cont::DataSet& result,
                                       const vtkm::cont::ArrayHandle<T, StorageType>& input,
                                       const vtkm::filter::FieldMetadata& fieldMeta,
                                       vtkm::filter::PolicyBase<DerivedPolicy>)
{
  vtkm::cont::ArrayHandle<T> fieldArray;

  if (fieldMeta.IsPointField())
    fieldArray = this->Worklet.ProcessPointField(input);
  else if (fieldMeta.IsCellField())
    fieldArray = this->Worklet.ProcessCellField(input);
  else
    return false;

  //use the same meta data as the input so we get the same field name, etc.
  result.AddField(fieldMeta.AsField(fieldArray));
  return true;
}
}
} // namespace vtkm::filter
