// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "grid_soa.h"
#include "../common/ray.h"
#include "triangle_intersector_pluecker.h"

namespace embree
{
  namespace isa
  {
    class GridSOAIntersector1
    {
    public:
      typedef void Primitive;
      
      class PrecalculationsBase
      { 
      public:
        __forceinline PrecalculationsBase (const Ray& ray, const void* ptr)
          : grid(nullptr) {}
        
      public:
        GridSOA* grid;
      };

      typedef Intersector1Precalculations<PrecalculationsBase> Precalculations;
      
      template<typename Loader>
        static __forceinline void intersect(Ray& ray,
                                            IntersectContext* context, 
                                            const float* const grid_x,
                                            const size_t line_offset,
                                            Precalculations& pre)
      {
        typedef typename Loader::vfloat vfloat;
        const size_t dim_offset    = pre.grid->dim_offset;
        const float* const grid_y  = grid_x + 1 * dim_offset;
        const float* const grid_z  = grid_x + 2 * dim_offset;
        const float* const grid_uv = grid_x + 3 * dim_offset;

        Vec3<vfloat> v0, v1, v2;
        Loader::gather(grid_x,grid_y,grid_z,line_offset,v0,v1,v2);
       
        GridSOA::MapUV<Loader> mapUV(grid_uv,line_offset);
        PlueckerIntersector1<Loader::M> intersector(ray,nullptr);
        intersector.intersect(ray,v0,v1,v2,mapUV,Intersect1EpilogMU<Loader::M,true>(ray,context,pre.grid->geomID,pre.grid->primID));
      };
      
      template<typename Loader>
        static __forceinline bool occluded(Ray& ray,
                                           IntersectContext* context, 
                                           const float* const grid_x,
                                           const size_t line_offset,
                                           Precalculations& pre)
      {
        typedef typename Loader::vfloat vfloat;
        const size_t dim_offset    = pre.grid->dim_offset;
        const float* const grid_y  = grid_x + 1 * dim_offset;
        const float* const grid_z  = grid_x + 2 * dim_offset;
        const float* const grid_uv = grid_x + 3 * dim_offset;

        Vec3<vfloat> v0, v1, v2;
        Loader::gather(grid_x,grid_y,grid_z,line_offset,v0,v1,v2);
        
        GridSOA::MapUV<Loader> mapUV(grid_uv,line_offset);
        PlueckerIntersector1<Loader::M> intersector(ray,nullptr);
        return intersector.intersect(ray,v0,v1,v2,mapUV,Occluded1EpilogMU<Loader::M,true>(ray,context,pre.grid->geomID,pre.grid->primID));
      }
      
      /*! Intersect a ray with the primitive. */
      static __forceinline void intersect(Precalculations& pre, Ray& ray, IntersectContext* context, const Primitive* prim, size_t ty, size_t& lazy_node) 
      {
        const size_t line_offset   = pre.grid->width;
        const float* const grid_x  = pre.grid->decodeLeaf(0,prim);
        
#if defined(__AVX__)
        intersect<GridSOA::Gather3x3>( ray, context, grid_x, line_offset, pre);
#else
        intersect<GridSOA::Gather2x3>(ray, context, grid_x            , line_offset, pre);
        intersect<GridSOA::Gather2x3>(ray, context, grid_x+line_offset, line_offset, pre);
#endif
      }
      
      /*! Test if the ray is occluded by the primitive */
      static __forceinline bool occluded(Precalculations& pre, Ray& ray, IntersectContext* context, const Primitive* prim, size_t ty, size_t& lazy_node) 
      {
        const size_t line_offset   = pre.grid->width;
        const float* const grid_x  = pre.grid->decodeLeaf(0,prim);
        
#if defined(__AVX__)
        return occluded<GridSOA::Gather3x3>( ray, context, grid_x, line_offset, pre);
#else
        if (occluded<GridSOA::Gather2x3>(ray, context, grid_x            , line_offset, pre)) return true;
        if (occluded<GridSOA::Gather2x3>(ray, context, grid_x+line_offset, line_offset, pre)) return true;
#endif
        return false;
      }      
    };

    class GridSOAMBlurIntersector1
    {
    public:
      typedef void Primitive;
      typedef GridSOAIntersector1::PrecalculationsBase PrecalculationsBase;
      typedef Intersector1PrecalculationsMB<PrecalculationsBase> Precalculations;
      
      template<typename Loader>
        static __forceinline void intersect(Ray& ray, const float ftime,
                                            IntersectContext* context, 
                                            const float* const grid_x,
                                            const size_t line_offset,
                                            Precalculations& pre)
      {
        typedef typename Loader::vfloat vfloat;
        const size_t dim_offset    = pre.grid->dim_offset;
        const size_t grid_offset   = pre.grid->gridBytes >> 2;
        const float* const grid_y  = grid_x + 1 * dim_offset;
        const float* const grid_z  = grid_x + 2 * dim_offset;
        const float* const grid_uv = grid_x + 3 * dim_offset;

        Vec3<vfloat> a0, a1, a2;
        Loader::gather(grid_x,grid_y,grid_z,line_offset,a0,a1,a2);

        Vec3<vfloat> b0, b1, b2;
        Loader::gather(grid_x+grid_offset,grid_y+grid_offset,grid_z+grid_offset,line_offset,b0,b1,b2);

        Vec3<vfloat> v0 = lerp(a0,b0,vfloat(ftime));
        Vec3<vfloat> v1 = lerp(a1,b1,vfloat(ftime));
        Vec3<vfloat> v2 = lerp(a2,b2,vfloat(ftime));

        GridSOA::MapUV<Loader> mapUV(grid_uv,line_offset);
        PlueckerIntersector1<Loader::M> intersector(ray,nullptr);
        intersector.intersect(ray,v0,v1,v2,mapUV,Intersect1EpilogMU<Loader::M,true>(ray,context,pre.grid->geomID,pre.grid->primID));
      };
      
      template<typename Loader>
        static __forceinline bool occluded(Ray& ray, const float ftime,
                                           IntersectContext* context, 
                                           const float* const grid_x,
                                           const size_t line_offset,
                                           Precalculations& pre)
      {
        typedef typename Loader::vfloat vfloat;
        const size_t dim_offset    = pre.grid->dim_offset;
        const size_t grid_offset   = pre.grid->gridBytes >> 2;
        const float* const grid_y  = grid_x + 1 * dim_offset;
        const float* const grid_z  = grid_x + 2 * dim_offset;
        const float* const grid_uv = grid_x + 3 * dim_offset;

        Vec3<vfloat> a0, a1, a2;
        Loader::gather(grid_x,grid_y,grid_z,line_offset,a0,a1,a2);

        Vec3<vfloat> b0, b1, b2;
        Loader::gather(grid_x+grid_offset,grid_y+grid_offset,grid_z+grid_offset,line_offset,b0,b1,b2);
       
        Vec3<vfloat> v0 = lerp(a0,b0,vfloat(ftime));
        Vec3<vfloat> v1 = lerp(a1,b1,vfloat(ftime));
        Vec3<vfloat> v2 = lerp(a2,b2,vfloat(ftime));
        
        GridSOA::MapUV<Loader> mapUV(grid_uv,line_offset);
        PlueckerIntersector1<Loader::M> intersector(ray,nullptr);
        return intersector.intersect(ray,v0,v1,v2,mapUV,Occluded1EpilogMU<Loader::M,true>(ray,context,pre.grid->geomID,pre.grid->primID));
      }
      
      /*! Intersect a ray with the primitive. */
      static __forceinline void intersect(Precalculations& pre, Ray& ray, IntersectContext* context, const Primitive* prim, size_t ty, size_t& lazy_node) 
      { 
        float ftime;
        const size_t itime = getTimeSegment(ray.time, float(pre.grid->time_steps-1), ftime);

        const size_t line_offset   = pre.grid->width;
        const float* const grid_x  = pre.grid->decodeLeaf(itime,prim);
        
#if defined(__AVX__)
        intersect<GridSOA::Gather3x3>( ray, ftime, context, grid_x, line_offset, pre);
#else
        intersect<GridSOA::Gather2x3>(ray, ftime, context, grid_x            , line_offset, pre);
        intersect<GridSOA::Gather2x3>(ray, ftime, context, grid_x+line_offset, line_offset, pre);
#endif
      }
      
      /*! Test if the ray is occluded by the primitive */
      static __forceinline bool occluded(Precalculations& pre, Ray& ray, IntersectContext* context, const Primitive* prim, size_t ty, size_t& lazy_node) 
      {
        float ftime;
        const size_t itime = getTimeSegment(ray.time, float(pre.grid->time_steps-1), ftime);

        const size_t line_offset   = pre.grid->width;
        const float* const grid_x  = pre.grid->decodeLeaf(itime,prim);
        
#if defined(__AVX__)
        return occluded<GridSOA::Gather3x3>( ray, ftime, context, grid_x, line_offset, pre);
#else
        if (occluded<GridSOA::Gather2x3>(ray, ftime, context, grid_x            , line_offset, pre)) return true;
        if (occluded<GridSOA::Gather2x3>(ray, ftime, context, grid_x+line_offset, line_offset, pre)) return true;
#endif
        return false;
      }      
    };
  }
}
