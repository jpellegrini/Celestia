// linerenderer.h
//
// Copyright (C) 2022-present, Celestia Development Team.
//
// Line renderer.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include <celengine/glsupport.h>        // gl::maxLineWidth
#include <celengine/shadermanager.h>
#include <celengine/render.h>
#include "linerenderer.h"
#include <iostream>

namespace celestia::render
{

/**
 * @brief Return number of elements of position attribute.
 *
 * @return int
 */
int
LineRenderer::pos_count() const
{
    return (static_cast<int>(m_format) & VF_COUNT_MASK);
}

/**
 * @brief Return number of elements of color attribute.
 *
 * @return int
 */
int
LineRenderer::color_count() const
{
    return ((static_cast<int>(m_format) >> VF_COLOR_POS) & VF_COUNT_MASK);
}

/**
 * @brief Return type of elements of color attribute.
 *
 * @return VF_UBYTE/VF_FLOAT
 */
int
LineRenderer::color_type() const
{
    return ((static_cast<int>(m_format) >> VF_COLOR_POS) & VF_UBYTE_BIT) != 0 ? VF_UBYTE : VF_FLOAT;
}

//! Draw triangles defined with segments.
void
LineRenderer::draw_triangles(int count, int offset) const
{
    m_trVertexObj->draw(GL_TRIANGLES, count, offset);
}

//! Draw triangle strips.
void
LineRenderer::draw_triangle_strip(int count, int offset) const
{
    m_trVertexObj->draw(GL_TRIANGLE_STRIP, count, offset);
}

//! Draw lines defained with segments.
void
LineRenderer::draw_lines(int count, int offset) const
{
    m_lnVertexObj->draw(static_cast<GLenum>(m_primType), count, offset);
}

//! Enable GPU shader and set it's uniform values. Set line width.
void
LineRenderer::setup_shader()
{
    if (m_prog == nullptr)
    {
        ShaderProperties props;
        props.texUsage = ShaderProperties::VertexColors;
        props.lightModel = ShaderProperties::UnlitModel;
        if (m_useTriangles)
            props.texUsage |= ShaderProperties::LineAsTriangles;
        if ((m_hints & DISABLE_FISHEYE_TRANFORMATION) != 0)
            props.fishEyeOverride = ShaderProperties::FisheyeOverrideModeDisabled;
        m_prog = m_renderer.getShaderManager().getShader(props);
        if (m_prog == nullptr)
            return;
    }

    m_prog->use();

    if (m_useTriangles)
    {
        m_prog->lineWidthX = m_width * width_multiplyer() * m_renderer.getPointWidth();
        m_prog->lineWidthY = m_width * width_multiplyer() * m_renderer.getPointHeight();
    }
    else
    {
        glLineWidth(rasterized_width());
    }
}

//! Allocate GPU memory for vertices and define its layout.
void
LineRenderer::create_vbo_lines()
{
    auto storageType = static_cast<GLenum>(m_storageType);
    m_lnVertexObj    = std::make_unique<VertexObject>(GL_ARRAY_BUFFER, 0, storageType);
    m_lnVertexObj->bind();

    m_lnVertexObj->allocate(m_vertices.capacity() * sizeof(m_vertices[0]), m_vertices.data());
    m_lnVertexObj->setVertexAttribArray(
        CelestiaGLProgram::VertexCoordAttributeIndex,
        pos_count(),
        GL_FLOAT,
        GL_FALSE,
        sizeof(m_vertices[0]),
        offsetof(Vertex, pos));
    if (color_count() != 0)
    {
        m_lnVertexObj->setVertexAttribArray(
            CelestiaGLProgram::ColorAttributeIndex,
            color_count(),
            color_type() == VF_UBYTE ? GL_UNSIGNED_BYTE : GL_FLOAT,
            color_type() == VF_UBYTE ? GL_TRUE : GL_FALSE,
            sizeof(m_vertices[0]),
            offsetof(Vertex, color));
    }
}

//! Update or create GPU memory for vertices.
void
LineRenderer::setup_vbo_lines()
{
    if (m_lnVertexObj != nullptr)
    {
        if (m_storageType == StorageType::Static)
        {
            m_lnVertexObj->bind();
        }
        else
        {
            m_lnVertexObj->bindWritable();
            m_lnVertexObj->allocate(
                m_vertices.capacity() * sizeof(m_vertices[0]),
                nullptr); // orphan old buffer
            m_lnVertexObj->setBufferData(
                m_vertices.data(),
                0,
                m_vertices.size() * sizeof(m_vertices[0]));
        }
    }
    else
    {
        create_vbo_lines();
    }
}

//! Allocate GPU memory for vertices and define its layout.
void
LineRenderer::create_vbo_triangles()
{
    auto storageType = static_cast<GLenum>(m_storageType);
    m_trVertexObj    = std::make_unique<VertexObject>(GL_ARRAY_BUFFER, 0, storageType);
    m_trVertexObj->bind();

    GLsizei               stride;
    std::array<size_t, 4> offset;
    if (m_primType == PrimType::Lines || (m_hints & PREFER_SIMPLE_TRIANGLES) != 0)
    {
        stride = static_cast<GLsizei>(sizeof(LineSegment));
        offset =
        {
            offsetof(LineSegment, point1),
            offsetof(LineSegment, point2),
            offsetof(LineSegment, scale),
            offsetof(LineSegment, point1) + offsetof(Vertex, color)
        };

        m_trVertexObj->allocate(m_segments.size() * stride, m_segments.data());
        m_segments.clear();
    }
    else
    {
        stride = static_cast<GLsizei>(sizeof(LineVertex));
        offset =
        {
            offsetof(LineVertex, point),
            2 * stride + offsetof(LineVertex, point),
            offsetof(LineVertex, scale),
            offsetof(LineVertex, point) + offsetof(Vertex, color)
        };

        m_trVertexObj->allocate(m_verticesTr.capacity() * stride, m_verticesTr.data());
        m_verticesTr.clear();
    }
    m_trVertexObj->setVertexAttribArray(
        CelestiaGLProgram::VertexCoordAttributeIndex,
        pos_count(),
        GL_FLOAT,
        GL_FALSE,
        stride,
        static_cast<GLsizeiptr>(offset[0]));
    m_trVertexObj->setVertexAttribArray(
        CelestiaGLProgram::NextVCoordAttributeIndex,
        pos_count(),
        GL_FLOAT,
        GL_FALSE,
        stride,
        static_cast<GLsizeiptr>(offset[1]));
    m_trVertexObj->setVertexAttribArray(
        CelestiaGLProgram::ScaleFactorAttributeIndex,
        1,
        GL_FLOAT,
        GL_FALSE,
        stride,
        static_cast<GLsizeiptr>(offset[2]));
    if (color_count() != 0)
    {
        m_trVertexObj->setVertexAttribArray(
            CelestiaGLProgram::ColorAttributeIndex,
            color_count(),
            color_type() == VF_UBYTE ? GL_UNSIGNED_BYTE : GL_FLOAT,
            color_type() == VF_UBYTE ? GL_TRUE : GL_FALSE,
            stride,
            static_cast<GLsizeiptr>(offset[3]));
    }
}

//! Update or create GPU memory for vertices.
void
LineRenderer::setup_vbo_triangles()
{
    if (m_trVertexObj != nullptr)
    {
        if (m_storageType == StorageType::Static)
        {
            m_trVertexObj->bind();
        }
        else
        {
            m_trVertexObj->bindWritable();
            const void *data;
            GLsizeiptr capacity, size;
            if (m_primType == PrimType::Lines || (m_hints & PREFER_SIMPLE_TRIANGLES) != 0)
            {
                auto stride = static_cast<GLsizei>(sizeof(LineSegment));
                data = m_segments.data();
                capacity = m_segments.capacity() * stride;
                size = m_segments.size() * stride;
            }
            else
            {
                auto stride = static_cast<GLsizei>(sizeof(LineVertex));
                data = m_verticesTr.data();
                capacity = m_verticesTr.capacity() * stride;
                size = m_verticesTr.size() * stride;
            }
            m_trVertexObj->allocate(capacity, nullptr); // orphan old buffer
            m_trVertexObj->setBufferData(data, 0, size);
        }
    }
    else
    {
        create_vbo_triangles();
    }
}

//! Update or create GPU memory for vertices.
void
LineRenderer::setup_vbo()
{
    if (!m_useTriangles)
        setup_vbo_lines();
    else
        setup_vbo_triangles();
}

//! Add new triagles for a line segment (when primitive is Lines).
void
LineRenderer::add_segment_points(const Vertex &point1, const Vertex &point2)
{
    m_segments.emplace_back(point1, point2, -0.5f);
    m_segments.emplace_back(point1, point2,  0.5f);
    m_segments.emplace_back(point2, point1, -0.5f);
    m_segments.emplace_back(point2, point1, -0.5f);
    m_segments.emplace_back(point2, point1,  0.5f);
    m_segments.emplace_back(point1, point2, -0.5f);
}

//! Convert line segments into triangles.
void
LineRenderer::triangulate_segments()
{
    auto count = static_cast<int>(m_vertices.size());
    m_segments.reserve(count*3);
    for (int i = 0; i < count; i+=2)
        add_segment_points(m_vertices[i], m_vertices[i+1]);
}

//! Convert line strip or loop into triangles.
void
LineRenderer::triangulate_vertices_as_segments()
{
    auto count = static_cast<int>(m_vertices.size());
    auto stop = m_primType == PrimType::LineStrip ? count-1 : count;
    m_segments.reserve(stop*6);
    for (int i = 0; i < stop; i++)
        add_segment_points(m_vertices[i], m_vertices[(i + 1) % count]);
}

//! Add additional triangle strips to simulate LineLoop.
void
LineRenderer::close_loop()
{
    // simulate loop by adding an additional endpoint (copy of the first line vertex)
    if (m_verticesTr.size() > 3)
    {
        m_verticesTr.push_back(m_verticesTr[0]);
        m_verticesTr.push_back(m_verticesTr[1]);
        close_strip();
    }
}

//! Add additional triangle strip to calculate normals used to define actual vertex position.
void
LineRenderer::close_strip()
{
    // triangulated lines require two more vertices to calculate tangent
    if (auto index = m_verticesTr.size(); index > 3)
    {
        // See #1417 for more information.
        //
        // append the second to last point again to calculate the last line
        // segment direction, only position is used
        m_verticesTr.push_back(m_verticesTr[index - 4]);
        m_verticesTr.push_back(m_verticesTr[index - 3]);
        // since the last line direction is calculated from last point to
        // second to last point, set the scales of last point to their inverse
        m_verticesTr[index - 2].scale = -m_verticesTr[index - 2].scale;
        m_verticesTr[index - 1].scale = -m_verticesTr[index - 1].scale;
    }
    m_loopDone = true;
}

//! Convert line strip or loop into triangle strip.
void
LineRenderer::triangulate_vertices()
{
    m_verticesTr.reserve(m_vertices.size()*2 + (m_primType == PrimType::LineLoop ? 4 : 2));
    for (const auto &vertex : m_vertices)
    {
        m_verticesTr.emplace_back(vertex, -0.5f);
        m_verticesTr.emplace_back(vertex,  0.5f);
    }

    if (m_primType == PrimType::LineLoop)
        close_loop();
    else if (m_primType == PrimType::LineStrip)
        close_strip();
}

//! Convert lines into triangles.
void
LineRenderer::triangulate()
{
    if (m_triangulated)
    {
        if(m_primType != PrimType::Lines && !m_loopDone)
        {
            if (m_primType == PrimType::LineLoop)
                close_loop();
            else if (m_primType == PrimType::LineStrip)
                close_strip();
        }
    }
    else
    {
        if (m_primType == PrimType::Lines)
            triangulate_segments();
        else if ((m_hints & PREFER_SIMPLE_TRIANGLES) != 0)
            triangulate_vertices_as_segments();
        else
            triangulate_vertices();

        m_triangulated = true;
    }
}

bool
LineRenderer::should_triangulate() const
{
    return rasterized_width() > celestia::gl::maxLineWidth;
}

float
LineRenderer::width_multiplyer() const
{
    return (m_renderer.getRenderFlags() & Renderer::ShowSmoothLines) != 0 ? 1.5f : 1.0f;
}

float
LineRenderer::rasterized_width() const
{
    return m_width * width_multiplyer() * m_renderer.getScaleFactor();
}

LineRenderer::LineRenderer(const Renderer &renderer, float width, PrimType primType, StorageType storageType, VertexFormat format) :
    m_renderer(renderer),
    m_width(width),
    m_primType(primType),
    m_storageType(storageType),
    m_format(format)
{
}

void
LineRenderer::startUpdate()
{
    if (m_storageType != StorageType::Static)
        m_useTriangles = should_triangulate();
}

void
LineRenderer::clear()
{
    m_verticesTr.clear();
    m_segments.clear();
    m_vertices.clear();
    m_triangulated = false;
    m_loopDone = false;
    m_inUse = false;
    m_prog = nullptr;
}

void
LineRenderer::orphan() const
{
    if (m_lnVertexObj != nullptr)
        m_lnVertexObj->allocate(m_lnVertexObj->getBufferSize(), nullptr);
    if (m_trVertexObj != nullptr)
        m_trVertexObj->allocate(m_trVertexObj->getBufferSize(), nullptr);
}

void
LineRenderer::finish()
{
    if (m_lnVertexObj != nullptr)
        m_lnVertexObj->unbind();
    if (m_trVertexObj != nullptr)
        m_trVertexObj->unbind();
    m_inUse = false;
    m_prog = nullptr;
}

void
LineRenderer::prerender()
{
    m_useTriangles = m_useTriangles || should_triangulate();

    if (m_useTriangles)
        triangulate();

    setup_vbo();
    setup_shader();

    m_inUse = true;
}

void
LineRenderer::render(const Matrices &mvp, int count, int offset)
{
    if (!m_inUse)
        prerender();

    m_prog->setMVPMatrices(*mvp.projection, *mvp.modelview);

    if (m_useTriangles)
    {
        if ((m_hints & PREFER_SIMPLE_TRIANGLES) != 0 && m_primType != PrimType::Lines)
        {
            if (m_primType == PrimType::LineStrip)
                count--;
            draw_triangles(count*6, offset*6);
        }
        else if (m_primType == PrimType::Lines)
        {
            draw_triangles(count*3, offset*3);
        }
        else
        {
            if (m_primType == PrimType::LineLoop)
                count++;
            draw_triangle_strip(count*2, offset*2);
        }
    }
    else
    {
        draw_lines(count, offset);
    }
}

void
LineRenderer::render(const Matrices &mvp, const Color &color, int count, int offset)
{
#ifdef GL_ES
    glVertexAttrib4fv(CelestiaGLProgram::ColorAttributeIndex, color.toVector4().data());
#else
    glVertexAttrib4Nubv(CelestiaGLProgram::ColorAttributeIndex, color.data());
#endif
    render(mvp, count, offset);
}

void LineRenderer::addVertex(const Vertex &vertex)
{
    if (!m_useTriangles)
    {
        m_vertices.push_back(vertex);
    }
    else
    {
        m_verticesTr.emplace_back(vertex, -0.5f);
        m_verticesTr.emplace_back(vertex,  0.5f);
    }
}

void
LineRenderer::addVertex(const Eigen::Vector3f &pos)
{
    if (!m_useTriangles)
    {
        m_vertices.emplace_back(pos);
    }
    else
    {
        Vertex v(pos);
        m_verticesTr.emplace_back(v, -0.5f);
        m_verticesTr.emplace_back(v,  0.5f);
    }
}

void
LineRenderer::addVertex(const Eigen::Vector3f &pos, const Color &color)
{
    if (!m_useTriangles)
    {
        m_vertices.emplace_back(pos, color);
    }
    else
    {
        Vertex v(pos, color);
        m_verticesTr.emplace_back(v, -0.5f);
        m_verticesTr.emplace_back(v,  0.5f);
    }
}

void
LineRenderer::addVertex(float x, float y, float z)
{
    if (!m_useTriangles)
    {
        m_vertices.emplace_back(Eigen::Vector3f(x, y, z));
    }
    else
    {
        Vertex v(Eigen::Vector3f(x, y, z));
        m_verticesTr.emplace_back(v, -0.5f);
        m_verticesTr.emplace_back(v,  0.5f);
    }
}

void
LineRenderer::addSegment(const Eigen::Vector3f &pos1, const Eigen::Vector3f &pos2)
{
    if (!m_useTriangles)
    {
        m_vertices.emplace_back(pos1);
        m_vertices.emplace_back(pos2);
    }
    else
    {
        add_segment_points(Vertex(pos1), Vertex(pos2));
    }
}

void
LineRenderer::dropLast()
{
    if ((m_hints & PREFER_SIMPLE_TRIANGLES) == 0 && m_primType != PrimType::Lines)
    {
        m_vertices.pop_back();
        m_verticesTr.pop_back();
        m_verticesTr.pop_back();
    }
}

void
LineRenderer::setVertexCount(int count)
{
    m_vertices.reserve(count);
}

void
LineRenderer::setHints(int hints)
{
    m_hints = hints;
}

void
LineRenderer::setCustomShader(CelestiaGLProgram *prog)
{
    m_prog = prog;
}
} // namespace
