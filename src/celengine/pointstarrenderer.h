// pointstarrenderer.h
//
// Copyright (C) 2001-2019, the Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#pragma once

#include <Eigen/Core>
#include <vector>
#include "objectrenderer.h"
#include "renderlistentry.h"

class ColorTemperatureTable;
class PointStarVertexBuffer;
class Star;
class StarDatabase;

// TODO: move these variables to PointStarRenderer class
// without adding a variable. Requires C++17
constexpr inline float StarDistanceLimit     = 1.0e6f;
// Star disc size in pixels
constexpr inline float BaseStarDiscSize      = 5.0f;
constexpr inline float MaxScaledDiscStarSize = 8.0f;
constexpr inline float GlareOpacity          = 0.65f;

class PointStarRenderer : public ObjectRenderer<Star, float>
{
 public:
#if 0
    static constexpr const float StarDistanceLimit     = 1.0e6f;
    // Star disc size in pixels
    static constexpr const float BaseStarDiscSize      = 5.0f;
    static constexpr const float MaxScaledDiscStarSize = 8.0f;
    static constexpr const float GlareOpacity          = 0.65f;
#endif

    PointStarRenderer();
    void process(const Star &star, float distance, float appMag);

    Eigen::Vector3d obsPos;
    Eigen::Vector3f viewNormal;
    std::vector<RenderListEntry>* renderList    { nullptr };
    PointStarVertexBuffer* starVertexBuffer     { nullptr };
    PointStarVertexBuffer* glareVertexBuffer    { nullptr };
    const StarDatabase* starDB                  { nullptr };
    const ColorTemperatureTable* colorTemp      { nullptr };
    float SolarSystemMaxDistance                { 1.0f };
    float cosFOV                                { 1.0f };
};
