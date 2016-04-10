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

#include <light/light.h>

typedef struct tnode_s {
    vec3_t normal;
    vec_t dist;
    int type;
    int children[2];
    const dplane_t *plane;
    const bsp2_dleaf_t *childleafs[2];
    const bsp2_dnode_t *node;
} tnode_t;

typedef struct faceinfo_s {
    int numedges;
    plane_t *edgeplanes;
    
    // sphere culling
    vec3_t origin;
    vec_t radiusSquared;
    
    int content;
    plane_t plane;
} faceinfo_t;

static tnode_t *tnodes;
static tnode_t *tnode_p;
static const bsp2_t *bsp_static;

static faceinfo_t *faceinfos;

// from hmap2
#define PlaneDiff(point,plane) (((plane)->type < 3 ? (point)[(plane)->type] : DotProduct((point), (plane)->normal)) - (plane)->dist)

/*
==============
Light_PointInLeaf
 
from hmap2
==============
*/
bsp2_dleaf_t *Light_PointInLeaf( const vec3_t point )
{
    int num = 0;
    
    while( num >= 0 )
        num = bsp_static->dnodes[num].children[PlaneDiff(point, &bsp_static->dplanes[bsp_static->dnodes[num].planenum]) < 0];
    
    return bsp_static->dleafs + (-1 - num);
}

/*
==============
Light_PointContents

from hmap2
==============
*/
int Light_PointContents( const vec3_t point )
{
    return Light_PointInLeaf(point)->contents;
}

/*
 * ==============
 * MakeTnodes
 * Converts the disk node structure into the efficient tracing structure
 * ==============
 */
static void
MakeTnodes_r(int nodenum, const bsp2_t *bsp)
{
    tnode_t *tnode;
    int i;
    bsp2_dnode_t *node;
    bsp2_dleaf_t *leaf;

    tnode = tnode_p++;

    node = bsp->dnodes + nodenum;
    tnode->plane = bsp->dplanes + node->planenum;
    tnode->node = node;

    tnode->type = tnode->plane->type;
    VectorCopy(tnode->plane->normal, tnode->normal);
    tnode->dist = tnode->plane->dist;

    for (i = 0; i < 2; i++) {
        if (node->children[i] < 0) {
            leaf = &bsp->dleafs[-node->children[i] - 1];
            tnode->children[i] = leaf->contents;
            tnode->childleafs[i] = leaf;
        } else {
            tnode->children[i] = tnode_p - tnodes;
            MakeTnodes_r(node->children[i], bsp);
        }
    }
}

vec_t *GetSurfaceVertexPoint(const bsp2_t *bsp, const bsp2_dface_t *f, int v);

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

static inline bool SphereCullPoint(const faceinfo_t *info, const vec3_t point)
{
    vec3_t delta;
    vec_t deltaLengthSquared;
    VectorSubtract(point, info->origin, delta);
    deltaLengthSquared = DotProduct(delta, delta);
    return deltaLengthSquared > info->radiusSquared;
}

static int
Face_Contents(const bsp2_t *bsp, const bsp2_dface_t *face)
{
    if (face->texinfo < 0)
        return CONTENTS_SOLID;
    
    if (!(bsp->texinfo[face->texinfo].flags & TEX_SPECIAL))
        return CONTENTS_SOLID;
    
    if (!bsp->texdatasize)
        return CONTENTS_SOLID; // no textures in bsp

    int texnum = bsp->texinfo[face->texinfo].miptex;
    const dmiptexlump_t *miplump = bsp->dtexdata.header;
    const miptex_t *miptex;
    
    if (!miplump->dataofs[texnum])
        return CONTENTS_SOLID; //sometimes the texture just wasn't written. including its name.
    
    miptex = (miptex_t*)(bsp->dtexdata.base + miplump->dataofs[texnum]);
    
    if (!Q_strncasecmp(miptex->name, "sky", 3))
        return CONTENTS_SKY;
    else if (!Q_strncasecmp(miptex->name, "*lava", 5))
        return CONTENTS_LAVA;
    else if (!Q_strncasecmp(miptex->name, "*slime", 6))
        return CONTENTS_SLIME;
    else if (miptex->name[0] == '*')
        return CONTENTS_WATER;
    else
        return CONTENTS_SOLID;
}

void
MakeFaceInfo(const bsp2_t *bsp, const bsp2_dface_t *face, faceinfo_t *info)
{
    info->numedges = face->numedges;
    info->edgeplanes = calloc(face->numedges, sizeof(plane_t));
    
    GetFaceNormal(bsp, face, &info->plane);
    
    // make edge planes
    for (int i=0; i<face->numedges; i++)
    {
        plane_t *dest = &info->edgeplanes[i];
        
        const vec_t *v0 = GetSurfaceVertexPoint(bsp, face, i);
        const vec_t *v1 = GetSurfaceVertexPoint(bsp, face, (i+1)%face->numedges);
        
        vec3_t edgevec;
        VectorSubtract(v1, v0, edgevec);
        VectorNormalize(edgevec);
        
        CrossProduct(edgevec, info->plane.normal, dest->normal);
        
        dest->dist = DotProduct(dest->normal, v0);
    }
    
    // make sphere that bounds the face
    vec3_t centroid = {0,0,0};
    for (int i=0; i<face->numedges; i++)
    {
        const vec_t *v = GetSurfaceVertexPoint(bsp, face, i);
        VectorAdd(centroid, v, centroid);
    }
    VectorScale(centroid, 1.0f/face->numedges, centroid);
    VectorCopy(centroid, info->origin);
    
    // calculate radius
    vec_t maxRadiusSq = 0;
    for (int i=0; i<face->numedges; i++)
    {
        vec3_t delta;
        vec_t radiusSq;
        const vec_t *v = GetSurfaceVertexPoint(bsp, face, i);
        VectorSubtract(v, centroid, delta);
        radiusSq = DotProduct(delta, delta);
        if (radiusSq > maxRadiusSq)
            maxRadiusSq = radiusSq;
    }
    info->radiusSquared = maxRadiusSq;
    
    info->content = Face_Contents(bsp, face);
    
#if 0
    //test
    for (int i=0; i<face->numedges; i++)
    {
        const vec_t *v = GetSurfaceVertexPoint(bsp, face, i);
        assert(!SphereCullPoint(info, v));
    }
    //test
    {
        vec_t radius = sqrt(maxRadiusSq);
        radius ++;
        
        vec3_t test;
        vec3_t n = {1, 0, 0};
        VectorMA(centroid, radius, n, test);
        
        assert(SphereCullPoint(info, test));
    }
#endif
}

void
MakeTnodes(const bsp2_t *bsp)
{
    int i;
    bsp_static = bsp;
    tnode_p = tnodes = malloc(bsp->numnodes * sizeof(tnode_t));
    for (i = 0; i < bsp->nummodels; i++)
        MakeTnodes_r(bsp->dmodels[i].headnode[0], bsp);
    
    faceinfos = malloc(bsp->numfaces * sizeof(faceinfo_t));
    for (i = 0; i < bsp->numfaces; i++)
        MakeFaceInfo(bsp, &bsp->dfaces[i], &faceinfos[i]);
}

/*
 * ============================================================================
 * FENCE TEXTURE TESTING
 * ============================================================================
 */

static miptex_t *
MiptexForFace(const bsp2_dface_t *face, const bsp2_t *bsp)
{
    const texinfo_t *tex;
    dmiptexlump_t *miplump = bsp->dtexdata.header;
    miptex_t *miptex; 
    tex = &bsp->texinfo[face->texinfo];    

    miptex = (miptex_t*)(bsp->dtexdata.base + miplump->dataofs[tex->miptex]);
    return miptex;
}

vec_t fix_coord(vec_t in, int width)
{
    if (in > 0)
    {
        return (int)in % width;
    }
    else
    {
        vec_t in_abs = fabs(in);
        int in_abs_mod = (int)in_abs % width;
        return width - in_abs_mod;
    }
}

static int
SampleTexture(const bsp2_dface_t *face, const bsp2_t *bsp, const vec3_t point)
{
    vec_t texcoord[2];
    const texinfo_t *tex;
    dmiptexlump_t *miplump = bsp->dtexdata.header;
    miptex_t *miptex; 
    int x, y;
    byte *data;
    int sample;

    tex = &bsp->texinfo[face->texinfo];

    WorldToTexCoord(point, tex, texcoord);

    if (miplump->dataofs[tex->miptex] == -1) {
        return -1;
    }

    miptex = (miptex_t*)(bsp->dtexdata.base + miplump->dataofs[tex->miptex]);

    x = fix_coord(texcoord[0], miptex->width);
    y = fix_coord(texcoord[1], miptex->height);

    data = (byte*)miptex + miptex->offsets[0];
    sample = data[(miptex->width * y) + x];

    return sample;
}

/* assumes point is on the same plane as face */
static inline qboolean
TestHitFace(const faceinfo_t *fi, const vec3_t point)
{
    for (int i=0; i<fi->numedges; i++)
    {
        /* faces toward the center of the face */
        const plane_t *edgeplane = &fi->edgeplanes[i];
        
        vec_t dist = DotProduct(point, edgeplane->normal) - edgeplane->dist;
        if (dist < 0)
            return false;
    }
    return true;
}

static inline bsp2_dface_t *
SearchNodeForHitFace(const bsp2_dnode_t *bspnode, const vec3_t point)
{
    // search the faces on this node
    int i;
    for (i=0; i<bspnode->numfaces; i++)
    {
        int facenum = bspnode->firstface + i;
        const faceinfo_t *fi = &faceinfos[facenum];
        
        if (SphereCullPoint(fi, point))
            continue;
        
        if (TestHitFace(fi, point)) {
            return &bsp_static->dfaces[facenum];
        }
    }
    return NULL;
}

/*
 * ============================================================================
 * LINE TRACING
 * The major lighting operation is a point to point visibility test, performed
 * by recursive subdivision of the line by the BSP tree.
 * ============================================================================
 */

typedef struct {
    vec3_t back;
    vec3_t front;
    int node;
    int side;
    const dplane_t *plane;
} tracestack_t;

/*
 * ==============
 * TraceLine
 * ==============
 */
#define MAX_TSTACK 128
int
TraceLine(const dmodel_t *model, const int traceflags,
          const vec3_t start, const vec3_t stop, tracepoint_t *hitpoint)
{
    int node, side, tracehit;
    vec3_t front, back;
    vec_t frontdist, backdist;
    tracestack_t tracestack[MAX_TSTACK];
    tracestack_t *tstack, *crossnode;
    tnode_t *tnode;
//    const tracestack_t *const tstack_max = tracestack + MAX_TSTACK;
    
    if (traceflags <= 0)
        Error("Internal error: %s - bad traceflags (%d)",
              __func__, traceflags);

    VectorCopy(start, front);
    VectorCopy(stop, back);

    tstack = tracestack;
    node = model->headnode[0];
    crossnode = NULL;
    tracehit = TRACE_HIT_NONE;

    while (1) {
        while (node < 0) {
            switch (node) {
            case CONTENTS_SOLID:
                if (traceflags & TRACE_HIT_SOLID)
                    tracehit = TRACE_HIT_SOLID;
                break;
            case CONTENTS_WATER:
                if (traceflags & TRACE_HIT_WATER)
                    tracehit = TRACE_HIT_WATER;
                break;
            case CONTENTS_SLIME:
                if (traceflags & TRACE_HIT_SLIME)
                    tracehit = TRACE_HIT_SLIME;
                break;
            case CONTENTS_LAVA:
                if (traceflags & TRACE_HIT_LAVA)
                    tracehit = TRACE_HIT_LAVA;
                break;
            case CONTENTS_SKY:
                if (traceflags & TRACE_HIT_SKY)
                    tracehit = TRACE_HIT_SKY;
                break;
            default:
                break;
            }
            if (tracehit != TRACE_HIT_NONE) {
                /* If we haven't crossed, start was inside flagged contents */
                if (!crossnode)
                    return -tracehit;

                if (hitpoint) {
                    hitpoint->dplane = crossnode->plane;
                    hitpoint->side = crossnode->side;
                    VectorCopy(crossnode->front, hitpoint->point);
                }

                return tracehit;
            }

            /* If the stack is empty, no obstructions were hit */
            if (tstack == tracestack)
                return TRACE_HIT_NONE;

            /* Pop the stack and go down the back side */
            crossnode = --tstack;
            VectorCopy(tstack->front, front);
            VectorCopy(tstack->back, back);
            node = tnodes[tstack->node].children[!tstack->side];
        }

        tnode = &tnodes[node];
        switch (tnode->type) {
        case PLANE_X:
            frontdist = front[0] - tnode->dist;
            backdist = back[0] - tnode->dist;
            break;
        case PLANE_Y:
            frontdist = front[1] - tnode->dist;
            backdist = back[1] - tnode->dist;
            break;
        case PLANE_Z:
            frontdist = front[2] - tnode->dist;
            backdist = back[2] - tnode->dist;
            break;
        default:
            frontdist = DotProduct(front, tnode->normal) - tnode->dist;
            backdist = DotProduct(back, tnode->normal) - tnode->dist;
            break;
        }

        if (frontdist >= -ON_EPSILON && backdist >= -ON_EPSILON) {
            node = tnode->children[0];
            continue;
        }
        if (frontdist < ON_EPSILON && backdist < ON_EPSILON) {
            node = tnode->children[1];
            continue;
        }

        /*
         * If we get here, we have a clean split with front and back on
         * opposite sides. The new back is the intersection point with the
         * node plane. Push the other segment onto the stack and continue.
         */
        side = frontdist < 0;
        tstack->node = node;
        tstack->side = side;
        tstack->plane = tnode->plane;
        VectorCopy(back, tstack->back);
        VectorSubtract(back, front, back);
        VectorMA(front, frontdist / (frontdist - backdist), back, back);
        VectorCopy(back, tstack->front);
        crossnode = tstack++;
        node = tnode->children[side];
    }
}

qboolean
TestLight(const vec3_t start, const vec3_t stop, const dmodel_t *self)
{
    const modelinfo_t *const *model;
    const int traceflags = TRACE_HIT_SOLID;
    int result = TRACE_HIT_NONE;

    /* Check against the list of global shadow casters */
    for (model = tracelist; *model; model++) {
        if ((*model)->model == self)
            continue;
        result = TraceLine((*model)->model, traceflags, start, stop, NULL);
        if (result != TRACE_HIT_NONE)
            break;
    }

    /* If not yet obscured, check against the self-shadow model */
    if (result == TRACE_HIT_NONE && self)
        result = TraceLine(self, traceflags, start, stop, NULL);

    return (result == TRACE_HIT_NONE);
}

qboolean
TestSky(const vec3_t start, const vec3_t dirn, const dmodel_t *self)
{
    const modelinfo_t *const *model;
    int traceflags = TRACE_HIT_SKY | TRACE_HIT_SOLID;
    int result = TRACE_HIT_NONE;
    vec3_t stop;
    tracepoint_t hit;

    /* Trace towards the sunlight for a sky brush */
    VectorAdd(dirn, start, stop);
    result = TraceLine(tracelist[0]->model, traceflags, start, stop, &hit);
    if (result != TRACE_HIT_SKY)
        return false;

    /* If good, check it isn't shadowed by another model */
    traceflags = TRACE_HIT_SOLID;
    for (model = tracelist + 1; *model; model++) {
        if ((*model)->model == self)
            continue;
        result = TraceLine((*model)->model, traceflags, start, hit.point, NULL);
        if (result != TRACE_HIT_NONE)
            return false;
    }

    /* Check for self-shadowing */
    if (self) {
        result = TraceLine(self, traceflags, start, hit.point, NULL);
        if (result != TRACE_HIT_NONE)
            return false;
    }

    return true;
}


/*
=============
TraceFaces
 
From lordhavoc, johnfitz (RecursiveLightPoint)
=============
*/
bool TraceFaces (traceinfo_t *ti, int node, const vec3_t start, const vec3_t end)
{
    float		front, back, frac;
    vec3_t		mid;
    tnode_t             *tnode;
    
    if (node < 0)
        return false;		// didn't hit anything
    
    tnode = &tnodes[node]; //ericw
    
    // calculate mid point
    if (tnode->type < 3)
    {
        front = start[tnode->type] - tnode->dist;
        back = end[tnode->type] - tnode->dist;
    }
    else
    {
        front = DotProduct(start, tnode->normal) - tnode->dist;
        back = DotProduct(end, tnode->normal) - tnode->dist;
    }
    
    if ((back < 0) == (front < 0))
        return TraceFaces (ti, tnode->children[front < 0], start, end);
    
    frac = front / (front-back);
    mid[0] = start[0] + (end[0] - start[0])*frac;
    mid[1] = start[1] + (end[1] - start[1])*frac;
    mid[2] = start[2] + (end[2] - start[2])*frac;
    
    // go down front side
    if (TraceFaces (ti, tnode->children[front < 0], start, mid))
        return true;	// hit something
    else
    {
        // check for impact on this node
        VectorCopy (mid, ti->point);
        //ti->lightplane = tnode->plane;
        
        bsp2_dface_t *face = SearchNodeForHitFace(tnode->node, mid);
        if (face) {
            int facenum = face - bsp_static->dfaces;
            const faceinfo_t *fi = &faceinfos[facenum];
            
            // only solid and sky faces stop the trace.
            if (fi->content == CONTENTS_SOLID || fi->content == CONTENTS_SKY) {
                ti->face = face;
                ti->hitsky = (fi->content == CONTENTS_SKY);
                VectorCopy(fi->plane.normal, ti->hitnormal);
                // check if we hit the back side
                ti->hitback = (DotProduct(ti->dir, fi->plane.normal) >= 0);
                
                return true;
            }
        }

        //ericw -- no impact found on this node.
        
        // go down back side
        return TraceFaces (ti, tnode->children[front >= 0], mid, end);
    }
}
