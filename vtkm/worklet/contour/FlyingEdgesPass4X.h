
//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================


#ifndef vtk_m_worklet_contour_flyingedges_pass4x_h
#define vtk_m_worklet_contour_flyingedges_pass4x_h


#include <vtkm/worklet/contour/FlyingEdgesHelpers.h>
#include <vtkm/worklet/contour/FlyingEdgesTables.h>

namespace vtkm
{
namespace worklet
{
namespace flying_edges
{

template <typename T>
struct ComputePass4X : public vtkm::worklet::WorkletVisitCellsWithPoints
{

  vtkm::Id3 PointDims;
  vtkm::Vec3f Origin;
  vtkm::Vec3f Spacing;

  T IsoValue;

  vtkm::Id CellWriteOffset;
  vtkm::Id PointWriteOffset;

  ComputePass4X() {}
  ComputePass4X(T value,
                const vtkm::Id3& pdims,
                const vtkm::Vec3f& origin,
                const vtkm::Vec3f& spacing,
                vtkm::Id multiContourCellOffset,
                vtkm::Id multiContourPointOffset)
    : PointDims(pdims)
    , Origin(origin)
    , Spacing(spacing)
    , IsoValue(value)
    , CellWriteOffset(multiContourCellOffset)
    , PointWriteOffset(multiContourPointOffset)
  {
  }

  using ControlSignature = void(CellSetIn,
                                FieldInPoint axis_sums,
                                FieldInPoint axis_mins,
                                FieldInPoint axis_maxs,
                                WholeArrayIn cell_tri_count,
                                WholeArrayIn edgeData,
                                WholeArrayIn data,
                                WholeArrayOut connectivity,
                                WholeArrayOut edgeIds,
                                WholeArrayOut weights,
                                WholeArrayOut inputCellIds,
                                WholeArrayOut points);
  using ExecutionSignature =
    void(ThreadIndices, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, WorkIndex);

  template <typename ThreadIndices,
            typename FieldInPointId3,
            typename FieldInPointId,
            typename WholeTriField,
            typename WholeEdgeField,
            typename WholeDataField,
            typename WholeConnField,
            typename WholeEdgeIdField,
            typename WholeWeightField,
            typename WholeCellIdField,
            typename WholePointField>
  VTKM_EXEC void operator()(const ThreadIndices& threadIndices,
                            const FieldInPointId3& axis_sums,
                            const FieldInPointId& axis_mins,
                            const FieldInPointId& axis_maxs,
                            const WholeTriField& cellTriCount,
                            const WholeEdgeField& edges,
                            const WholeDataField& field,
                            const WholeConnField& conn,
                            const WholeEdgeIdField& interpolatedEdgeIds,
                            const WholeWeightField& weights,
                            const WholeCellIdField& inputCellIds,
                            const WholePointField& points,
                            vtkm::Id oidx) const
  {
    using AxisToSum = SumXAxis;

    //This works as cellTriCount was computed with ScanExtended
    //and therefore has one more entry than the number of cells
    vtkm::Id cell_tri_offset = cellTriCount.Get(oidx);
    vtkm::Id next_tri_offset = cellTriCount.Get(oidx + 1);
    if (cell_tri_offset == next_tri_offset)
    { //we produce nothing
      return;
    }
    cell_tri_offset += this->CellWriteOffset;

    const Pass4TrimState state(
      AxisToSum{}, this->PointDims, threadIndices, axis_mins, axis_maxs, edges);
    if (!state.valid)
    {
      return;
    }

    const vtkm::Id3 pdims = this->PointDims;
    const vtkm::Id3 increments = compute_incs3d(pdims);
    const vtkm::Id startingCellId =
      compute_start(AxisToSum{}, state.ijk, pdims - vtkm::Id3{ 1, 1, 1 });
    vtkm::Id edgeIds[12];

    auto edgeCase = getEdgeCase(edges, state.startPos, (state.axis_inc * state.left));
    init_voxelIds(AxisToSum{}, this->PointWriteOffset, edgeCase, axis_sums, edgeIds);
    for (vtkm::Id i = state.left; i < state.right; ++i) // run along the trimmed voxels
    {
      auto ijk = state.ijk;
      ijk[AxisToSum::xindex] = i;
      edgeCase = getEdgeCase(edges, state.startPos, (state.axis_inc * i));
      vtkm::UInt8 numTris = data::GetNumberOfPrimitives(edgeCase);
      if (numTris > 0)
      {
        //compute what the current cellId is
        vtkm::Id cellId = compute_cell_Id(AxisToSum{}, startingCellId, state.axis_inc, i);

        // Start by generating triangles for this case
        generate_tris(cellId, edgeCase, numTris, edgeIds, cell_tri_offset, conn, inputCellIds);

        // Now generate edgeIds and weights along voxel axes if needed. Remember to take
        // boundary into account.
        vtkm::UInt8 loc = static_cast<vtkm::UInt8>(
          state.yzLoc | (i < 1 ? FlyingEdges3D::MinBoundary
                               : (i >= (pdims[AxisToSum::xindex] - 2) ? FlyingEdges3D::MaxBoundary
                                                                      : FlyingEdges3D::Interior)));
        auto* edgeUses = data::GetEdgeUses(edgeCase);
        if (loc != FlyingEdges3D::Interior || case_includes_axes(edgeUses))
        {
          this->Generate(loc,
                         ijk,
                         field,
                         interpolatedEdgeIds,
                         weights,
                         points,
                         state.startPos,
                         increments,
                         (state.axis_inc * i),
                         edgeUses,
                         edgeIds);
        }
        advance_voxelIds(edgeUses, edgeIds);
      }
    }
  }

  //----------------------------------------------------------------------------
  template <typename WholeDataField,
            typename WholeIEdgeField,
            typename WholeWeightField,
            typename WholePointField>
  VTKM_EXEC inline void Generate(vtkm::UInt8 loc,
                                 const vtkm::Id3& ijk,
                                 const WholeDataField& field,
                                 const WholeIEdgeField& interpolatedEdgeIds,
                                 const WholeWeightField& weights,
                                 const WholePointField& points,
                                 const vtkm::Id4& startPos,
                                 const vtkm::Id3& incs,
                                 vtkm::Id offset,
                                 vtkm::UInt8 const* const edgeUses,
                                 vtkm::Id* edgeIds) const
  {
    using AxisToSum = SumXAxis;

    vtkm::Id2 pos(startPos[0] + offset, 0);
    {
      auto s0 = field.Get(pos[0]);

      //EdgesUses 0,4,8 work for Y axis
      if (edgeUses[0])
      { // edgesUses[0] == i axes edge
        auto writeIndex = edgeIds[0];
        pos[1] = startPos[0] + offset + incs[AxisToSum::xindex];
        auto s1 = field.Get(pos[1]);
        T t = static_cast<T>((this->IsoValue - s0) / (s1 - s0));

        interpolatedEdgeIds.Set(writeIndex, pos);
        weights.Set(writeIndex, static_cast<vtkm::FloatDefault>(t));

        auto coord = this->InterpolateCoordinate(t, ijk, ijk + vtkm::Id3{ 1, 0, 0 });
        points.Set(writeIndex, coord);
      }
      if (edgeUses[4])
      { // edgesUses[4] == j axes edge
        auto writeIndex = edgeIds[4];
        pos[1] = startPos[1] + offset;
        auto s1 = field.Get(pos[1]);
        T t = static_cast<T>((this->IsoValue - s0) / (s1 - s0));

        interpolatedEdgeIds.Set(writeIndex, pos);
        weights.Set(writeIndex, static_cast<vtkm::FloatDefault>(t));

        auto coord = this->InterpolateCoordinate(t, ijk, ijk + vtkm::Id3{ 0, 1, 0 });
        points.Set(writeIndex, coord);
      }
      if (edgeUses[8])
      { // edgesUses[8] == k axes edge
        auto writeIndex = edgeIds[8];
        pos[1] = startPos[2] + offset;
        auto s1 = field.Get(pos[1]);
        T t = static_cast<T>((this->IsoValue - s0) / (s1 - s0));

        interpolatedEdgeIds.Set(writeIndex, pos);
        weights.Set(writeIndex, static_cast<vtkm::FloatDefault>(t));

        auto coord = this->InterpolateCoordinate(t, ijk, ijk + vtkm::Id3{ 0, 0, 1 });
        points.Set(writeIndex, coord);
      }
    }
    // On the boundary cells special work has to be done to cover the partial
    // cell axes. These are boundary situations where the voxel axes is not
    // fully formed. These situations occur on the +x,+y,+z volume
    // boundaries. (The other cases fall through the default: case which is
    // expected.)
    //
    // Note that loc is one of 27 regions in the volume, with (0,1,2)
    // indicating (interior, min, max) along coordinate axes.
    switch (loc)
    {
      case 2:
      case 6:
      case 18:
      case 22: //+x
        this->InterpolateEdge(
          ijk, pos[0], incs, 5, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 9, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        break;
      case 8:
      case 9:
      case 24:
      case 25: //+y
        this->InterpolateEdge(
          ijk, pos[0], incs, 1, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 10, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        break;
      case 32:
      case 33:
      case 36:
      case 37: //+z
        this->InterpolateEdge(
          ijk, pos[0], incs, 2, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 6, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        break;
      case 10:
      case 26: //+x +y
        this->InterpolateEdge(
          ijk, pos[0], incs, 1, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 5, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 9, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 10, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 11, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        break;
      case 34:
      case 38: //+x +z
        this->InterpolateEdge(
          ijk, pos[0], incs, 2, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 5, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 9, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 6, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 7, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        break;
      case 40:
      case 41: //+y +z
        this->InterpolateEdge(
          ijk, pos[0], incs, 1, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 2, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 3, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 6, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 10, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        break;
      case 42: //+x +y +z happens no more than once per volume
        this->InterpolateEdge(
          ijk, pos[0], incs, 1, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 2, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 3, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 5, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 9, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 10, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 11, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 6, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        this->InterpolateEdge(
          ijk, pos[0], incs, 7, edgeUses, edgeIds, field, interpolatedEdgeIds, weights, points);
        break;
      default: // interior, or -x,-y,-z boundaries
        return;
    }
  }

  // Indicate whether voxel axes need processing for this case.
  //----------------------------------------------------------------------------
  template <typename WholeField,
            typename WholeIEdgeField,
            typename WholeWeightField,
            typename WholePointField>
  VTKM_EXEC inline void InterpolateEdge(const vtkm::Id3& ijk,
                                        vtkm::Id currentIdx,
                                        const vtkm::Id3& incs,
                                        vtkm::Id edgeNum,
                                        vtkm::UInt8 const* const edgeUses,
                                        vtkm::Id* edgeIds,
                                        const WholeField& field,
                                        const WholeIEdgeField& interpolatedEdgeIds,
                                        const WholeWeightField& weights,
                                        const WholePointField& points) const
  {
    using AxisToSum = SumXAxis;

    // if this edge is not used then get out
    if (!edgeUses[edgeNum])
    {
      return;
    }
    const vtkm::Id writeIndex = edgeIds[edgeNum];

    // build the edge information
    vtkm::Vec<vtkm::UInt8, 2> verts = data::GetVertMap(edgeNum);

    vtkm::Id3 offsets1 = data::GetVertOffsets(AxisToSum{}, verts[0]);
    vtkm::Id3 offsets2 = data::GetVertOffsets(AxisToSum{}, verts[1]);

    vtkm::Id2 iEdge(currentIdx + vtkm::Dot(offsets1, incs), currentIdx + vtkm::Dot(offsets2, incs));

    interpolatedEdgeIds.Set(writeIndex, iEdge);

    auto s0 = field.Get(iEdge[0]);
    auto s1 = field.Get(iEdge[1]);
    T t = static_cast<T>((this->IsoValue - s0) / (s1 - s0));
    weights.Set(writeIndex, static_cast<vtkm::FloatDefault>(t));

    auto coord = this->InterpolateCoordinate(t, ijk + offsets1, ijk + offsets2);
    points.Set(writeIndex, coord);
  }

  //----------------------------------------------------------------------------
  inline VTKM_EXEC vtkm::Vec3f InterpolateCoordinate(T t,
                                                     const vtkm::Id3& ijk0,
                                                     const vtkm::Id3& ijk1) const
  {
    return vtkm::Vec3f(
      this->Origin[0] +
        this->Spacing[0] * static_cast<vtkm::FloatDefault>(ijk0[0] + t * (ijk1[0] - ijk0[0])),
      this->Origin[1] +
        this->Spacing[1] * static_cast<vtkm::FloatDefault>(ijk0[1] + t * (ijk1[1] - ijk0[1])),
      this->Origin[2] +
        this->Spacing[2] * static_cast<vtkm::FloatDefault>(ijk0[2] + t * (ijk1[2] - ijk0[2])));
  }
};
}
}
}
#endif
