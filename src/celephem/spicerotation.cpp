// spicerotation.cpp
//
// Rotation model interface to the SPICE Toolkit
//
// Copyright (C) 2008, Celestia Development Team
// Initial implementation by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "spicerotation.h"
#include "spiceinterface.h"
#include <celcompat/numbers.h>
#include <celengine/astro.h>
#include <celmath/geomutil.h>
#include <celmath/mathlib.h>
#include <celutil/logger.h>
#include "SpiceUsr.h"
#include <iostream>
#include <cstdio>
#include <limits>

using namespace Eigen;
using namespace std;
using namespace celmath;
using celestia::util::GetLogger;

static const double MILLISEC = astro::secsToDays(0.001);
static const Quaterniond Rx90 = XRotation(celestia::numbers::pi / 2.0);
static const Quaterniond Ry180 = YRotation(celestia::numbers::pi);

/*! Create a new rotation model based on a SPICE frame. The
 *  orientation of the rotation model is the orientation of the
 *  named SPICE frame relative to the base frame orientation.
 *  The rotation is valid during a time range between beginning
 *  and end. The period can be specified for periodic rotations
 *  (units are Julian days), or set to zero for aperiodic rotation
 *  models.
 */
SpiceRotation::SpiceRotation(const std::string& frameName,
                             const std::string& baseFrameName,
                             double period,
                             double beginning,
                             double ending) :
    m_frameName(frameName),
    m_baseFrameName(baseFrameName),
    m_period(period),
    m_spiceErr(false),
    m_validIntervalBegin(beginning),
    m_validIntervalEnd(ending),
    m_useDefaultTimeInterval(false)
{
}


/*! Create a new rotation model based on a SPICE frame. The
 *  orientation of the rotation model is the orientation of the
 *  named SPICE frame relative to the base frame orientation.
 *  The rotation is valid during a time range between beginning
 *  and end. The period can be specified for periodic rotations
 *  (units are Julian days), or set to zero for aperiodic rotation
 *  models.
 */
SpiceRotation::SpiceRotation(const std::string& frameName,
                             const std::string& baseFrameName,
                             double period) :
    m_frameName(frameName),
    m_baseFrameName(baseFrameName),
    m_period(period),
    m_spiceErr(false),
    m_validIntervalBegin(-numeric_limits<double>::infinity()),
    m_validIntervalEnd(numeric_limits<double>::infinity()),
    m_useDefaultTimeInterval(true)
{
}


bool
SpiceRotation::isPeriodic() const
{
    return m_period != 0.0;
};


double
SpiceRotation::getPeriod() const
{
    if (isPeriodic())
        return m_period;

    return m_validIntervalEnd - m_validIntervalBegin;
}


bool
SpiceRotation::init(const fs::path& path,
                    const list<string>* requiredKernels)
{
    // Load required kernel files
    if (requiredKernels != nullptr)
    {
        for (const auto& kernel : *requiredKernels)
        {
            fs::path filepath = path / "data" / kernel;
            if (!LoadSpiceKernel(filepath))
            {
                m_spiceErr = true;
                break;
            }
        }
    }

    // Reduce valid interval by a millisecond at each end.
    m_validIntervalBegin += MILLISEC;
    m_validIntervalEnd -= MILLISEC;

    // Test getting the frame rotation matrix to make sure that there's
    // adequate data in the kernel.
    double beginning = astro::daysToSecs(m_validIntervalBegin - astro::J2000);
    double xform[3][3];
    pxform_c(m_frameName.c_str(), m_frameName.c_str(), beginning, xform);
    if (failed_c())
    {
        // Print the error message
        char errMsg[1024];
        getmsg_c("long", sizeof(errMsg), errMsg);
        GetLogger()->error("{}\n", errMsg);
        m_spiceErr = true;

        reset_c();
    }

    return !m_spiceErr;
}


Quaterniond
SpiceRotation::computeSpin(double jd) const
{
    if (jd < m_validIntervalBegin)
        jd = m_validIntervalBegin;
    else if (jd > m_validIntervalEnd)
        jd = m_validIntervalEnd;

    if (m_spiceErr)
    {
        return Quaterniond::Identity();
    }
    else
    {
        // Input time for SPICE is seconds after J2000
        double t = astro::daysToSecs(jd - astro::J2000);
        double xform[3][3];

        pxform_c(m_frameName.c_str(), m_baseFrameName.c_str(), t, xform);

        if (failed_c())
        {
            // Print the error message
            char errMsg[1024];
            getmsg_c("long", sizeof(errMsg), errMsg);
            GetLogger()->error("{}\n", errMsg);

            // Reset the error state
            reset_c();
        }

        // Eigen stores matrices in column-major order...
        double matrixData[9] =
        {
            xform[0][0], xform[0][1], xform[0][2],
            xform[1][0], xform[1][1], xform[1][2],
            xform[2][0], xform[2][1], xform[2][2]
        };

        // ...but Celestia's rotations are reversed, thus the extra
        // call to conjugate()
        Quaterniond q = Quaterniond(Map<Matrix3d>(matrixData)).conjugate();

        // Transform into Celestia's coordinate system
        return Ry180 * Rx90.conjugate() * q.conjugate() * Rx90;
    }
}
