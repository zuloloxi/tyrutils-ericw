/*  Copyright (C) 1996-1997  Id Software, Inc.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 
 See file, 'COPYING', for details.
 */

#include <common/bsputils.hh>
#include <assert.h>

#include <glm/glm.hpp>

int Face_GetNum(const bsp2_t *bsp, const bsp2_dface_t *f)
{
    return f - bsp->dfaces;
}

/* small helper that just retrieves the correct vertex from face->surfedge->edge lookups */
int Face_VertexAtIndex(const bsp2_t *bsp, const bsp2_dface_t *f, int v)
{
    Q_assert(v >= 0);
    Q_assert(v < f->numedges);
    
    int edge = f->firstedge + v;
    edge = bsp->dsurfedges[edge];
    if (edge < 0)
        return bsp->dedges[-edge].v[1];
    return bsp->dedges[edge].v[0];
}

static void
Vertex_GetPos(const bsp2_t *bsp, int num, vec3_t out)
{
    Q_assert(num >= 0 && num < bsp->numvertexes);
    const dvertex_t *v = &bsp->dvertexes[num];
    
    for (int i=0; i<3; i++)
        out[i] = v->point[i];
}

void
Face_Normal(const bsp2_t *bsp, const bsp2_dface_t *f, vec3_t norm)
{
    if (f->side)
        VectorSubtract(vec3_origin, bsp->dplanes[f->planenum].normal, norm);
    else
        VectorCopy(bsp->dplanes[f->planenum].normal, norm);
}

plane_t
Face_Plane(const bsp2_t *bsp, const bsp2_dface_t *f)
{
    const int vertnum = Face_VertexAtIndex(bsp, f, 0);
    vec3_t vertpos;
    Vertex_GetPos(bsp, vertnum, vertpos);
    
    plane_t res;
    Face_Normal(bsp, f, res.normal);
    res.dist = DotProduct(vertpos, res.normal);
    return res;
}

//FIXME: Any reason to prefer this implementation vs the above one?
#if 0
static void GetFaceNormal(const bsp2_t *bsp, const bsp2_dface_t *face, plane_t *plane)
{
    const dplane_t *dplane = &bsp->dplanes[face->planenum];
    
    if (face->side) {
        VectorSubtract(vec3_origin, dplane->normal, plane->normal);
        plane->dist = -dplane->dist;
    } else {
        VectorCopy(dplane->normal, plane->normal);
        plane->dist = dplane->dist;
    }
}
#endif

const texinfo_t *Face_Texinfo(const bsp2_t *bsp, const bsp2_dface_t *face)
{
    if (face->texinfo < 0 || face->texinfo >= bsp->numtexinfo)
        return nullptr;

    return &bsp->texinfo[face->texinfo];
}

const miptex_t *
Face_Miptex(const bsp2_t *bsp, const bsp2_dface_t *face)
{
    if (!bsp->texdatasize)
        return nullptr;
    
    const texinfo_t *texinfo = Face_Texinfo(bsp, face);
    if (texinfo == nullptr)
        return nullptr;
    
    const int texnum = texinfo->miptex;
    const dmiptexlump_t *miplump = bsp->dtexdata.header;
    
    int offset = miplump->dataofs[texnum];
    if (offset < 0)
        return NULL; //sometimes the texture just wasn't written. including its name.
    
    const miptex_t *miptex = (miptex_t*)(bsp->dtexdata.base + offset);
    return miptex;
}

const char *
Face_TextureName(const bsp2_t *bsp, const bsp2_dface_t *face)
{
    const miptex_t *miptex = Face_Miptex(bsp, face);
    if (miptex)
        return miptex->name;
    else
        return "";
}

bool Face_IsLightmapped(const bsp2_t *bsp, const bsp2_dface_t *face)
{
    const texinfo_t *texinfo = Face_Texinfo(bsp, face);
    if (texinfo == nullptr)
        return false;
    
    if (texinfo->flags & TEX_SPECIAL)
        return false;
    
    return true;
}

const float *GetSurfaceVertexPoint(const bsp2_t *bsp, const bsp2_dface_t *f, int v)
{
    return bsp->dvertexes[Face_VertexAtIndex(bsp, f, v)].point;
}

int
TextureName_Contents(const char *texname)
{
    if (!Q_strncasecmp(texname, "sky", 3))
        return CONTENTS_SKY;
    else if (!Q_strncasecmp(texname, "*lava", 5))
        return CONTENTS_LAVA;
    else if (!Q_strncasecmp(texname, "*slime", 6))
        return CONTENTS_SLIME;
    else if (texname[0] == '*')
        return CONTENTS_WATER;
    
    return CONTENTS_SOLID;
}

int
Face_Contents(const bsp2_t *bsp, const bsp2_dface_t *face)
{
    const char *texname = Face_TextureName(bsp, face);
    return TextureName_Contents(texname);
}

const dmodel_t *BSP_DModelForModelString(const bsp2_t *bsp, const std::string &submodel_str)
{
    int submodel = -1;
    if (1 == sscanf(submodel_str.c_str(), "*%d", &submodel)) {
        
        if (submodel < 0 || submodel >= bsp->nummodels) {
            return nullptr;
        }
        
        return &bsp->dmodels[submodel];
        
    }
    return nullptr;
}

vec_t Plane_Dist(const vec3_t point, const dplane_t *plane)
{
    switch (plane->type)
    {
        case PLANE_X: return point[0] - plane->dist;
        case PLANE_Y: return point[1] - plane->dist;
        case PLANE_Z: return point[2] - plane->dist;
        default: return DotProduct(point, plane->normal) - plane->dist;
    }
}

static bool Light_PointInSolid_r(const bsp2_t *bsp, int nodenum, const vec3_t point )
{
    if (nodenum < 0) {
        bsp2_dleaf_t *leaf = bsp->dleafs + (-1 - nodenum);
        
        return leaf->contents == CONTENTS_SOLID
        || leaf->contents == CONTENTS_SKY;
    }
    
    const bsp2_dnode_t *node = &bsp->dnodes[nodenum];
    vec_t dist = Plane_Dist(point, &bsp->dplanes[node->planenum]);
    
    if (dist > 0.1)
        return Light_PointInSolid_r(bsp, node->children[0], point);
    else if (dist < -0.1)
        return Light_PointInSolid_r(bsp, node->children[1], point);
    else {
        // too close to the plane, check both sides
        return Light_PointInSolid_r(bsp, node->children[0], point)
        || Light_PointInSolid_r(bsp, node->children[1], point);
    }
}

// Tests model 0 of the given model
bool Light_PointInSolid(const bsp2_t *bsp, const dmodel_t *model, const vec3_t point)
{
    return Light_PointInSolid_r(bsp, model->headnode[0], point);
}

bool Light_PointInWorld(const bsp2_t *bsp, const vec3_t point)
{
    return Light_PointInSolid(bsp, &bsp->dmodels[0], point);
}

plane_t *
Face_AllocInwardFacingEdgePlanes(const bsp2_t *bsp, const bsp2_dface_t *face)
{
    plane_t *out = (plane_t *)calloc(face->numedges, sizeof(plane_t));
    
    const plane_t faceplane = Face_Plane(bsp, face);
    for (int i=0; i<face->numedges; i++)
    {
        plane_t *dest = &out[i];
        
        const vec_t *v0 = GetSurfaceVertexPoint(bsp, face, i);
        const vec_t *v1 = GetSurfaceVertexPoint(bsp, face, (i+1)%face->numedges);
        
        vec3_t edgevec;
        VectorSubtract(v1, v0, edgevec);
        VectorNormalize(edgevec);
        
        CrossProduct(edgevec, faceplane.normal, dest->normal);
        dest->dist = DotProduct(dest->normal, v0);
    }
    
    return out;
}

bool
EdgePlanes_PointInside(const bsp2_dface_t *face, const plane_t *edgeplanes, const vec3_t point)
{
    for (int i=0; i<face->numedges; i++) {
        vec_t planedist = DotProduct(point, edgeplanes[i].normal) - edgeplanes[i].dist;
        if (planedist < 0) {
            return false;
        }
    }
    return true;
}

// glm stuff

using namespace glm;

glm::vec4 Face_Plane_E(const bsp2_t *bsp, const bsp2_dface_t *f)
{
    const vec3 p0 = Face_PointAtIndex_E(bsp, f, 0);
    const vec3 norm = Face_Normal_E(bsp, f);
    const vec4 plane(norm, dot(norm, p0));
    return plane;
}

glm::vec3 Face_PointAtIndex_E(const bsp2_t *bsp, const bsp2_dface_t *f, int v)
{
    return Vertex_GetPos_E(bsp, Face_VertexAtIndex(bsp, f, v));
}

glm::vec3 Vertex_GetPos_E(const bsp2_t *bsp, int num)
{
    vec3_t temp;
    Vertex_GetPos(bsp, num, temp);
    return vec3_t_to_glm(temp);
}

glm::vec3 Face_Normal_E(const bsp2_t *bsp, const bsp2_dface_t *f)
{
    vec3_t temp;
    Face_Normal(bsp, f, temp);
    return vec3_t_to_glm(temp);
}

std::vector<glm::vec3> GLM_FacePoints(const bsp2_t *bsp, const bsp2_dface_t *f)
{
    std::vector<glm::vec3> points;
    for (int j = 0; j < f->numedges; j++) {
        points.push_back(Face_PointAtIndex_E(bsp, f, j));
    }
    return points;
}

glm::vec3 Face_Centroid(const bsp2_t *bsp, const bsp2_dface_t *face)
{
    return GLM_PolyCentroid(GLM_FacePoints(bsp, face));
}
