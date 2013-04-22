/*
    Copyright (C) 1996-1997  Id Software, Inc.
    Copyright (C) 1997       Greg Lewis

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
// globals.c

#include "qbsp.h"

// A convenient collection of most shared globals in the program

const int rgcMemSize[GLOBAL + 1] = {
    sizeof(char), sizeof(dplane_t), sizeof(byte), sizeof(dvertex_t),
    sizeof(byte), sizeof(dnode_t), sizeof(texinfo_t), sizeof(dface_t),
    sizeof(byte), sizeof(dclipnode_t), sizeof(dleaf_t),
	sizeof(unsigned short),
    sizeof(dedge_t), sizeof(int), sizeof(dmodel_t), sizeof(mapface_t),
    sizeof(mapbrush_t), sizeof(mapentity_t), 1 /* winding_t */,
	sizeof(face_t),
    sizeof(plane_t), sizeof(portal_t), sizeof(surface_t), sizeof(node_t),
    sizeof(brush_t), sizeof(miptex_t), sizeof(wvert_t), sizeof(wedge_t),
    sizeof(hashvert_t), 1, 1
};

mapdata_t map;

// Useful shortcuts
mapentity_t *pWorldEnt;

// Mathlib.c
vec3_t vec3_origin = { 0, 0, 0 };

// util.c
FILE *logfile;

const char *rgszWarnings[cWarnings] = {
    "No wad or _wad key exists in the worldmodel",
    "No valid WAD filenames in worldmodel",
    "Multiple info_player_start entities",
    "line %d: Brush with duplicate plane",
    "line %d: Brush plane with no normal",
    "No info_player_start entity in level",
    "No info_player_deathmatch entities in level",
    "No info_player_coop entities in level",
    "Point (%.3f %.3f %.3f) off plane by %2.4f",
    "Couldn't create brush faces",

    "Reached occupant at (%.0f %.0f %.0f), no filling performed.",
    "Portal siding direction is wrong",
    "New portal was clipped away in CutNodePortals_r",
    "Winding outside node",
    "Winding with area %f",
    "%s isn't a wadfile",
    "Texture %s not found",
    "%s is an invalid option",
    "Unable to open qbsp.log",
    "No entities in empty space -- no filling performed (hull %d)",

    "Strange map face count",
    "Too many edges in TryMerge",
    "Healing degenerate edge at (%.3f %.3f %.3f)",
    "No target for rotation entity \"%s\"",
    "line %d: Face with degenerate QuArK-style texture axes",
};

const char *rgszErrors[cErrors] = {
    "No axis found for winding",
    "Points on winding exceeded estimate",
    "Degenerate edge at (%.3f %.3f %.3f)",
    "Found a non-canonical vector",
    "Normalization error in FindPlane (vector length %.4f)",
    "Internal error: pWorldEnt->lumps[BSPPLANE].index >= pWorldEnt->lumps[BSPPLANE].count",
    "Internal error: numwedges >= cWEdges",
    "Internal error: numwverts >= cWVerts",
    "f->original exists in SplitFaceForTjunc",
    "Internal error: invalid memory %i type in AllocMem",
    "Too many points on winding (%d)",
    "Out of memory",
    "Clipnodes in map exceed " stringify(MAX_BSP_CLIPNODES),
};
