//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#ifndef vtk_m_filter_ParticleAdvection2_h
#define vtk_m_filter_ParticleAdvection2_h

#include <vtkm/Particle.h>
#include <vtkm/filter/NewFilterField.h>
#include <vtkm/filter/NewFilterParticleAdvection.h>
#include <vtkm/filter/particleadvection/ParticleAdvectionTypes.h>

namespace vtkm
{
namespace filter
{
/// \brief Advect particles in a vector field.

/// Takes as input a vector field and seed locations and generates the
/// end points for each seed through the vector field.

class ParticleAdvection2 : public vtkm::filter::NewFilterSteadyStateParticleAdvection
{
public:
  VTKM_CONT ParticleAdvection2()
    : NewFilterSteadyStateParticleAdvection(
        vtkm::filter::particleadvection::ParticleAdvectionResultType::PARTICLE_ADVECT_TYPE)
  {
  }

protected:
  VTKM_CONT vtkm::cont::PartitionedDataSet DoExecutePartitions(
    const vtkm::cont::PartitionedDataSet& inData) override;
};

}
} // namespace vtkm::filter

#ifndef vtk_m_filter_ParticleAdvection2_hxx
#include <vtkm/filter/ParticleAdvection2.hxx>
#endif

#endif // vtk_m_filter_ParticleAdvection2_h
