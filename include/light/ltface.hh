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

#ifndef __LIGHT_LTFACE_H__
#define __LIGHT_LTFACE_H__

#include <common/cmdlib.hh>
#include <common/mathlib.hh>
#include <common/bspfile.hh>
#include <common/log.hh>
#include <common/threads.hh>
#include <common/polylib.hh>

#include <light/litfile.hh>
#include <light/trace.hh>
#include <light/entities.hh>

#include <glm/mat4x4.hpp>

#include <vector>
#include <map>
#include <string>
#include <cassert>
#include <limits>
#include <sstream>
#include <atomic>

extern std::atomic<uint32_t> total_light_rays, total_light_ray_hits, total_samplepoints;
extern std::atomic<uint32_t> total_bounce_rays, total_bounce_ray_hits;

class faceextents_t {
private:
    glm::ivec2 m_texmins;
    glm::ivec2 m_texsize;
    float m_lightmapscale;
    glm::mat4x4 m_worldToTexCoord;
    glm::mat4x4 m_texCoordToWorld;
    
public:
    faceextents_t() = default;
    faceextents_t(const bsp2_dface_t *face, const bsp2_t *bsp, float lmscale);
    int width() const;
    int height() const;
    int numsamples() const;
    glm::ivec2 texsize() const;
    int indexOf(const glm::ivec2 &lm) const;
    glm::ivec2 intCoordsFromIndex(int index) const;
    glm::vec2 LMCoordToTexCoord(const glm::vec2 &LMCoord) const;
    glm::vec2 TexCoordToLMCoord(const glm::vec2 &tc) const;
    glm::vec2 worldToTexCoord(glm::vec3 world) const;
    glm::vec3 texCoordToWorld(glm::vec2 tc) const;
    glm::vec2 worldToLMCoord(glm::vec3 world) const;
    glm::vec3 LMCoordToWorld(glm::vec2 lm) const;
};

glm::vec2 WorldToTexCoord_HighPrecision(const bsp2_t *bsp, const bsp2_dface_t *face, const glm::vec3 &world);
glm::mat4x4 WorldToTexSpace(const bsp2_t *bsp, const bsp2_dface_t *f);
glm::mat4x4 TexSpaceToWorld(const bsp2_t *bsp, const bsp2_dface_t *f);
void WorldToTexCoord(const vec3_t world, const texinfo_t *tex, vec_t coord[2]);
void PrintFaceInfo(const bsp2_dface_t *face, const bsp2_t *bsp);
// FIXME: remove light param. add normal param and dir params.
vec_t GetLightValue(const globalconfig_t &cfg, const light_t *entity, vec_t dist);
std::map<int, glm::vec3> GetDirectLighting(const globalconfig_t &cfg, raystream_t *rs, const vec3_t origin, const vec3_t normal);
void SetupDirt(globalconfig_t &cfg);
float DirtAtPoint(const globalconfig_t &cfg, raystream_t *rs, const vec3_t point, const vec3_t normal, const dmodel_t *selfshadow);
void LightFace(const bsp2_t *bsp, bsp2_dface_t *face, facesup_t *facesup, const globalconfig_t &cfg);

#endif /* __LIGHT_LTFACE_H__ */
