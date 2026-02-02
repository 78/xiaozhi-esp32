/*
    Astronomy Engine for C/C++.
    https://github.com/cosinekitty/astronomy

    MIT License

    Copyright (c) 2019-2020 Don Cross <cosinekitty@gmail.com>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "astronomy.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @cond DOXYGEN_SKIP */
#define PI      3.14159265358979323846

typedef struct
{
    double x;
    double y;
    double z;
}
terse_vector_t;

static const terse_vector_t VecZero;

static terse_vector_t VecAdd(terse_vector_t a, terse_vector_t b)
{
    terse_vector_t c;
    c.x = a.x + b.x;
    c.y = a.y + b.y;
    c.z = a.z + b.z;
    return c;
}

static terse_vector_t VecSub(terse_vector_t a, terse_vector_t b)
{
    terse_vector_t c;
    c.x = a.x - b.x;
    c.y = a.y - b.y;
    c.z = a.z - b.z;
    return c;
}

static void VecIncr(terse_vector_t *target, terse_vector_t source)
{
    target->x += source.x;
    target->y += source.y;
    target->z += source.z;
}

static void VecDecr(terse_vector_t *target, terse_vector_t source)
{
    target->x -= source.x;
    target->y -= source.y;
    target->z -= source.z;
}

static terse_vector_t VecMul(double s, terse_vector_t v)
{
    terse_vector_t p;
    p.x = s * v.x;
    p.y = s * v.y;
    p.z = s * v.z;
    return p;
}

static void VecScale(terse_vector_t *target, double scalar)
{
    target->x *= scalar;
    target->y *= scalar;
    target->z *= scalar;
}

static terse_vector_t VecRamp(terse_vector_t a, terse_vector_t b, double ramp)
{
    terse_vector_t c;
    c.x = (1-ramp)*a.x + ramp*b.x;
    c.y = (1-ramp)*a.y + ramp*b.y;
    c.z = (1-ramp)*a.z + ramp*b.z;
    return c;
}

static terse_vector_t VecMean(terse_vector_t a, terse_vector_t b)
{
    terse_vector_t c;
    c.x = (a.x + b.x) / 2;
    c.y = (a.y + b.y) / 2;
    c.z = (a.z + b.z) / 2;
    return c;
}

static astro_vector_t PublicVec(astro_time_t time, terse_vector_t terse)
{
    astro_vector_t vector;

    vector.status = ASTRO_SUCCESS;
    vector.t = time;
    vector.x = terse.x;
    vector.y = terse.y;
    vector.z = terse.z;

    return vector;
}

typedef struct
{
    double          tt;  /* Terrestrial Time in J2000 days */
    terse_vector_t  r;   /* position [au] */
    terse_vector_t  v;   /* velocity [au/day] */
}
body_state_t;
/** @endcond */

static const double DAYS_PER_TROPICAL_YEAR = 365.24217;
static const double DEG2RAD = 0.017453292519943296;
static const double RAD2DEG = 57.295779513082321;
static const double ASEC360 = 1296000.0;
static const double ASEC2RAD = 4.848136811095359935899141e-6;
static const double PI2 = 2.0 * PI;
static const double ARC = 3600.0 * 180.0 / PI;          /* arcseconds per radian */
static const double C_AUDAY = 173.1446326846693;        /* speed of light in AU/day */
static const double KM_PER_AU = 1.4959787069098932e+8;
static const double SECONDS_PER_DAY = 24.0 * 3600.0;
static const double SOLAR_DAYS_PER_SIDEREAL_DAY = 0.9972695717592592;
static const double MEAN_SYNODIC_MONTH = 29.530588;     /* average number of days for Moon to return to the same phase */
static const double EARTH_ORBITAL_PERIOD = 365.256;
static const double NEPTUNE_ORBITAL_PERIOD = 60189.0;
static const double REFRACTION_NEAR_HORIZON = 34.0 / 60.0;   /* degrees of refractive "lift" seen for objects near horizon */

static const double SUN_RADIUS_KM  = 695700.0;
#define             SUN_RADIUS_AU  (SUN_RADIUS_KM / KM_PER_AU)

#define EARTH_FLATTENING            0.996647180302104
#define EARTH_EQUATORIAL_RADIUS_KM  6378.1366
#define EARTH_EQUATORIAL_RADIUS_AU  (EARTH_EQUATORIAL_RADIUS_KM / KM_PER_AU)
#define EARTH_MEAN_RADIUS_KM        6371.0            /* mean radius of the Earth's geoid, without atmosphere */
#define EARTH_ATMOSPHERE_KM           88.0            /* effective atmosphere thickness for lunar eclipses */
#define EARTH_ECLIPSE_RADIUS_KM     (EARTH_MEAN_RADIUS_KM + EARTH_ATMOSPHERE_KM)
/* Note: if we ever need Earth's polar radius, it is (EARTH_FLATTENING * EARTH_EQUATORIAL_RADIUS_KM) */

#define MOON_EQUATORIAL_RADIUS_KM   1738.1
#define MOON_MEAN_RADIUS_KM         1737.4
#define MOON_POLAR_RADIUS_KM        1736.0
#define MOON_EQUATORIAL_RADIUS_AU   (MOON_EQUATORIAL_RADIUS_KM / KM_PER_AU)

static const double ASEC180 = 180.0 * 60.0 * 60.0;      /* arcseconds per 180 degrees (or pi radians) */
static const double EARTH_MOON_MASS_RATIO = 81.30056;

/*
    Masses of the Sun and outer planets, used for:
    (1) Calculating the Solar System Barycenter
    (2) Integrating the movement of Pluto

    https://web.archive.org/web/20120220062549/http://iau-comm4.jpl.nasa.gov/de405iom/de405iom.pdf

    Page 10 in the above document describes the constants used in the DE405 ephemeris.
    The following are G*M values (gravity constant * mass) in [au^3 / day^2].
    This side-steps issues of not knowing the exact values of G and masses M[i];
    the products GM[i] are known extremely accurately.
*/
static const double SUN_GM     = 0.2959122082855911e-03;
static const double JUPITER_GM = 0.2825345909524226e-06;
static const double SATURN_GM  = 0.8459715185680659e-07;
static const double URANUS_GM  = 0.1292024916781969e-07;
static const double NEPTUNE_GM = 0.1524358900784276e-07;

/** @cond DOXYGEN_SKIP */
#define ARRAYSIZE(x)    (sizeof(x) / sizeof(x[0]))
#define AU_PER_PARSEC   (ASEC180 / PI)             /* exact definition of how many AU = one parsec */
#define Y2000_IN_MJD    (T0 - MJD_BASIS)
/** @endcond */

static astro_ecliptic_t RotateEquatorialToEcliptic(const double pos[3], double obliq_radians);
static int QuadInterp(
    double tm, double dt, double fa, double fm, double fb,
    double *x, double *t, double *df_dt);

static double LongitudeOffset(double diff)
{
    double offset = diff;

    while (offset <= -180.0)
        offset += 360.0;

    while (offset > 180.0)
        offset -= 360.0;

    return offset;
}

static double NormalizeLongitude(double lon)
{
    while (lon < 0.0)
        lon += 360.0;

    while (lon >= 360.0)
        lon -= 360.0;

    return lon;
}

/**
 * @brief Calculates the length of the given vector.
 *
 * Calculates the non-negative length of the given vector.
 * The length is expressed in the same units as the vector's components,
 * usually astronomical units (AU).
 *
 * @param vector The vector whose length is to be calculated.
 * @return The length of the vector.
 */
double Astronomy_VectorLength(astro_vector_t vector)
{
    return sqrt(vector.x*vector.x + vector.y*vector.y + vector.z*vector.z);
}

/**
 * @brief Finds the name of a celestial body.
 * @param body The celestial body whose name is to be found.
 * @return The English-language name of the celestial body, or "" if the body is not valid.
 */
const char *Astronomy_BodyName(astro_body_t body)
{
    switch (body)
    {
    case BODY_MERCURY:  return "Mercury";
    case BODY_VENUS:    return "Venus";
    case BODY_EARTH:    return "Earth";
    case BODY_MARS:     return "Mars";
    case BODY_JUPITER:  return "Jupiter";
    case BODY_SATURN:   return "Saturn";
    case BODY_URANUS:   return "Uranus";
    case BODY_NEPTUNE:  return "Neptune";
    case BODY_PLUTO:    return "Pluto";
    case BODY_SUN:      return "Sun";
    case BODY_MOON:     return "Moon";
    case BODY_EMB:      return "EMB";
    case BODY_SSB:      return "SSB";
    default:            return "";
    }
}

/**
 * @brief Returns the #astro_body_t value corresponding to the given English name.
 * @param name One of the following strings: Sun, Moon, Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune, Pluto, EMB, SSB.
 * @return If `name` is one of the listed strings (case-sensitive), the returned value is the corresponding #astro_body_t value, otherwise it is `BODY_INVALID`.
 */
astro_body_t Astronomy_BodyCode(const char *name)
{
    if (name != NULL)
    {
        if (!strcmp(name, "Mercury"))   return BODY_MERCURY;
        if (!strcmp(name, "Venus"))     return BODY_VENUS;
        if (!strcmp(name, "Earth"))     return BODY_EARTH;
        if (!strcmp(name, "Mars"))      return BODY_MARS;
        if (!strcmp(name, "Jupiter"))   return BODY_JUPITER;
        if (!strcmp(name, "Saturn"))    return BODY_SATURN;
        if (!strcmp(name, "Uranus"))    return BODY_URANUS;
        if (!strcmp(name, "Neptune"))   return BODY_NEPTUNE;
        if (!strcmp(name, "Pluto"))     return BODY_PLUTO;
        if (!strcmp(name, "Sun"))       return BODY_SUN;
        if (!strcmp(name, "Moon"))      return BODY_MOON;
        if (!strcmp(name, "EMB"))       return BODY_EMB;
        if (!strcmp(name, "SSB"))       return BODY_SSB;
    }
    return BODY_INVALID;
}

/**
 * @brief Returns 1 for planets that are farther from the Sun than the Earth is, 0 otherwise.
 */
static int IsSuperiorPlanet(astro_body_t body)
{
    switch (body)
    {
    case BODY_MARS:
    case BODY_JUPITER:
    case BODY_SATURN:
    case BODY_URANUS:
    case BODY_NEPTUNE:
    case BODY_PLUTO:
        return 1;

    default:
        return 0;
    }
}

/**
 * @brief Returns the number of days it takes for a planet to orbit the Sun.
 */
static double PlanetOrbitalPeriod(astro_body_t body)
{
    switch (body)
    {
    case BODY_MERCURY:  return     87.969;
    case BODY_VENUS:    return    224.701;
    case BODY_EARTH:    return    EARTH_ORBITAL_PERIOD;
    case BODY_MARS:     return    686.980;
    case BODY_JUPITER:  return   4332.589;
    case BODY_SATURN:   return  10759.22;
    case BODY_URANUS:   return  30685.4;
    case BODY_NEPTUNE:  return  NEPTUNE_ORBITAL_PERIOD;
    case BODY_PLUTO:    return  90560.0;
    default:            return  0.0;        /* invalid body */
    }
}

static void FatalError(const char *message)
{
    fprintf(stderr, "FATAL: %s\n", message);
    exit(1);
}

static astro_vector_t VecError(astro_status_t status, astro_time_t time)
{
    astro_vector_t vec;
    vec.x = vec.y = vec.z = NAN;
    vec.t = time;
    vec.status = status;
    return vec;
}

static astro_spherical_t SphereError(astro_status_t status)
{
    astro_spherical_t sphere;
    sphere.status = status;
    sphere.dist = sphere.lat = sphere.lon = NAN;
    return sphere;
}

static astro_equatorial_t EquError(astro_status_t status)
{
    astro_equatorial_t equ;
    equ.ra = equ.dec = equ.dist = NAN;
    equ.status = status;
    return equ;
}

static astro_ecliptic_t EclError(astro_status_t status)
{
    astro_ecliptic_t ecl;
    ecl.status = status;
    ecl.ex = ecl.ey = ecl.ez = ecl.elat = ecl.elon = NAN;
    return ecl;
}

static astro_angle_result_t AngleError(astro_status_t status)
{
    astro_angle_result_t result;
    result.status = status;
    result.angle = NAN;
    return result;
}

static astro_func_result_t FuncError(astro_status_t status)
{
    astro_func_result_t result;
    result.status = status;
    result.value = NAN;
    return result;
}

static astro_time_t TimeError(void)
{
    astro_time_t time;
    time.tt = time.ut = time.eps = time.psi = NAN;
    return time;
}

static astro_rotation_t RotationErr(astro_status_t status)
{
    astro_rotation_t rotation;
    int i, j;

    rotation.status = status;
    for (i=0; i<3; ++i)
        for (j=0; j<3; ++j)
            rotation.rot[i][j] = NAN;

    return rotation;
}

static astro_moon_quarter_t MoonQuarterError(astro_status_t status)
{
    astro_moon_quarter_t result;
    result.status = status;
    result.quarter = -1;
    result.time = TimeError();
    return result;
}

static astro_elongation_t ElongError(astro_status_t status)
{
    astro_elongation_t result;

    result.status = status;
    result.elongation = NAN;
    result.ecliptic_separation = NAN;
    result.time = TimeError();
    result.visibility = (astro_visibility_t)(-1);

    return result;
}

static astro_hour_angle_t HourAngleError(astro_status_t status)
{
    astro_hour_angle_t result;

    result.status = status;
    result.time = TimeError();
    result.hor.altitude = result.hor.azimuth = result.hor.dec = result.hor.ra = NAN;

    return result;
}

static astro_illum_t IllumError(astro_status_t status)
{
    astro_illum_t result;

    result.status = status;
    result.time = TimeError();
    result.mag = NAN;
    result.phase_angle = NAN;
    result.helio_dist = NAN;
    result.ring_tilt = NAN;

    return result;
}

static astro_apsis_t ApsisError(astro_status_t status)
{
    astro_apsis_t result;

    result.status = status;
    result.time = TimeError();
    result.kind = APSIS_INVALID;
    result.dist_km = result.dist_au = NAN;

    return result;
}

static astro_search_result_t SearchError(astro_status_t status)
{
    astro_search_result_t result;
    result.time = TimeError();
    result.status = status;
    return result;
}

static astro_constellation_t ConstelErr(astro_status_t status)
{
    astro_constellation_t constel;
    constel.status = status;
    constel.symbol = constel.name = NULL;
    constel.ra_1875 = constel.dec_1875 = NAN;
    return constel;
}

static astro_transit_t TransitErr(astro_status_t status)
{
    astro_transit_t transit;
    transit.status = status;
    transit.start = transit.peak = transit.finish = TimeError();
    transit.separation = NAN;
    return transit;
}

static astro_func_result_t SynodicPeriod(astro_body_t body)
{
    double Tp;                         /* planet's orbital period in days */
    astro_func_result_t result;

    /* The Earth does not have a synodic period as seen from itself. */
    if (body == BODY_EARTH)
        return FuncError(ASTRO_EARTH_NOT_ALLOWED);

    if (body == BODY_MOON)
    {
        result.status = ASTRO_SUCCESS;
        result.value = MEAN_SYNODIC_MONTH;
        return result;
    }

    Tp = PlanetOrbitalPeriod(body);
    if (Tp <= 0.0)
        return FuncError(ASTRO_INVALID_BODY);

    result.status = ASTRO_SUCCESS;
    result.value = fabs(EARTH_ORBITAL_PERIOD / (EARTH_ORBITAL_PERIOD/Tp - 1.0));
    return result;
}

static astro_angle_result_t AngleBetween(astro_vector_t a, astro_vector_t b)
{
    double r, dot;
    astro_angle_result_t result;

    r = Astronomy_VectorLength(a) * Astronomy_VectorLength(b);
    if (r < 1.0e-8)
        return AngleError(ASTRO_BAD_VECTOR);

    dot = (a.x*b.x + a.y*b.y + a.z*b.z) / r;

    if (dot <= -1.0)
        result.angle = 180.0;
    else if (dot >= +1.0)
        result.angle = 0.0;
    else
        result.angle = RAD2DEG * acos(dot);

    result.status = ASTRO_SUCCESS;
    return result;
}

/**
 * @brief The default Delta T function used by Astronomy Engine.
 *
 * Espenak and Meeus use a series of piecewise polynomials to
 * approximate DeltaT of the Earth in their "Five Millennium Canon of Solar Eclipses".
 * See: https://eclipse.gsfc.nasa.gov/SEhelp/deltatpoly2004.html
 * This is the default Delta T function used by Astronomy Engine.
 *
 * @param ut
 *      The floating point number of days since noon UTC on January 1, 2000.
 *
 * @returns
 *      The estimated difference TT-UT on the given date, expressed in seconds.
 */
double Astronomy_DeltaT_EspenakMeeus(double ut)
{
    double y, u, u2, u3, u4, u5, u6, u7;

    /*
        Fred Espenak writes about Delta-T generically here:
        https://eclipse.gsfc.nasa.gov/SEhelp/deltaT.html
        https://eclipse.gsfc.nasa.gov/SEhelp/deltat2004.html

        He provides polynomial approximations for distant years here:
        https://eclipse.gsfc.nasa.gov/SEhelp/deltatpoly2004.html

        They start with a year value 'y' such that y=2000 corresponds
        to the UTC Date 15-January-2000. Convert difference in days
        to mean tropical years.
    */

    y = 2000 + ((ut - 14) / DAYS_PER_TROPICAL_YEAR);

    if (y < -500)
    {
        u = (y - 1820) / 100;
        return -20 + (32 * u*u);
    }
    if (y < 500)
    {
        u = y / 100;
        u2 = u*u; u3 = u*u2; u4 = u2*u2; u5 = u2*u3; u6 = u3*u3;
        return 10583.6 - 1014.41*u + 33.78311*u2 - 5.952053*u3 - 0.1798452*u4 + 0.022174192*u5 + 0.0090316521*u6;
    }
    if (y < 1600)
    {
        u = (y - 1000) / 100;
        u2 = u*u; u3 = u*u2; u4 = u2*u2; u5 = u2*u3; u6 = u3*u3;
        return 1574.2 - 556.01*u + 71.23472*u2 + 0.319781*u3 - 0.8503463*u4 - 0.005050998*u5 + 0.0083572073*u6;
    }
    if (y < 1700)
    {
        u = y - 1600;
        u2 = u*u; u3 = u*u2;
        return 120 - 0.9808*u - 0.01532*u2 + u3/7129.0;
    }
    if (y < 1800)
    {
        u = y - 1700;
        u2 = u*u; u3 = u*u2; u4 = u2*u2;
        return 8.83 + 0.1603*u - 0.0059285*u2 + 0.00013336*u3 - u4/1174000;
    }
    if (y < 1860)
    {
        u = y - 1800;
        u2 = u*u; u3 = u*u2; u4 = u2*u2; u5 = u2*u3; u6 = u3*u3; u7 = u3*u4;
        return 13.72 - 0.332447*u + 0.0068612*u2 + 0.0041116*u3 - 0.00037436*u4 + 0.0000121272*u5 - 0.0000001699*u6 + 0.000000000875*u7;
    }
    if (y < 1900)
    {
        u = y - 1860;
        u2 = u*u; u3 = u*u2; u4 = u2*u2; u5 = u2*u3;
        return 7.62 + 0.5737*u - 0.251754*u2 + 0.01680668*u3 - 0.0004473624*u4 + u5/233174;
    }
    if (y < 1920)
    {
        u = y - 1900;
        u2 = u*u; u3 = u*u2; u4 = u2*u2;
        return -2.79 + 1.494119*u - 0.0598939*u2 + 0.0061966*u3 - 0.000197*u4;
    }
    if (y < 1941)
    {
        u = y - 1920;
        u2 = u*u; u3 = u*u2;
        return 21.20 + 0.84493*u - 0.076100*u2 + 0.0020936*u3;
    }
    if (y < 1961)
    {
        u = y - 1950;
        u2 = u*u; u3 = u*u2;
        return 29.07 + 0.407*u - u2/233 + u3/2547;
    }
    if (y < 1986)
    {
        u = y - 1975;
        u2 = u*u; u3 = u*u2;
        return 45.45 + 1.067*u - u2/260 - u3/718;
    }
    if (y < 2005)
    {
        u = y - 2000;
        u2 = u*u; u3 = u*u2; u4 = u2*u2; u5 = u2*u3;
        return 63.86 + 0.3345*u - 0.060374*u2 + 0.0017275*u3 + 0.000651814*u4 + 0.00002373599*u5;
    }
    if (y < 2050)
    {
        u = y - 2000;
        return 62.92 + 0.32217*u + 0.005589*u*u;
    }
    if (y < 2150)
    {
        u = (y-1820)/100;
        return -20 + 32*u*u - 0.5628*(2150 - y);
    }

    /* all years after 2150 */
    u = (y - 1820) / 100;
    return -20 + (32 * u*u);
}

/**
 * @brief A Delta T function that approximates the one used by the JPL Horizons tool.
 *
 * In order to support unit tests based on data generated by the JPL Horizons online
 * tool, I had to reverse engineer their Delta T function by generating a table that
 * contained it. The main difference between their tool and the Espenak/Meeus function
 * is that they stop extrapolating the Earth's deceleration after the year 2017.
 *
 * @param ut
 *      The floating point number of days since noon UTC on January 1, 2000.
 *
 * @returns
 *      The estimated difference TT-UT on the given date, expressed in seconds.
 */
double Astronomy_DeltaT_JplHorizons(double ut)
{
    if (ut > 17.0 * DAYS_PER_TROPICAL_YEAR)
        ut = 17.0 * DAYS_PER_TROPICAL_YEAR;

    return Astronomy_DeltaT_EspenakMeeus(ut);
}

static astro_deltat_func DeltaTFunc = Astronomy_DeltaT_EspenakMeeus;

/**
 * @brief Changes the function Astronomy Engine uses to calculate Delta T.
 *
 * Most programs should not call this function. It is for advanced use cases only.
 * By default, Astronomy Engine uses the function #Astronomy_DeltaT_EspenakMeeus
 * to estimate changes in the Earth's rotation rate over time.
 * However, for the sake of unit tests that compare calculations against
 * external data sources that use alternative models for Delta T,
 * it is sometimes useful to replace the Delta T model to match.
 * This function allows replacing the Delta T model with any other
 * desired model.
 *
 * @param func
 *      A pointer to a function to convert UT values to DeltaT values.
 */
void Astronomy_SetDeltaTFunction(astro_deltat_func func)
{
    DeltaTFunc = func;
}

static double TerrestrialTime(double ut)
{
    return ut + DeltaTFunc(ut)/86400.0;
}

/**
 * @brief
 *      Converts a J2000 day value to an #astro_time_t value.
 *
 * This function can be useful for reproducing an #astro_time_t structure
 * from its `ut` field only.
 *
 * @param ut
 *      The floating point number of days since noon UTC on January 1, 2000.
 *
 * @returns
 *      An #astro_time_t value for the given `ut` value.
 */
astro_time_t Astronomy_TimeFromDays(double ut)
{
    astro_time_t  time;
    time.ut = ut;
    time.tt = TerrestrialTime(ut);
    time.psi = time.eps = NAN;
    return time;
}

/**
 * @brief Returns the computer's current date and time in the form of an #astro_time_t.
 *
 * Uses the computer's system clock to find the current UTC date and time with 1-second granularity.
 * Converts that date and time to an #astro_time_t value and returns the result.
 * Callers can pass this value to other Astronomy Engine functions to calculate
 * current observational conditions.
 */
astro_time_t Astronomy_CurrentTime(void)
{
    astro_time_t t;

    /* Get seconds since midnight January 1, 1970, divide to convert to days, */
    /* then subtract to get days since noon on January 1, 2000. */

    t.ut = (time(NULL) / SECONDS_PER_DAY) - 10957.5;
    t.tt = TerrestrialTime(t.ut);
    t.psi = t.eps = NAN;
    return t;
}

/**
 * @brief Creates an #astro_time_t value from a given calendar date and time.
 *
 * Given a UTC calendar date and time, calculates an #astro_time_t value that can
 * be passed to other Astronomy Engine functions for performing various calculations
 * relating to that date and time.
 *
 * It is the caller's responsibility to ensure that the parameter values are correct.
 * The parameters are not checked for validity,
 * and this function never returns any indication of an error.
 * Invalid values, for example passing in February 31, may cause unexpected return values.
 *
 * @param year      The UTC calendar year, e.g. 2019.
 * @param month     The UTC calendar month in the range 1..12.
 * @param day       The UTC calendar day in the range 1..31.
 * @param hour      The UTC hour of the day in the range 0..23.
 * @param minute    The UTC minute in the range 0..59.
 * @param second    The UTC floating-point second in the range [0, 60).
 *
 * @return  An #astro_time_t value that represents the given calendar date and time.
 */
astro_time_t Astronomy_MakeTime(int year, int month, int day, int hour, int minute, double second)
{
    astro_time_t time;
    long int jd12h;
    long int y2000;

    /* This formula is adapted from NOVAS C 3.1 function julian_date() */
    jd12h = (long) day - 32075L + 1461L * ((long) year + 4800L
        + ((long) month - 14L) / 12L) / 4L
        + 367L * ((long) month - 2L - ((long) month - 14L) / 12L * 12L)
        / 12L - 3L * (((long) year + 4900L + ((long) month - 14L) / 12L)
        / 100L) / 4L;

    y2000 = jd12h - 2451545L;

    time.ut = (double)y2000 - 0.5 + (hour / 24.0) + (minute / (24.0 * 60.0)) + (second / (24.0 * 3600.0));
    time.tt = TerrestrialTime(time.ut);
    time.psi = time.eps = NAN;

    return time;
}

/**
 * @brief   Calculates the sum or difference of an #astro_time_t with a specified floating point number of days.
 *
 * Sometimes we need to adjust a given #astro_time_t value by a certain amount of time.
 * This function adds the given real number of days in `days` to the date and time in `time`.
 *
 * More precisely, the result's Universal Time field `ut` is exactly adjusted by `days` and
 * the Terrestrial Time field `tt` is adjusted correctly for the resulting UTC date and time,
 * according to the historical and predictive Delta-T model provided by the
 * [United States Naval Observatory](http://maia.usno.navy.mil/ser7/).
 *
 * The value stored in `time` will not be modified; it is passed by value.
 *
 * @param time  A date and time for which to calculate an adjusted date and time.
 * @param days  A floating point number of days by which to adjust `time`. May be negative, 0, or positive.
 * @return  A date and time that is conceptually equal to `time + days`.
 */
astro_time_t Astronomy_AddDays(astro_time_t time, double days)
{
    /*
        This is slightly wrong, but the error is tiny.
        We really should be adding to TT, not to UT.
        But using TT would require creating an inverse function for DeltaT,
        which would be quite a bit of extra calculation.
        I estimate the error is in practice on the order of 10^(-7)
        times the value of 'days'.
        This is based on a typical drift of 1 second per year between UT and TT.
    */

    astro_time_t sum;

    sum.ut = time.ut + days;
    sum.tt = TerrestrialTime(sum.ut);
    sum.eps = sum.psi = NAN;

    return sum;
}

/**
 * @brief   Creates an #astro_time_t value from a given calendar date and time.
 *
 * This function is similar to #Astronomy_MakeTime, only it receives a
 * UTC calendar date and time in the form of an #astro_utc_t structure instead of
 * as separate numeric parameters.  Astronomy_TimeFromUtc is the inverse of
 * #Astronomy_UtcFromTime.
 *
 * @param utc   The UTC calendar date and time to be converted to #astro_time_t.
 * @return  A value that can be used for astronomical calculations for the given date and time.
 */
astro_time_t Astronomy_TimeFromUtc(astro_utc_t utc)
{
    return Astronomy_MakeTime(utc.year, utc.month, utc.day, utc.hour, utc.minute, utc.second);
}

/**
 * @brief Determines the calendar year, month, day, and time from an #astro_time_t value.
 *
 * After calculating the date and time of an astronomical event in the form of
 * an #astro_time_t value, it is often useful to display the result in a human-readable
 * form. This function converts the linear time scales in the `ut` field of #astro_time_t
 * into a calendar date and time: year, month, day, hours, minutes, and seconds, expressed
 * in UTC.
 *
 * @param time  The astronomical time value to be converted to calendar date and time.
 * @return  A date and time broken out into conventional year, month, day, hour, minute, and second.
 */
astro_utc_t Astronomy_UtcFromTime(astro_time_t time)
{
    /* Adapted from the NOVAS C 3.1 function cal_date() */
    astro_utc_t utc;
    long jd, k, m, n;
    double djd, x;

    djd = time.ut + 2451545.5;
    jd = (long)djd;

    x = 24.0 * fmod(djd, 1.0);
    utc.hour = (int)x;
    x = 60.0 * fmod(x, 1.0);
    utc.minute = (int)x;
    utc.second = 60.0 * fmod(x, 1.0);

    k = jd + 68569L;
    n = 4L * k / 146097L;
    k = k - (146097L * n + 3L) / 4L;
    m = 4000L * (k + 1L) / 1461001L;
    k = k - 1461L * m / 4L + 31L;

    utc.month = (int) (80L * k / 2447L);
    utc.day = (int) (k - 2447L * (long)utc.month / 80L);
    k = (long) utc.month / 11L;

    utc.month = (int) ((long)utc.month + 2L - 12L * k);
    utc.year = (int) (100L * (n - 49L) + m + k);

    return utc;
}


/**
 * @brief Formats an #astro_time_t value as an ISO 8601 string.
 *
 * Given an #astro_time_t value `time`, formats it as an ISO 8601
 * string to the resolution specified by the `format` parameter.
 * The result is stored in the `text` buffer whose capacity in bytes
 * is specified by `size`.
 *
 * @param time
 *      The date and time whose civil time `time.ut` is to be formatted as an ISO 8601 string.
 *      If the civil time is outside the year range 0000 to 9999, the function fails
 *      and returns `ASTRO_BAD_TIME`. Years prior to 1583 are treated as if they are
 *      using the modern Gregorian calendar, even when the Julian calendar was actually in effect.
 *
 * @param format
 *      Specifies the resolution to which the date and time should be formatted,
 *      as explained at #astro_time_format_t.
 *      If the value of `format` is not recognized, the function fails and
 *      returns `ASTRO_INVALID_PARAMETER`.
 *
 * @param text
 *      A pointer to a text buffer to receive the output.
 *      If `text` is `NULL`, this function returns `ASTRO_INVALID_PARAMETER`.
 *      If the function fails for any reason, and `text` is not `NULL`,
 *      and `size` is greater than 0, the `text` buffer is set to an empty string.
 *
 * @param size
 *      The size in bytes of the buffer pointed to by `text`. The buffer must
 *      be large enough to accomodate the output format selected by the
 *      `format` parameter, as specified at #astro_time_format_t.
 *      If `size` is too small to hold the string as specified by `format`,
 *      the `text` buffer is set to `""` (if possible)
 *      and the function returns `ASTRO_BUFFER_TOO_SMALL`.
 *      A buffer that is `TIME_TEXT_BYTES` (25) bytes or larger is always large enough for this function.
 *
 * @return `ASTRO_SUCCESS` on success; otherwise an error as described in the parameter notes.
 */
astro_status_t Astronomy_FormatTime(
    astro_time_t time,
    astro_time_format_t format,
    char *text,
    size_t size)
{
    int nprinted;
    double rounding;
    size_t min_size;
    astro_utc_t utc;

    if (text == NULL)
        return ASTRO_INVALID_PARAMETER;

    if (size == 0)
        return ASTRO_BUFFER_TOO_SMALL;

    text[0] = '\0';     /* initialize to empty string, in case an error occurs */

    /* Validate 'size' parameter and perform date/time rounding. */
    switch (format)
    {
    case TIME_FORMAT_DAY:
        min_size = 11;                          /* "2020-12-31" */
        rounding = 0.0;                         /* no rounding */
        break;

    case TIME_FORMAT_MINUTE:
        min_size = 18;                          /* "2020-12-31T15:47Z" */
        rounding = 0.5 / (24.0 * 60.0);         /* round to nearest minute */
        break;

    case TIME_FORMAT_SECOND:
        min_size = 21;                          /* "2020-12-31T15:47:59Z" */
        rounding = 0.5 / (24.0 * 3600.0);       /* round to nearest second */
        break;

    case TIME_FORMAT_MILLI:
        min_size = 25;                          /* "2020-12-31T15:47:59.123Z" */
        rounding = 0.5 / (24.0 * 3600000.0);    /* round to nearest millisecond */
        break;

    default:
        return ASTRO_INVALID_PARAMETER;
    }

    /* Check for insufficient buffer size. */
    if (size < min_size)
        return ASTRO_BUFFER_TOO_SMALL;

    /* Perform rounding. */
    time.ut += rounding;

    /* Convert linear J2000 days to Gregorian UTC date/time. */
    utc = Astronomy_UtcFromTime(time);

    /* We require the year to be formatted as a 4-digit non-negative integer. */
    if (utc.year < 0 || utc.year > 9999)
        return ASTRO_BAD_TIME;

    /* Format the string. */
    switch (format)
    {
    case TIME_FORMAT_DAY:
        nprinted = snprintf(text, size, "%04d-%02d-%02d",
            utc.year, utc.month, utc.day);
        break;

    case TIME_FORMAT_MINUTE:
        nprinted = snprintf(text, size, "%04d-%02d-%02dT%02d:%02dZ",
            utc.year, utc.month, utc.day,
            utc.hour, utc.minute);
        break;

    case TIME_FORMAT_SECOND:
        nprinted = snprintf(text, size, "%04d-%02d-%02dT%02d:%02d:%02.0lfZ",
            utc.year, utc.month, utc.day,
            utc.hour, utc.minute, floor(utc.second));
        break;

    case TIME_FORMAT_MILLI:
        nprinted = snprintf(text, size, "%04d-%02d-%02dT%02d:%02d:%06.3lfZ",
            utc.year, utc.month, utc.day,
            utc.hour, utc.minute, floor(1000.0 * utc.second) / 1000.0);
        break;

    default:
        /* We should have already failed for any unknown 'format' value. */
        return ASTRO_INTERNAL_ERROR;
    }

    if (nprinted < 0)
        return ASTRO_INTERNAL_ERROR;    /* should not be possible for snprintf to return a negative number */

    if (1+(int)nprinted != min_size)
        return ASTRO_INTERNAL_ERROR;    /* there must be a bug calculating min_size or formatting the string */

    return ASTRO_SUCCESS;
}


/**
 * @brief   Creates an observer object that represents a location on or near the surface of the Earth.
 *
 * Some Astronomy Engine functions calculate values pertaining to an observer on the Earth.
 * These functions require a value of type #astro_observer_t that represents the location
 * of such an observer.
 *
 * @param latitude      The geographic latitude of the observer in degrees north (positive) or south (negative) of the equator.
 * @param longitude     The geographic longitude of the observer in degrees east (positive) or west (negative) of the prime meridian at Greenwich, England.
 * @param height        The height of the observer in meters above mean sea level.
 * @return An observer object that can be passed to astronomy functions that require a geographic location.
 */
astro_observer_t Astronomy_MakeObserver(double latitude, double longitude, double height)
{
    astro_observer_t observer;

    observer.latitude = latitude;
    observer.longitude = longitude;
    observer.height = height;

    return observer;
}

static void iau2000b(astro_time_t *time)
{
    /* Adapted from the NOVAS C 3.1 function of the same name. */

    struct row_t
    {
        int nals[5];
        double cls[6];
    };

    static const struct row_t row[77] =
    {

        { {  0,  0,  0,  0,  1 }, {   -172064161,      -174666,        33386,     92052331,         9086,        15377 } },
        { {  0,  0,  2, -2,  2 }, {    -13170906,        -1675,       -13696,      5730336,        -3015,        -4587 } },
        { {  0,  0,  2,  0,  2 }, {     -2276413,         -234,         2796,       978459,         -485,         1374 } },
        { {  0,  0,  0,  0,  2 }, {      2074554,          207,         -698,      -897492,          470,         -291 } },
        { {  0,  1,  0,  0,  0 }, {      1475877,        -3633,        11817,        73871,         -184,        -1924 } },
        { {  0,  1,  2, -2,  2 }, {      -516821,         1226,         -524,       224386,         -677,         -174 } },
        { {  1,  0,  0,  0,  0 }, {       711159,           73,         -872,        -6750,            0,          358 } },
        { {  0,  0,  2,  0,  1 }, {      -387298,         -367,          380,       200728,           18,          318 } },
        { {  1,  0,  2,  0,  2 }, {      -301461,          -36,          816,       129025,          -63,          367 } },
        { {  0, -1,  2, -2,  2 }, {       215829,         -494,          111,       -95929,          299,          132 } },
        { {  0,  0,  2, -2,  1 }, {       128227,          137,          181,       -68982,           -9,           39 } },
        { { -1,  0,  2,  0,  2 }, {       123457,           11,           19,       -53311,           32,           -4 } },
        { { -1,  0,  0,  2,  0 }, {       156994,           10,         -168,        -1235,            0,           82 } },
        { {  1,  0,  0,  0,  1 }, {        63110,           63,           27,       -33228,            0,           -9 } },
        { { -1,  0,  0,  0,  1 }, {       -57976,          -63,         -189,        31429,            0,          -75 } },
        { { -1,  0,  2,  2,  2 }, {       -59641,          -11,          149,        25543,          -11,           66 } },
        { {  1,  0,  2,  0,  1 }, {       -51613,          -42,          129,        26366,            0,           78 } },
        { { -2,  0,  2,  0,  1 }, {        45893,           50,           31,       -24236,          -10,           20 } },
        { {  0,  0,  0,  2,  0 }, {        63384,           11,         -150,        -1220,            0,           29 } },
        { {  0,  0,  2,  2,  2 }, {       -38571,           -1,          158,        16452,          -11,           68 } },
        { {  0, -2,  2, -2,  2 }, {        32481,            0,            0,       -13870,            0,            0 } },
        { { -2,  0,  0,  2,  0 }, {       -47722,            0,          -18,          477,            0,          -25 } },
        { {  2,  0,  2,  0,  2 }, {       -31046,           -1,          131,        13238,          -11,           59 } },
        { {  1,  0,  2, -2,  2 }, {        28593,            0,           -1,       -12338,           10,           -3 } },
        { { -1,  0,  2,  0,  1 }, {        20441,           21,           10,       -10758,            0,           -3 } },
        { {  2,  0,  0,  0,  0 }, {        29243,            0,          -74,         -609,            0,           13 } },
        { {  0,  0,  2,  0,  0 }, {        25887,            0,          -66,         -550,            0,           11 } },
        { {  0,  1,  0,  0,  1 }, {       -14053,          -25,           79,         8551,           -2,          -45 } },
        { { -1,  0,  0,  2,  1 }, {        15164,           10,           11,        -8001,            0,           -1 } },
        { {  0,  2,  2, -2,  2 }, {       -15794,           72,          -16,         6850,          -42,           -5 } },
        { {  0,  0, -2,  2,  0 }, {        21783,            0,           13,         -167,            0,           13 } },
        { {  1,  0,  0, -2,  1 }, {       -12873,          -10,          -37,         6953,            0,          -14 } },
        { {  0, -1,  0,  0,  1 }, {       -12654,           11,           63,         6415,            0,           26 } },
        { { -1,  0,  2,  2,  1 }, {       -10204,            0,           25,         5222,            0,           15 } },
        { {  0,  2,  0,  0,  0 }, {        16707,          -85,          -10,          168,           -1,           10 } },
        { {  1,  0,  2,  2,  2 }, {        -7691,            0,           44,         3268,            0,           19 } },
        { { -2,  0,  2,  0,  0 }, {       -11024,            0,          -14,          104,            0,            2 } },
        { {  0,  1,  2,  0,  2 }, {         7566,          -21,          -11,        -3250,            0,           -5 } },
        { {  0,  0,  2,  2,  1 }, {        -6637,          -11,           25,         3353,            0,           14 } },
        { {  0, -1,  2,  0,  2 }, {        -7141,           21,            8,         3070,            0,            4 } },
        { {  0,  0,  0,  2,  1 }, {        -6302,          -11,            2,         3272,            0,            4 } },
        { {  1,  0,  2, -2,  1 }, {         5800,           10,            2,        -3045,            0,           -1 } },
        { {  2,  0,  2, -2,  2 }, {         6443,            0,           -7,        -2768,            0,           -4 } },
        { { -2,  0,  0,  2,  1 }, {        -5774,          -11,          -15,         3041,            0,           -5 } },
        { {  2,  0,  2,  0,  1 }, {        -5350,            0,           21,         2695,            0,           12 } },
        { {  0, -1,  2, -2,  1 }, {        -4752,          -11,           -3,         2719,            0,           -3 } },
        { {  0,  0,  0, -2,  1 }, {        -4940,          -11,          -21,         2720,            0,           -9 } },
        { { -1, -1,  0,  2,  0 }, {         7350,            0,           -8,          -51,            0,            4 } },
        { {  2,  0,  0, -2,  1 }, {         4065,            0,            6,        -2206,            0,            1 } },
        { {  1,  0,  0,  2,  0 }, {         6579,            0,          -24,         -199,            0,            2 } },
        { {  0,  1,  2, -2,  1 }, {         3579,            0,            5,        -1900,            0,            1 } },
        { {  1, -1,  0,  0,  0 }, {         4725,            0,           -6,          -41,            0,            3 } },
        { { -2,  0,  2,  0,  2 }, {        -3075,            0,           -2,         1313,            0,           -1 } },
        { {  3,  0,  2,  0,  2 }, {        -2904,            0,           15,         1233,            0,            7 } },
        { {  0, -1,  0,  2,  0 }, {         4348,            0,          -10,          -81,            0,            2 } },
        { {  1, -1,  2,  0,  2 }, {        -2878,            0,            8,         1232,            0,            4 } },
        { {  0,  0,  0,  1,  0 }, {        -4230,            0,            5,          -20,            0,           -2 } },
        { { -1, -1,  2,  2,  2 }, {        -2819,            0,            7,         1207,            0,            3 } },
        { { -1,  0,  2,  0,  0 }, {        -4056,            0,            5,           40,            0,           -2 } },
        { {  0, -1,  2,  2,  2 }, {        -2647,            0,           11,         1129,            0,            5 } },
        { { -2,  0,  0,  0,  1 }, {        -2294,            0,          -10,         1266,            0,           -4 } },
        { {  1,  1,  2,  0,  2 }, {         2481,            0,           -7,        -1062,            0,           -3 } },
        { {  2,  0,  0,  0,  1 }, {         2179,            0,           -2,        -1129,            0,           -2 } },
        { { -1,  1,  0,  1,  0 }, {         3276,            0,            1,           -9,            0,            0 } },
        { {  1,  1,  0,  0,  0 }, {        -3389,            0,            5,           35,            0,           -2 } },
        { {  1,  0,  2,  0,  0 }, {         3339,            0,          -13,         -107,            0,            1 } },
        { { -1,  0,  2, -2,  1 }, {        -1987,            0,           -6,         1073,            0,           -2 } },
        { {  1,  0,  0,  0,  2 }, {        -1981,            0,            0,          854,            0,            0 } },
        { { -1,  0,  0,  1,  0 }, {         4026,            0,         -353,         -553,            0,         -139 } },
        { {  0,  0,  2,  1,  2 }, {         1660,            0,           -5,         -710,            0,           -2 } },
        { { -1,  0,  2,  4,  2 }, {        -1521,            0,            9,          647,            0,            4 } },
        { { -1,  1,  0,  1,  1 }, {         1314,            0,            0,         -700,            0,            0 } },
        { {  0, -2,  2, -2,  1 }, {        -1283,            0,            0,          672,            0,            0 } },
        { {  1,  0,  2,  2,  1 }, {        -1331,            0,            8,          663,            0,            4 } },
        { { -2,  0,  2,  2,  2 }, {         1383,            0,           -2,         -594,            0,           -2 } },
        { { -1,  0,  0,  0,  2 }, {         1405,            0,            4,         -610,            0,            2 } },
        { {  1,  1,  2, -2,  2 }, {         1290,            0,            0,         -556,            0,            0 } }

    };

    double t, el, elp, f, d, om, arg, dp, de, sarg, carg;
    int i;

    if (isnan(time->psi))
    {
        t = time->tt / 36525;
        el  = fmod(485868.249036 + t * 1717915923.2178, ASEC360) * ASEC2RAD;
        elp = fmod(1287104.79305 + t * 129596581.0481,  ASEC360) * ASEC2RAD;
        f   = fmod(335779.526232 + t * 1739527262.8478, ASEC360) * ASEC2RAD;
        d   = fmod(1072260.70369 + t * 1602961601.2090, ASEC360) * ASEC2RAD;
        om  = fmod(450160.398036 - t * 6962890.5431,    ASEC360) * ASEC2RAD;
        dp = 0;
        de = 0;
        for (i=76; i >= 0; --i)
        {
            arg = fmod((row[i].nals[0]*el + row[i].nals[1]*elp + row[i].nals[2]*f + row[i].nals[3]*d + row[i].nals[4]*om), PI2);
            sarg = sin(arg);
            carg = cos(arg);
            dp += (row[i].cls[0] + row[i].cls[1]*t) * sarg + row[i].cls[2]*carg;
            de += (row[i].cls[3] + row[i].cls[4]*t) * carg + row[i].cls[5]*sarg;
        }

        time->psi = -0.000135 + (dp * 1.0e-7);
        time->eps = +0.000388 + (de * 1.0e-7);
    }
}

static double mean_obliq(double tt)
{
    double t = tt / 36525.0;
    double asec =
        (((( -  0.0000000434   * t
             -  0.000000576  ) * t
             +  0.00200340   ) * t
             -  0.0001831    ) * t
             - 46.836769     ) * t + 84381.406;

    return asec / 3600.0;
}

/** @cond DOXYGEN_SKIP */
typedef struct
{
    double tt;
    double dpsi;
    double deps;
    double ee;
    double mobl;
    double tobl;
}
earth_tilt_t;
/** @endcond */

static earth_tilt_t e_tilt(astro_time_t *time)
{
    earth_tilt_t et;

    iau2000b(time);
    et.dpsi = time->psi;
    et.deps = time->eps;
    et.mobl = mean_obliq(time->tt);
    et.tobl = et.mobl + (et.deps / 3600.0);
    et.tt = time->tt;
    et.ee = et.dpsi * cos(et.mobl * DEG2RAD) / 15.0;

    return et;
}

static void ecl2equ_vec(astro_time_t time, const double ecl[3], double equ[3])
{
    double obl = mean_obliq(time.tt) * DEG2RAD;
    double cos_obl = cos(obl);
    double sin_obl = sin(obl);

    equ[0] = ecl[0];
    equ[1] = ecl[1]*cos_obl - ecl[2]*sin_obl;
    equ[2] = ecl[1]*sin_obl + ecl[2]*cos_obl;
}


static astro_rotation_t precession_rot(double tt1, double tt2)
{
    astro_rotation_t rotation;
    double xx, yx, zx, xy, yy, zy, xz, yz, zz;
    double t, psia, omegaa, chia, sa, ca, sb, cb, sc, cc, sd, cd;
    double eps0 = 84381.406;

    if ((tt1 != 0.0) && (tt2 != 0.0))
        FatalError("precession_rot: one of (tt1, tt2) must be zero.");

    t = (tt2 - tt1) / 36525;
    if (tt2 == 0)
        t = -t;

    psia   = (((((-    0.0000000951  * t
                 +    0.000132851 ) * t
                 -    0.00114045  ) * t
                 -    1.0790069   ) * t
                 + 5038.481507    ) * t);

    omegaa = (((((+    0.0000003337  * t
                 -    0.000000467 ) * t
                 -    0.00772503  ) * t
                 +    0.0512623   ) * t
                 -    0.025754    ) * t + eps0);

    chia   = (((((-    0.0000000560  * t
                 +    0.000170663 ) * t
                 -    0.00121197  ) * t
                 -    2.3814292   ) * t
                 +   10.556403    ) * t);

    eps0 = eps0 * ASEC2RAD;
    psia = psia * ASEC2RAD;
    omegaa = omegaa * ASEC2RAD;
    chia = chia * ASEC2RAD;

    sa = sin(eps0);
    ca = cos(eps0);
    sb = sin(-psia);
    cb = cos(-psia);
    sc = sin(-omegaa);
    cc = cos(-omegaa);
    sd = sin(chia);
    cd = cos(chia);

    xx =  cd * cb - sb * sd * cc;
    yx =  cd * sb * ca + sd * cc * cb * ca - sa * sd * sc;
    zx =  cd * sb * sa + sd * cc * cb * sa + ca * sd * sc;
    xy = -sd * cb - sb * cd * cc;
    yy = -sd * sb * ca + cd * cc * cb * ca - sa * cd * sc;
    zy = -sd * sb * sa + cd * cc * cb * sa + ca * cd * sc;
    xz =  sb * sc;
    yz = -sc * cb * ca - sa * cc;
    zz = -sc * cb * sa + cc * ca;

    if (tt2 == 0.0)
    {
        /* Perform rotation from other epoch to J2000.0. */
        rotation.rot[0][0] = xx;
        rotation.rot[0][1] = yx;
        rotation.rot[0][2] = zx;
        rotation.rot[1][0] = xy;
        rotation.rot[1][1] = yy;
        rotation.rot[1][2] = zy;
        rotation.rot[2][0] = xz;
        rotation.rot[2][1] = yz;
        rotation.rot[2][2] = zz;
    }
    else
    {
        /* Perform rotation from J2000.0 to other epoch. */
        rotation.rot[0][0] = xx;
        rotation.rot[0][1] = xy;
        rotation.rot[0][2] = xz;
        rotation.rot[1][0] = yx;
        rotation.rot[1][1] = yy;
        rotation.rot[1][2] = yz;
        rotation.rot[2][0] = zx;
        rotation.rot[2][1] = zy;
        rotation.rot[2][2] = zz;
    }

    rotation.status = ASTRO_SUCCESS;
    return rotation;
}


static void precession(double tt1, const double pos1[3], double tt2, double pos2[3])
{
    astro_rotation_t r = precession_rot(tt1, tt2);
    pos2[0] = r.rot[0][0]*pos1[0] + r.rot[1][0]*pos1[1] + r.rot[2][0]*pos1[2];
    pos2[1] = r.rot[0][1]*pos1[0] + r.rot[1][1]*pos1[1] + r.rot[2][1]*pos1[2];
    pos2[2] = r.rot[0][2]*pos1[0] + r.rot[1][2]*pos1[1] + r.rot[2][2]*pos1[2];
}


static astro_equatorial_t vector2radec(const double pos[3])
{
    astro_equatorial_t equ;
    double xyproj;

    xyproj = pos[0]*pos[0] + pos[1]*pos[1];
    equ.dist = sqrt(xyproj + pos[2]*pos[2]);
    equ.status = ASTRO_SUCCESS;
    if (xyproj == 0.0)
    {
        if (pos[2] == 0.0)
        {
            /* Indeterminate coordinates; pos vector has zero length. */
            equ = EquError(ASTRO_BAD_VECTOR);
        }
        else if (pos[2] < 0)
        {
            equ.ra = 0.0;
            equ.dec = -90.0;
        }
        else
        {
            equ.ra = 0.0;
            equ.dec = +90.0;
        }
    }
    else
    {
        equ.ra = atan2(pos[1], pos[0]) / (DEG2RAD * 15.0);
        if (equ.ra < 0)
            equ.ra += 24.0;

        equ.dec = RAD2DEG * atan2(pos[2], sqrt(xyproj));
    }

    return equ;
}


static astro_rotation_t nutation_rot(astro_time_t *time, int direction)
{
    astro_rotation_t rotation;
    earth_tilt_t tilt = e_tilt(time);
    double oblm = tilt.mobl * DEG2RAD;
    double oblt = tilt.tobl * DEG2RAD;
    double psi = tilt.dpsi * ASEC2RAD;
    double cobm = cos(oblm);
    double sobm = sin(oblm);
    double cobt = cos(oblt);
    double sobt = sin(oblt);
    double cpsi = cos(psi);
    double spsi = sin(psi);

    double xx = cpsi;
    double yx = -spsi * cobm;
    double zx = -spsi * sobm;
    double xy = spsi * cobt;
    double yy = cpsi * cobm * cobt + sobm * sobt;
    double zy = cpsi * sobm * cobt - cobm * sobt;
    double xz = spsi * sobt;
    double yz = cpsi * cobm * sobt - sobm * cobt;
    double zz = cpsi * sobm * sobt + cobm * cobt;

    if (direction == 0)
    {
        /* forward rotation */
        rotation.rot[0][0] = xx;
        rotation.rot[0][1] = xy;
        rotation.rot[0][2] = xz;
        rotation.rot[1][0] = yx;
        rotation.rot[1][1] = yy;
        rotation.rot[1][2] = yz;
        rotation.rot[2][0] = zx;
        rotation.rot[2][1] = zy;
        rotation.rot[2][2] = zz;
    }
    else
    {
        /* inverse rotation */
        rotation.rot[0][0] = xx;
        rotation.rot[0][1] = yx;
        rotation.rot[0][2] = zx;
        rotation.rot[1][0] = xy;
        rotation.rot[1][1] = yy;
        rotation.rot[1][2] = zy;
        rotation.rot[2][0] = xz;
        rotation.rot[2][1] = yz;
        rotation.rot[2][2] = zz;
    }

    rotation.status = ASTRO_SUCCESS;
    return rotation;
}

static void nutation(astro_time_t *time, int direction, const double inpos[3], double outpos[3])
{
    astro_rotation_t r = nutation_rot(time, direction);
    outpos[0] = r.rot[0][0]*inpos[0] + r.rot[1][0]*inpos[1] + r.rot[2][0]*inpos[2];
    outpos[1] = r.rot[0][1]*inpos[0] + r.rot[1][1]*inpos[1] + r.rot[2][1]*inpos[2];
    outpos[2] = r.rot[0][2]*inpos[0] + r.rot[1][2]*inpos[1] + r.rot[2][2]*inpos[2];
}

static double era(double ut)        /* Earth Rotation Angle */
{
    double thet1 = 0.7790572732640 + 0.00273781191135448 * ut;
    double thet3 = fmod(ut, 1.0);
    double theta = 360.0 * fmod(thet1 + thet3, 1.0);
    if (theta < 0.0)
        theta += 360.0;

    return theta;
}

static double sidereal_time(astro_time_t *time)
{
    double t = time->tt / 36525.0;
    double eqeq = 15.0 * e_tilt(time).ee;    /* Replace with eqeq=0 to get GMST instead of GAST (if we ever need it) */
    double theta = era(time->ut);
    double st = (eqeq + 0.014506 +
        (((( -    0.0000000368   * t
            -    0.000029956  ) * t
            -    0.00000044   ) * t
            +    1.3915817    ) * t
            + 4612.156534     ) * t);

    double gst = fmod(st/3600.0 + theta, 360.0) / 15.0;
    if (gst < 0.0)
        gst += 24.0;

    return gst;
}

static void terra(astro_observer_t observer, double st, double pos[3])
{
    double df2 = EARTH_FLATTENING * EARTH_FLATTENING;
    double phi = observer.latitude * DEG2RAD;
    double sinphi = sin(phi);
    double cosphi = cos(phi);
    double c = 1.0 / sqrt(cosphi*cosphi + df2*sinphi*sinphi);
    double s = df2 * c;
    double ht_km = observer.height / 1000.0;
    double ach = EARTH_EQUATORIAL_RADIUS_KM*c + ht_km;
    double ash = EARTH_EQUATORIAL_RADIUS_KM*s + ht_km;
    double stlocl = (15.0*st + observer.longitude) * DEG2RAD;
    double sinst = sin(stlocl);
    double cosst = cos(stlocl);

    pos[0] = ach * cosphi * cosst / KM_PER_AU;
    pos[1] = ach * cosphi * sinst / KM_PER_AU;
    pos[2] = ash * sinphi / KM_PER_AU;

#if 0
    /* If we ever need to calculate the observer's velocity vector, here is how NOVAS C 3.1 does it... */
    static const double ANGVEL = 7.2921150e-5;
    vel[0] = -ANGVEL * ach * cosphi * sinst * 86400.0;
    vel[1] = +ANGVEL * ach * cosphi * cosst * 86400.0;
    vel[2] = 0.0;
#endif
}

static void geo_pos(astro_time_t *time, astro_observer_t observer, double outpos[3])
{
    double gast, pos1[3], pos2[3];

    gast = sidereal_time(time);
    terra(observer, gast, pos1);
    nutation(time, -1, pos1, pos2);
    precession(time->tt, pos2, 0.0, outpos);
}

static void spin(double angle, const double pos1[3], double vec2[3])
{
    double angr = angle * DEG2RAD;
    double cosang = cos(angr);
    double sinang = sin(angr);
    vec2[0] = +cosang*pos1[0] + sinang*pos1[1];
    vec2[1] = -sinang*pos1[0] + cosang*pos1[1];
    vec2[2] = pos1[2];
}

/*------------------ CalcMoon ------------------*/

/** @cond DOXYGEN_SKIP */

#define DECLARE_PASCAL_ARRAY_1(elemtype,name,xmin,xmax) \
    elemtype name[(xmax)-(xmin)+1]

#define DECLARE_PASCAL_ARRAY_2(elemtype,name,xmin,xmax,ymin,ymax) \
    elemtype name[(xmax)-(xmin)+1][(ymax)-(ymin)+1]

#define ACCESS_PASCAL_ARRAY_1(name,xmin,x) \
    ((name)[(x)-(xmin)])

#define ACCESS_PASCAL_ARRAY_2(name,xmin,ymin,x,y) \
    ((name)[(x)-(xmin)][(y)-(ymin)])

typedef struct
{
    double t;
    double dgam;
    double dlam, n, gam1c, sinpi;
    double l0, l, ls, f, d, s;
    double dl0, dl, dls, df, dd, ds;
    DECLARE_PASCAL_ARRAY_2(double,co,-6,6,1,4);   /* ARRAY[-6..6,1..4] OF REAL */
    DECLARE_PASCAL_ARRAY_2(double,si,-6,6,1,4);   /* ARRAY[-6..6,1..4] OF REAL */
}
MoonContext;

#define T           (ctx->t)
#define DGAM        (ctx->dgam)
#define DLAM        (ctx->dlam)
#define N           (ctx->n)
#define GAM1C       (ctx->gam1c)
#define SINPI       (ctx->sinpi)
#define L0          (ctx->l0)
#define L           (ctx->l)
#define LS          (ctx->ls)
#define F           (ctx->f)
#define D           (ctx->d)
#define S           (ctx->s)
#define DL0         (ctx->dl0)
#define DL          (ctx->dl)
#define DLS         (ctx->dls)
#define DF          (ctx->df)
#define DD          (ctx->dd)
#define DS          (ctx->ds)
#define CO(x,y)     ACCESS_PASCAL_ARRAY_2(ctx->co,-6,1,x,y)
#define SI(x,y)     ACCESS_PASCAL_ARRAY_2(ctx->si,-6,1,x,y)

static double Frac(double x)
{
    return x - floor(x);
}

static void AddThe(
    double c1, double s1, double c2, double s2,
    double *c, double *s)
{
    *c = c1*c2 - s1*s2;
    *s = s1*c2 + c1*s2;
}

static double Sine(double phi)
{
    /* sine, of phi in revolutions, not radians */
    return sin(PI2 * phi);
}

static void LongPeriodic(MoonContext *ctx)
{
    double S1 = Sine(0.19833+0.05611*T);
    double S2 = Sine(0.27869+0.04508*T);
    double S3 = Sine(0.16827-0.36903*T);
    double S4 = Sine(0.34734-5.37261*T);
    double S5 = Sine(0.10498-5.37899*T);
    double S6 = Sine(0.42681-0.41855*T);
    double S7 = Sine(0.14943-5.37511*T);

    DL0 = 0.84*S1+0.31*S2+14.27*S3+ 7.26*S4+ 0.28*S5+0.24*S6;
    DL  = 2.94*S1+0.31*S2+14.27*S3+ 9.34*S4+ 1.12*S5+0.83*S6;
    DLS =-6.40*S1                                   -1.89*S6;
    DF  = 0.21*S1+0.31*S2+14.27*S3-88.70*S4-15.30*S5+0.24*S6-1.86*S7;
    DD  = DL0-DLS;
    DGAM  = -3332E-9 * Sine(0.59734-5.37261*T)
             -539E-9 * Sine(0.35498-5.37899*T)
              -64E-9 * Sine(0.39943-5.37511*T);
}

static void Init(MoonContext *ctx)
{
    int I, J, MAX;
    double T2, ARG, FAC;

    T2 = T*T;
    DLAM = 0;
    DS = 0;
    GAM1C = 0;
    SINPI = 3422.7000;
    LongPeriodic(ctx);
    L0 = PI2*Frac(0.60643382+1336.85522467*T-0.00000313*T2) + DL0/ARC;
    L  = PI2*Frac(0.37489701+1325.55240982*T+0.00002565*T2) + DL /ARC;
    LS = PI2*Frac(0.99312619+  99.99735956*T-0.00000044*T2) + DLS/ARC;
    F  = PI2*Frac(0.25909118+1342.22782980*T-0.00000892*T2) + DF /ARC;
    D  = PI2*Frac(0.82736186+1236.85308708*T-0.00000397*T2) + DD /ARC;
    for (I=1; I<=4; ++I)
    {
        switch(I)
        {
            case 1:  ARG=L;  MAX=4; FAC=1.000002208;               break;
            case 2:  ARG=LS; MAX=3; FAC=0.997504612-0.002495388*T; break;
            case 3:  ARG=F;  MAX=4; FAC=1.000002708+139.978*DGAM;  break;
            default: ARG=D;  MAX=6; FAC=1.0;                       break;
        }
        CO(0,I) = 1.0;
        CO(1,I) = cos(ARG)*FAC;
        SI(0,I) = 0.0;
        SI(1,I) = sin(ARG)*FAC;
        for (J=2; J<=MAX; ++J)
            AddThe(CO(J-1,I), SI(J-1,I), CO(1,I), SI(1,I), &CO(J,I), &SI(J,I));

        for (J=1; J<=MAX; ++J)
        {
            CO(-J,I) =  CO(J,I);
            SI(-J,I) = -SI(J,I);
        }
    }
}

static void Term(MoonContext *ctx, int p, int q, int r, int s, double *x, double *y)
{
    int k;
    DECLARE_PASCAL_ARRAY_1(int, i, 1, 4);
    #define I(n) ACCESS_PASCAL_ARRAY_1(i,1,n)

    I(1) = p;
    I(2) = q;
    I(3) = r;
    I(4) = s;
    *x = 1.0;
    *y = 0.0;

    for (k=1; k<=4; ++k)
        if (I(k) != 0.0)
            AddThe(*x, *y, CO(I(k), k), SI(I(k), k), x, y);

    #undef I
}

static void AddSol(
    MoonContext *ctx,
    double coeffl,
    double coeffs,
    double coeffg,
    double coeffp,
    int p,
    int q,
    int r,
    int s)
{
    double x, y;
    Term(ctx, p, q, r, s, &x, &y);
    DLAM += coeffl*y;
    DS += coeffs*y;
    GAM1C += coeffg*x;
    SINPI += coeffp*x;
}

#define ADDN(coeffn,p,q,r,s)    ( Term(ctx, (p),(q),(r),(s),&x,&y), (N += (coeffn)*y) )

static void SolarN(MoonContext *ctx)
{
    double x, y;

    N = 0.0;
    ADDN(-526.069, 0, 0,1,-2);
    ADDN(  -3.352, 0, 0,1,-4);
    ADDN( +44.297,+1, 0,1,-2);
    ADDN(  -6.000,+1, 0,1,-4);
    ADDN( +20.599,-1, 0,1, 0);
    ADDN( -30.598,-1, 0,1,-2);
    ADDN( -24.649,-2, 0,1, 0);
    ADDN(  -2.000,-2, 0,1,-2);
    ADDN( -22.571, 0,+1,1,-2);
    ADDN( +10.985, 0,-1,1,-2);
}

static void Planetary(MoonContext *ctx)
{
    DLAM +=
        +0.82*Sine(0.7736  -62.5512*T)+0.31*Sine(0.0466 -125.1025*T)
        +0.35*Sine(0.5785  -25.1042*T)+0.66*Sine(0.4591+1335.8075*T)
        +0.64*Sine(0.3130  -91.5680*T)+1.14*Sine(0.1480+1331.2898*T)
        +0.21*Sine(0.5918+1056.5859*T)+0.44*Sine(0.5784+1322.8595*T)
        +0.24*Sine(0.2275   -5.7374*T)+0.28*Sine(0.2965   +2.6929*T)
        +0.33*Sine(0.3132   +6.3368*T);
}

int _CalcMoonCount;     /* Undocumented global for performance tuning. */

static void CalcMoon(
    double centuries_since_j2000,
    double *geo_eclip_lon,      /* (LAMBDA) equinox of date */
    double *geo_eclip_lat,      /* (BETA)   equinox of date */
    double *distance_au)        /* (R) */
{
    double lat_seconds;
    MoonContext context;
    MoonContext *ctx = &context;    /* goofy, but makes macros work inside this function */

    context.t = centuries_since_j2000;
    Init(ctx);

    AddSol(ctx,    13.9020,    14.0600,    -0.0010,     0.2607, 0, 0, 0, 4);
    AddSol(ctx,     0.4030,    -4.0100,     0.3940,     0.0023, 0, 0, 0, 3);
    AddSol(ctx,  2369.9120,  2373.3600,     0.6010,    28.2333, 0, 0, 0, 2);
    AddSol(ctx,  -125.1540,  -112.7900,    -0.7250,    -0.9781, 0, 0, 0, 1);
    AddSol(ctx,     1.9790,     6.9800,    -0.4450,     0.0433, 1, 0, 0, 4);
    AddSol(ctx,   191.9530,   192.7200,     0.0290,     3.0861, 1, 0, 0, 2);
    AddSol(ctx,    -8.4660,   -13.5100,     0.4550,    -0.1093, 1, 0, 0, 1);
    AddSol(ctx, 22639.5000, 22609.0700,     0.0790,   186.5398, 1, 0, 0, 0);
    AddSol(ctx,    18.6090,     3.5900,    -0.0940,     0.0118, 1, 0, 0,-1);
    AddSol(ctx, -4586.4650, -4578.1300,    -0.0770,    34.3117, 1, 0, 0,-2);
    AddSol(ctx,     3.2150,     5.4400,     0.1920,    -0.0386, 1, 0, 0,-3);
    AddSol(ctx,   -38.4280,   -38.6400,     0.0010,     0.6008, 1, 0, 0,-4);
    AddSol(ctx,    -0.3930,    -1.4300,    -0.0920,     0.0086, 1, 0, 0,-6);
    AddSol(ctx,    -0.2890,    -1.5900,     0.1230,    -0.0053, 0, 1, 0, 4);
    AddSol(ctx,   -24.4200,   -25.1000,     0.0400,    -0.3000, 0, 1, 0, 2);
    AddSol(ctx,    18.0230,    17.9300,     0.0070,     0.1494, 0, 1, 0, 1);
    AddSol(ctx,  -668.1460,  -126.9800,    -1.3020,    -0.3997, 0, 1, 0, 0);
    AddSol(ctx,     0.5600,     0.3200,    -0.0010,    -0.0037, 0, 1, 0,-1);
    AddSol(ctx,  -165.1450,  -165.0600,     0.0540,     1.9178, 0, 1, 0,-2);
    AddSol(ctx,    -1.8770,    -6.4600,    -0.4160,     0.0339, 0, 1, 0,-4);
    AddSol(ctx,     0.2130,     1.0200,    -0.0740,     0.0054, 2, 0, 0, 4);
    AddSol(ctx,    14.3870,    14.7800,    -0.0170,     0.2833, 2, 0, 0, 2);
    AddSol(ctx,    -0.5860,    -1.2000,     0.0540,    -0.0100, 2, 0, 0, 1);
    AddSol(ctx,   769.0160,   767.9600,     0.1070,    10.1657, 2, 0, 0, 0);
    AddSol(ctx,     1.7500,     2.0100,    -0.0180,     0.0155, 2, 0, 0,-1);
    AddSol(ctx,  -211.6560,  -152.5300,     5.6790,    -0.3039, 2, 0, 0,-2);
    AddSol(ctx,     1.2250,     0.9100,    -0.0300,    -0.0088, 2, 0, 0,-3);
    AddSol(ctx,   -30.7730,   -34.0700,    -0.3080,     0.3722, 2, 0, 0,-4);
    AddSol(ctx,    -0.5700,    -1.4000,    -0.0740,     0.0109, 2, 0, 0,-6);
    AddSol(ctx,    -2.9210,   -11.7500,     0.7870,    -0.0484, 1, 1, 0, 2);
    AddSol(ctx,     1.2670,     1.5200,    -0.0220,     0.0164, 1, 1, 0, 1);
    AddSol(ctx,  -109.6730,  -115.1800,     0.4610,    -0.9490, 1, 1, 0, 0);
    AddSol(ctx,  -205.9620,  -182.3600,     2.0560,     1.4437, 1, 1, 0,-2);
    AddSol(ctx,     0.2330,     0.3600,     0.0120,    -0.0025, 1, 1, 0,-3);
    AddSol(ctx,    -4.3910,    -9.6600,    -0.4710,     0.0673, 1, 1, 0,-4);
    AddSol(ctx,     0.2830,     1.5300,    -0.1110,     0.0060, 1,-1, 0, 4);
    AddSol(ctx,    14.5770,    31.7000,    -1.5400,     0.2302, 1,-1, 0, 2);
    AddSol(ctx,   147.6870,   138.7600,     0.6790,     1.1528, 1,-1, 0, 0);
    AddSol(ctx,    -1.0890,     0.5500,     0.0210,     0.0000, 1,-1, 0,-1);
    AddSol(ctx,    28.4750,    23.5900,    -0.4430,    -0.2257, 1,-1, 0,-2);
    AddSol(ctx,    -0.2760,    -0.3800,    -0.0060,    -0.0036, 1,-1, 0,-3);
    AddSol(ctx,     0.6360,     2.2700,     0.1460,    -0.0102, 1,-1, 0,-4);
    AddSol(ctx,    -0.1890,    -1.6800,     0.1310,    -0.0028, 0, 2, 0, 2);
    AddSol(ctx,    -7.4860,    -0.6600,    -0.0370,    -0.0086, 0, 2, 0, 0);
    AddSol(ctx,    -8.0960,   -16.3500,    -0.7400,     0.0918, 0, 2, 0,-2);
    AddSol(ctx,    -5.7410,    -0.0400,     0.0000,    -0.0009, 0, 0, 2, 2);
    AddSol(ctx,     0.2550,     0.0000,     0.0000,     0.0000, 0, 0, 2, 1);
    AddSol(ctx,  -411.6080,    -0.2000,     0.0000,    -0.0124, 0, 0, 2, 0);
    AddSol(ctx,     0.5840,     0.8400,     0.0000,     0.0071, 0, 0, 2,-1);
    AddSol(ctx,   -55.1730,   -52.1400,     0.0000,    -0.1052, 0, 0, 2,-2);
    AddSol(ctx,     0.2540,     0.2500,     0.0000,    -0.0017, 0, 0, 2,-3);
    AddSol(ctx,     0.0250,    -1.6700,     0.0000,     0.0031, 0, 0, 2,-4);
    AddSol(ctx,     1.0600,     2.9600,    -0.1660,     0.0243, 3, 0, 0, 2);
    AddSol(ctx,    36.1240,    50.6400,    -1.3000,     0.6215, 3, 0, 0, 0);
    AddSol(ctx,   -13.1930,   -16.4000,     0.2580,    -0.1187, 3, 0, 0,-2);
    AddSol(ctx,    -1.1870,    -0.7400,     0.0420,     0.0074, 3, 0, 0,-4);
    AddSol(ctx,    -0.2930,    -0.3100,    -0.0020,     0.0046, 3, 0, 0,-6);
    AddSol(ctx,    -0.2900,    -1.4500,     0.1160,    -0.0051, 2, 1, 0, 2);
    AddSol(ctx,    -7.6490,   -10.5600,     0.2590,    -0.1038, 2, 1, 0, 0);
    AddSol(ctx,    -8.6270,    -7.5900,     0.0780,    -0.0192, 2, 1, 0,-2);
    AddSol(ctx,    -2.7400,    -2.5400,     0.0220,     0.0324, 2, 1, 0,-4);
    AddSol(ctx,     1.1810,     3.3200,    -0.2120,     0.0213, 2,-1, 0, 2);
    AddSol(ctx,     9.7030,    11.6700,    -0.1510,     0.1268, 2,-1, 0, 0);
    AddSol(ctx,    -0.3520,    -0.3700,     0.0010,    -0.0028, 2,-1, 0,-1);
    AddSol(ctx,    -2.4940,    -1.1700,    -0.0030,    -0.0017, 2,-1, 0,-2);
    AddSol(ctx,     0.3600,     0.2000,    -0.0120,    -0.0043, 2,-1, 0,-4);
    AddSol(ctx,    -1.1670,    -1.2500,     0.0080,    -0.0106, 1, 2, 0, 0);
    AddSol(ctx,    -7.4120,    -6.1200,     0.1170,     0.0484, 1, 2, 0,-2);
    AddSol(ctx,    -0.3110,    -0.6500,    -0.0320,     0.0044, 1, 2, 0,-4);
    AddSol(ctx,     0.7570,     1.8200,    -0.1050,     0.0112, 1,-2, 0, 2);
    AddSol(ctx,     2.5800,     2.3200,     0.0270,     0.0196, 1,-2, 0, 0);
    AddSol(ctx,     2.5330,     2.4000,    -0.0140,    -0.0212, 1,-2, 0,-2);
    AddSol(ctx,    -0.3440,    -0.5700,    -0.0250,     0.0036, 0, 3, 0,-2);
    AddSol(ctx,    -0.9920,    -0.0200,     0.0000,     0.0000, 1, 0, 2, 2);
    AddSol(ctx,   -45.0990,    -0.0200,     0.0000,    -0.0010, 1, 0, 2, 0);
    AddSol(ctx,    -0.1790,    -9.5200,     0.0000,    -0.0833, 1, 0, 2,-2);
    AddSol(ctx,    -0.3010,    -0.3300,     0.0000,     0.0014, 1, 0, 2,-4);
    AddSol(ctx,    -6.3820,    -3.3700,     0.0000,    -0.0481, 1, 0,-2, 2);
    AddSol(ctx,    39.5280,    85.1300,     0.0000,    -0.7136, 1, 0,-2, 0);
    AddSol(ctx,     9.3660,     0.7100,     0.0000,    -0.0112, 1, 0,-2,-2);
    AddSol(ctx,     0.2020,     0.0200,     0.0000,     0.0000, 1, 0,-2,-4);
    AddSol(ctx,     0.4150,     0.1000,     0.0000,     0.0013, 0, 1, 2, 0);
    AddSol(ctx,    -2.1520,    -2.2600,     0.0000,    -0.0066, 0, 1, 2,-2);
    AddSol(ctx,    -1.4400,    -1.3000,     0.0000,     0.0014, 0, 1,-2, 2);
    AddSol(ctx,     0.3840,    -0.0400,     0.0000,     0.0000, 0, 1,-2,-2);
    AddSol(ctx,     1.9380,     3.6000,    -0.1450,     0.0401, 4, 0, 0, 0);
    AddSol(ctx,    -0.9520,    -1.5800,     0.0520,    -0.0130, 4, 0, 0,-2);
    AddSol(ctx,    -0.5510,    -0.9400,     0.0320,    -0.0097, 3, 1, 0, 0);
    AddSol(ctx,    -0.4820,    -0.5700,     0.0050,    -0.0045, 3, 1, 0,-2);
    AddSol(ctx,     0.6810,     0.9600,    -0.0260,     0.0115, 3,-1, 0, 0);
    AddSol(ctx,    -0.2970,    -0.2700,     0.0020,    -0.0009, 2, 2, 0,-2);
    AddSol(ctx,     0.2540,     0.2100,    -0.0030,     0.0000, 2,-2, 0,-2);
    AddSol(ctx,    -0.2500,    -0.2200,     0.0040,     0.0014, 1, 3, 0,-2);
    AddSol(ctx,    -3.9960,     0.0000,     0.0000,     0.0004, 2, 0, 2, 0);
    AddSol(ctx,     0.5570,    -0.7500,     0.0000,    -0.0090, 2, 0, 2,-2);
    AddSol(ctx,    -0.4590,    -0.3800,     0.0000,    -0.0053, 2, 0,-2, 2);
    AddSol(ctx,    -1.2980,     0.7400,     0.0000,     0.0004, 2, 0,-2, 0);
    AddSol(ctx,     0.5380,     1.1400,     0.0000,    -0.0141, 2, 0,-2,-2);
    AddSol(ctx,     0.2630,     0.0200,     0.0000,     0.0000, 1, 1, 2, 0);
    AddSol(ctx,     0.4260,     0.0700,     0.0000,    -0.0006, 1, 1,-2,-2);
    AddSol(ctx,    -0.3040,     0.0300,     0.0000,     0.0003, 1,-1, 2, 0);
    AddSol(ctx,    -0.3720,    -0.1900,     0.0000,    -0.0027, 1,-1,-2, 2);
    AddSol(ctx,     0.4180,     0.0000,     0.0000,     0.0000, 0, 0, 4, 0);
    AddSol(ctx,    -0.3300,    -0.0400,     0.0000,     0.0000, 3, 0, 2, 0);

    SolarN(ctx);
    Planetary(ctx);
    S = F + DS/ARC;

    lat_seconds = (1.000002708 + 139.978*DGAM)*(18518.511+1.189+GAM1C)*sin(S)-6.24*sin(3*S) + N;

    *geo_eclip_lon = PI2 * Frac((L0+DLAM/ARC) / PI2);
    *geo_eclip_lat = lat_seconds * (DEG2RAD / 3600.0);
    *distance_au = (ARC * EARTH_EQUATORIAL_RADIUS_AU) / (0.999953253 * SINPI);
    ++_CalcMoonCount;
}

#undef T
#undef DGAM
#undef DLAM
#undef N
#undef GAM1C
#undef SINPI
#undef L0
#undef L
#undef LS
#undef F
#undef D
#undef S
#undef DL0
#undef DL
#undef DLS
#undef DF
#undef DD
#undef DS
#undef CO
#undef SI

/** @endcond */

/**
 * @brief Calculates the geocentric position of the Moon at a given time.
 *
 * Given a time of observation, calculates the Moon's position as a vector.
 * The vector gives the location of the Moon's center relative to the Earth's center
 * with x-, y-, and z-components measured in astronomical units.
 *
 * This algorithm is based on Nautical Almanac Office's *Improved Lunar Ephemeris* of 1954,
 * which in turn derives from E. W. Brown's lunar theories from the early twentieth century.
 * It is adapted from Turbo Pascal code from the book
 * [Astronomy on the Personal Computer](https://www.springer.com/us/book/9783540672210)
 * by Montenbruck and Pfleger.
 *
 * @param time  The date and time for which to calculate the Moon's position.
 * @return The Moon's position as a vector in J2000 Cartesian equatorial coordinates.
 */
astro_vector_t Astronomy_GeoMoon(astro_time_t time)
{
    double geo_eclip_lon, geo_eclip_lat, distance_au;
    double dist_cos_lat;
    astro_vector_t vector;
    double gepos[3];
    double mpos1[3];
    double mpos2[3];

    CalcMoon(time.tt / 36525.0, &geo_eclip_lon, &geo_eclip_lat, &distance_au);

    /* Convert geocentric ecliptic spherical coordinates to Cartesian coordinates. */
    dist_cos_lat = distance_au * cos(geo_eclip_lat);
    gepos[0] = dist_cos_lat * cos(geo_eclip_lon);
    gepos[1] = dist_cos_lat * sin(geo_eclip_lon);
    gepos[2] = distance_au * sin(geo_eclip_lat);

    /* Convert ecliptic coordinates to equatorial coordinates, both in mean equinox of date. */
    ecl2equ_vec(time, gepos, mpos1);

    /* Convert from mean equinox of date to J2000. */
    precession(time.tt, mpos1, 0, mpos2);

    vector.status = ASTRO_SUCCESS;
    vector.x = mpos2[0];
    vector.y = mpos2[1];
    vector.z = mpos2[2];
    vector.t = time;
    return vector;
}

/*------------------ VSOP ------------------*/

/** @cond DOXYGEN_SKIP */
typedef struct
{
    double amplitude;
    double phase;
    double frequency;
}
vsop_term_t;

typedef struct
{
    int nterms;
    const vsop_term_t *term;
}
vsop_series_t;

typedef struct
{
    int nseries;
    const vsop_series_t *series;
}
vsop_formula_t;

typedef struct
{
    const vsop_formula_t formula[3];
}
vsop_model_t;
/** @endcond */

static const vsop_term_t vsop_lon_Mercury_0[] =
{
    { 4.40250710144, 0.00000000000, 0.00000000000 },
    { 0.40989414977, 1.48302034195, 26087.90314157420 },
    { 0.05046294200, 4.47785489551, 52175.80628314840 },
    { 0.00855346844, 1.16520322459, 78263.70942472259 },
    { 0.00165590362, 4.11969163423, 104351.61256629678 },
    { 0.00034561897, 0.77930768443, 130439.51570787099 },
    { 0.00007583476, 3.71348404924, 156527.41884944518 }
};

static const vsop_term_t vsop_lon_Mercury_1[] =
{
    { 26087.90313685529, 0.00000000000, 0.00000000000 },
    { 0.01131199811, 6.21874197797, 26087.90314157420 },
    { 0.00292242298, 3.04449355541, 52175.80628314840 },
    { 0.00075775081, 6.08568821653, 78263.70942472259 },
    { 0.00019676525, 2.80965111777, 104351.61256629678 }
};

static const vsop_series_t vsop_lon_Mercury[] =
{
    { 7, vsop_lon_Mercury_0 },
    { 5, vsop_lon_Mercury_1 }
};

static const vsop_term_t vsop_lat_Mercury_0[] =
{
    { 0.11737528961, 1.98357498767, 26087.90314157420 },
    { 0.02388076996, 5.03738959686, 52175.80628314840 },
    { 0.01222839532, 3.14159265359, 0.00000000000 },
    { 0.00543251810, 1.79644363964, 78263.70942472259 },
    { 0.00129778770, 4.83232503958, 104351.61256629678 },
    { 0.00031866927, 1.58088495658, 130439.51570787099 },
    { 0.00007963301, 4.60972126127, 156527.41884944518 }
};

static const vsop_term_t vsop_lat_Mercury_1[] =
{
    { 0.00274646065, 3.95008450011, 26087.90314157420 },
    { 0.00099737713, 3.14159265359, 0.00000000000 }
};

static const vsop_series_t vsop_lat_Mercury[] =
{
    { 7, vsop_lat_Mercury_0 },
    { 2, vsop_lat_Mercury_1 }
};

static const vsop_term_t vsop_rad_Mercury_0[] =
{
    { 0.39528271651, 0.00000000000, 0.00000000000 },
    { 0.07834131818, 6.19233722598, 26087.90314157420 },
    { 0.00795525558, 2.95989690104, 52175.80628314840 },
    { 0.00121281764, 6.01064153797, 78263.70942472259 },
    { 0.00021921969, 2.77820093972, 104351.61256629678 },
    { 0.00004354065, 5.82894543774, 130439.51570787099 }
};

static const vsop_term_t vsop_rad_Mercury_1[] =
{
    { 0.00217347740, 4.65617158665, 26087.90314157420 },
    { 0.00044141826, 1.42385544001, 52175.80628314840 }
};

static const vsop_series_t vsop_rad_Mercury[] =
{
    { 6, vsop_rad_Mercury_0 },
    { 2, vsop_rad_Mercury_1 }
};

;
static const vsop_term_t vsop_lon_Venus_0[] =
{
    { 3.17614666774, 0.00000000000, 0.00000000000 },
    { 0.01353968419, 5.59313319619, 10213.28554621100 },
    { 0.00089891645, 5.30650047764, 20426.57109242200 },
    { 0.00005477194, 4.41630661466, 7860.41939243920 },
    { 0.00003455741, 2.69964447820, 11790.62908865880 },
    { 0.00002372061, 2.99377542079, 3930.20969621960 },
    { 0.00001317168, 5.18668228402, 26.29831979980 },
    { 0.00001664146, 4.25018630147, 1577.34354244780 },
    { 0.00001438387, 4.15745084182, 9683.59458111640 },
    { 0.00001200521, 6.15357116043, 30639.85663863300 }
};

static const vsop_term_t vsop_lon_Venus_1[] =
{
    { 10213.28554621638, 0.00000000000, 0.00000000000 },
    { 0.00095617813, 2.46406511110, 10213.28554621100 },
    { 0.00007787201, 0.62478482220, 20426.57109242200 }
};

static const vsop_series_t vsop_lon_Venus[] =
{
    { 10, vsop_lon_Venus_0 },
    { 3, vsop_lon_Venus_1 }
};

static const vsop_term_t vsop_lat_Venus_0[] =
{
    { 0.05923638472, 0.26702775812, 10213.28554621100 },
    { 0.00040107978, 1.14737178112, 20426.57109242200 },
    { 0.00032814918, 3.14159265359, 0.00000000000 }
};

static const vsop_term_t vsop_lat_Venus_1[] =
{
    { 0.00287821243, 1.88964962838, 10213.28554621100 }
};

static const vsop_series_t vsop_lat_Venus[] =
{
    { 3, vsop_lat_Venus_0 },
    { 1, vsop_lat_Venus_1 }
};

static const vsop_term_t vsop_rad_Venus_0[] =
{
    { 0.72334820891, 0.00000000000, 0.00000000000 },
    { 0.00489824182, 4.02151831717, 10213.28554621100 },
    { 0.00001658058, 4.90206728031, 20426.57109242200 },
    { 0.00001378043, 1.12846591367, 11790.62908865880 },
    { 0.00001632096, 2.84548795207, 7860.41939243920 },
    { 0.00000498395, 2.58682193892, 9683.59458111640 },
    { 0.00000221985, 2.01346696541, 19367.18916223280 },
    { 0.00000237454, 2.55136053886, 15720.83878487840 }
};

static const vsop_term_t vsop_rad_Venus_1[] =
{
    { 0.00034551041, 0.89198706276, 10213.28554621100 }
};

static const vsop_series_t vsop_rad_Venus[] =
{
    { 8, vsop_rad_Venus_0 },
    { 1, vsop_rad_Venus_1 }
};

;
static const vsop_term_t vsop_lon_Earth_0[] =
{
    { 1.75347045673, 0.00000000000, 0.00000000000 },
    { 0.03341656453, 4.66925680415, 6283.07584999140 },
    { 0.00034894275, 4.62610242189, 12566.15169998280 },
    { 0.00003417572, 2.82886579754, 3.52311834900 },
    { 0.00003497056, 2.74411783405, 5753.38488489680 },
    { 0.00003135899, 3.62767041756, 77713.77146812050 },
    { 0.00002676218, 4.41808345438, 7860.41939243920 },
    { 0.00002342691, 6.13516214446, 3930.20969621960 },
    { 0.00001273165, 2.03709657878, 529.69096509460 },
    { 0.00001324294, 0.74246341673, 11506.76976979360 },
    { 0.00000901854, 2.04505446477, 26.29831979980 },
    { 0.00001199167, 1.10962946234, 1577.34354244780 },
    { 0.00000857223, 3.50849152283, 398.14900340820 },
    { 0.00000779786, 1.17882681962, 5223.69391980220 },
    { 0.00000990250, 5.23268072088, 5884.92684658320 },
    { 0.00000753141, 2.53339052847, 5507.55323866740 },
    { 0.00000505267, 4.58292599973, 18849.22754997420 },
    { 0.00000492392, 4.20505711826, 775.52261132400 },
    { 0.00000356672, 2.91954114478, 0.06731030280 },
    { 0.00000284125, 1.89869240932, 796.29800681640 },
    { 0.00000242879, 0.34481445893, 5486.77784317500 },
    { 0.00000317087, 5.84901948512, 11790.62908865880 },
    { 0.00000271112, 0.31486255375, 10977.07880469900 },
    { 0.00000206217, 4.80646631478, 2544.31441988340 },
    { 0.00000205478, 1.86953770281, 5573.14280143310 },
    { 0.00000202318, 2.45767790232, 6069.77675455340 },
    { 0.00000126225, 1.08295459501, 20.77539549240 },
    { 0.00000155516, 0.83306084617, 213.29909543800 }
};

static const vsop_term_t vsop_lon_Earth_1[] =
{
    { 6283.07584999140, 0.00000000000, 0.00000000000 },
    { 0.00206058863, 2.67823455808, 6283.07584999140 },
    { 0.00004303419, 2.63512233481, 12566.15169998280 }
};

static const vsop_term_t vsop_lon_Earth_2[] =
{
    { 0.00008721859, 1.07253635559, 6283.07584999140 }
};

static const vsop_series_t vsop_lon_Earth[] =
{
    { 28, vsop_lon_Earth_0 },
    { 3, vsop_lon_Earth_1 },
    { 1, vsop_lon_Earth_2 }
};

static const vsop_term_t vsop_lat_Earth_1[] =
{
    { 0.00227777722, 3.41376620530, 6283.07584999140 },
    { 0.00003805678, 3.37063423795, 12566.15169998280 }
};

static const vsop_series_t vsop_lat_Earth[] =
{
    { 0, NULL },
    { 2, vsop_lat_Earth_1 }
};

static const vsop_term_t vsop_rad_Earth_0[] =
{
    { 1.00013988784, 0.00000000000, 0.00000000000 },
    { 0.01670699632, 3.09846350258, 6283.07584999140 },
    { 0.00013956024, 3.05524609456, 12566.15169998280 },
    { 0.00003083720, 5.19846674381, 77713.77146812050 },
    { 0.00001628463, 1.17387558054, 5753.38488489680 },
    { 0.00001575572, 2.84685214877, 7860.41939243920 },
    { 0.00000924799, 5.45292236722, 11506.76976979360 },
    { 0.00000542439, 4.56409151453, 3930.20969621960 },
    { 0.00000472110, 3.66100022149, 5884.92684658320 },
    { 0.00000085831, 1.27079125277, 161000.68573767410 },
    { 0.00000057056, 2.01374292245, 83996.84731811189 },
    { 0.00000055736, 5.24159799170, 71430.69561812909 },
    { 0.00000174844, 3.01193636733, 18849.22754997420 },
    { 0.00000243181, 4.27349530790, 11790.62908865880 }
};

static const vsop_term_t vsop_rad_Earth_1[] =
{
    { 0.00103018607, 1.10748968172, 6283.07584999140 },
    { 0.00001721238, 1.06442300386, 12566.15169998280 }
};

static const vsop_term_t vsop_rad_Earth_2[] =
{
    { 0.00004359385, 5.78455133808, 6283.07584999140 }
};

static const vsop_series_t vsop_rad_Earth[] =
{
    { 14, vsop_rad_Earth_0 },
    { 2, vsop_rad_Earth_1 },
    { 1, vsop_rad_Earth_2 }
};

;
static const vsop_term_t vsop_lon_Mars_0[] =
{
    { 6.20347711581, 0.00000000000, 0.00000000000 },
    { 0.18656368093, 5.05037100270, 3340.61242669980 },
    { 0.01108216816, 5.40099836344, 6681.22485339960 },
    { 0.00091798406, 5.75478744667, 10021.83728009940 },
    { 0.00027744987, 5.97049513147, 3.52311834900 },
    { 0.00010610235, 2.93958560338, 2281.23049651060 },
    { 0.00012315897, 0.84956094002, 2810.92146160520 },
    { 0.00008926784, 4.15697846427, 0.01725365220 },
    { 0.00008715691, 6.11005153139, 13362.44970679920 },
    { 0.00006797556, 0.36462229657, 398.14900340820 },
    { 0.00007774872, 3.33968761376, 5621.84292321040 },
    { 0.00003575078, 1.66186505710, 2544.31441988340 },
    { 0.00004161108, 0.22814971327, 2942.46342329160 },
    { 0.00003075252, 0.85696614132, 191.44826611160 },
    { 0.00002628117, 0.64806124465, 3337.08930835080 },
    { 0.00002937546, 6.07893711402, 0.06731030280 },
    { 0.00002389414, 5.03896442664, 796.29800681640 },
    { 0.00002579844, 0.02996736156, 3344.13554504880 },
    { 0.00001528141, 1.14979301996, 6151.53388830500 },
    { 0.00001798806, 0.65634057445, 529.69096509460 },
    { 0.00001264357, 3.62275122593, 5092.15195811580 },
    { 0.00001286228, 3.06796065034, 2146.16541647520 },
    { 0.00001546404, 2.91579701718, 1751.53953141600 },
    { 0.00001024902, 3.69334099279, 8962.45534991020 },
    { 0.00000891566, 0.18293837498, 16703.06213349900 },
    { 0.00000858759, 2.40093811940, 2914.01423582380 },
    { 0.00000832715, 2.46418619474, 3340.59517304760 },
    { 0.00000832720, 4.49495782139, 3340.62968035200 },
    { 0.00000712902, 3.66335473479, 1059.38193018920 },
    { 0.00000748723, 3.82248614017, 155.42039943420 },
    { 0.00000723861, 0.67497311481, 3738.76143010800 },
    { 0.00000635548, 2.92182225127, 8432.76438481560 },
    { 0.00000655162, 0.48864064125, 3127.31333126180 },
    { 0.00000550474, 3.81001042328, 0.98032106820 },
    { 0.00000552750, 4.47479317037, 1748.01641306700 },
    { 0.00000425966, 0.55364317304, 6283.07584999140 },
    { 0.00000415131, 0.49662285038, 213.29909543800 },
    { 0.00000472167, 3.62547124025, 1194.44701022460 },
    { 0.00000306551, 0.38052848348, 6684.74797174860 },
    { 0.00000312141, 0.99853944405, 6677.70173505060 },
    { 0.00000293198, 4.22131299634, 20.77539549240 },
    { 0.00000302375, 4.48618007156, 3532.06069281140 },
    { 0.00000274027, 0.54222167059, 3340.54511639700 },
    { 0.00000281079, 5.88163521788, 1349.86740965880 },
    { 0.00000231183, 1.28242156993, 3870.30339179440 },
    { 0.00000283602, 5.76885434940, 3149.16416058820 },
    { 0.00000236117, 5.75503217933, 3333.49887969900 },
    { 0.00000274033, 0.13372524985, 3340.67973700260 },
    { 0.00000299395, 2.78323740866, 6254.62666252360 }
};

static const vsop_term_t vsop_lon_Mars_1[] =
{
    { 3340.61242700512, 0.00000000000, 0.00000000000 },
    { 0.01457554523, 3.60433733236, 3340.61242669980 },
    { 0.00168414711, 3.92318567804, 6681.22485339960 },
    { 0.00020622975, 4.26108844583, 10021.83728009940 },
    { 0.00003452392, 4.73210393190, 3.52311834900 },
    { 0.00002586332, 4.60670058555, 13362.44970679920 },
    { 0.00000841535, 4.45864030426, 2281.23049651060 }
};

static const vsop_term_t vsop_lon_Mars_2[] =
{
    { 0.00058152577, 2.04961712429, 3340.61242669980 },
    { 0.00013459579, 2.45738706163, 6681.22485339960 }
};

static const vsop_series_t vsop_lon_Mars[] =
{
    { 49, vsop_lon_Mars_0 },
    { 7, vsop_lon_Mars_1 },
    { 2, vsop_lon_Mars_2 }
};

static const vsop_term_t vsop_lat_Mars_0[] =
{
    { 0.03197134986, 3.76832042431, 3340.61242669980 },
    { 0.00298033234, 4.10616996305, 6681.22485339960 },
    { 0.00289104742, 0.00000000000, 0.00000000000 },
    { 0.00031365539, 4.44651053090, 10021.83728009940 },
    { 0.00003484100, 4.78812549260, 13362.44970679920 }
};

static const vsop_term_t vsop_lat_Mars_1[] =
{
    { 0.00217310991, 6.04472194776, 3340.61242669980 },
    { 0.00020976948, 3.14159265359, 0.00000000000 },
    { 0.00012834709, 1.60810667915, 6681.22485339960 }
};

static const vsop_series_t vsop_lat_Mars[] =
{
    { 5, vsop_lat_Mars_0 },
    { 3, vsop_lat_Mars_1 }
};

static const vsop_term_t vsop_rad_Mars_0[] =
{
    { 1.53033488271, 0.00000000000, 0.00000000000 },
    { 0.14184953160, 3.47971283528, 3340.61242669980 },
    { 0.00660776362, 3.81783443019, 6681.22485339960 },
    { 0.00046179117, 4.15595316782, 10021.83728009940 },
    { 0.00008109733, 5.55958416318, 2810.92146160520 },
    { 0.00007485318, 1.77239078402, 5621.84292321040 },
    { 0.00005523191, 1.36436303770, 2281.23049651060 },
    { 0.00003825160, 4.49407183687, 13362.44970679920 },
    { 0.00002306537, 0.09081579001, 2544.31441988340 },
    { 0.00001999396, 5.36059617709, 3337.08930835080 },
    { 0.00002484394, 4.92545639920, 2942.46342329160 },
    { 0.00001960195, 4.74249437639, 3344.13554504880 },
    { 0.00001167119, 2.11260868341, 5092.15195811580 },
    { 0.00001102816, 5.00908403998, 398.14900340820 },
    { 0.00000899066, 4.40791133207, 529.69096509460 },
    { 0.00000992252, 5.83861961952, 6151.53388830500 },
    { 0.00000807354, 2.10217065501, 1059.38193018920 },
    { 0.00000797915, 3.44839203899, 796.29800681640 },
    { 0.00000740975, 1.49906336885, 2146.16541647520 }
};

static const vsop_term_t vsop_rad_Mars_1[] =
{
    { 0.01107433345, 2.03250524857, 3340.61242669980 },
    { 0.00103175887, 2.37071847807, 6681.22485339960 },
    { 0.00012877200, 0.00000000000, 0.00000000000 },
    { 0.00010815880, 2.70888095665, 10021.83728009940 }
};

static const vsop_term_t vsop_rad_Mars_2[] =
{
    { 0.00044242249, 0.47930604954, 3340.61242669980 },
    { 0.00008138042, 0.86998389204, 6681.22485339960 }
};

static const vsop_series_t vsop_rad_Mars[] =
{
    { 19, vsop_rad_Mars_0 },
    { 4, vsop_rad_Mars_1 },
    { 2, vsop_rad_Mars_2 }
};

;
static const vsop_term_t vsop_lon_Jupiter_0[] =
{
    { 0.59954691494, 0.00000000000, 0.00000000000 },
    { 0.09695898719, 5.06191793158, 529.69096509460 },
    { 0.00573610142, 1.44406205629, 7.11354700080 },
    { 0.00306389205, 5.41734730184, 1059.38193018920 },
    { 0.00097178296, 4.14264726552, 632.78373931320 },
    { 0.00072903078, 3.64042916389, 522.57741809380 },
    { 0.00064263975, 3.41145165351, 103.09277421860 },
    { 0.00039806064, 2.29376740788, 419.48464387520 },
    { 0.00038857767, 1.27231755835, 316.39186965660 },
    { 0.00027964629, 1.78454591820, 536.80451209540 },
    { 0.00013589730, 5.77481040790, 1589.07289528380 },
    { 0.00008246349, 3.58227925840, 206.18554843720 },
    { 0.00008768704, 3.63000308199, 949.17560896980 },
    { 0.00007368042, 5.08101194270, 735.87651353180 },
    { 0.00006263150, 0.02497628807, 213.29909543800 },
    { 0.00006114062, 4.51319998626, 1162.47470440780 },
    { 0.00004905396, 1.32084470588, 110.20632121940 },
    { 0.00005305285, 1.30671216791, 14.22709400160 },
    { 0.00005305441, 4.18625634012, 1052.26838318840 },
    { 0.00004647248, 4.69958103684, 3.93215326310 },
    { 0.00003045023, 4.31676431084, 426.59819087600 },
    { 0.00002609999, 1.56667394063, 846.08283475120 },
    { 0.00002028191, 1.06376530715, 3.18139373770 },
    { 0.00001764763, 2.14148655117, 1066.49547719000 },
    { 0.00001722972, 3.88036268267, 1265.56747862640 },
    { 0.00001920945, 0.97168196472, 639.89728631400 },
    { 0.00001633223, 3.58201833555, 515.46387109300 },
    { 0.00001431999, 4.29685556046, 625.67019231240 },
    { 0.00000973272, 4.09764549134, 95.97922721780 }
};

static const vsop_term_t vsop_lon_Jupiter_1[] =
{
    { 529.69096508814, 0.00000000000, 0.00000000000 },
    { 0.00489503243, 4.22082939470, 529.69096509460 },
    { 0.00228917222, 6.02646855621, 7.11354700080 },
    { 0.00030099479, 4.54540782858, 1059.38193018920 },
    { 0.00020720920, 5.45943156902, 522.57741809380 },
    { 0.00012103653, 0.16994816098, 536.80451209540 },
    { 0.00006067987, 4.42422292017, 103.09277421860 },
    { 0.00005433968, 3.98480737746, 419.48464387520 },
    { 0.00004237744, 5.89008707199, 14.22709400160 }
};

static const vsop_term_t vsop_lon_Jupiter_2[] =
{
    { 0.00047233601, 4.32148536482, 7.11354700080 },
    { 0.00030649436, 2.92977788700, 529.69096509460 },
    { 0.00014837605, 3.14159265359, 0.00000000000 }
};

static const vsop_series_t vsop_lon_Jupiter[] =
{
    { 29, vsop_lon_Jupiter_0 },
    { 9, vsop_lon_Jupiter_1 },
    { 3, vsop_lon_Jupiter_2 }
};

static const vsop_term_t vsop_lat_Jupiter_0[] =
{
    { 0.02268615702, 3.55852606721, 529.69096509460 },
    { 0.00109971634, 3.90809347197, 1059.38193018920 },
    { 0.00110090358, 0.00000000000, 0.00000000000 },
    { 0.00008101428, 3.60509572885, 522.57741809380 },
    { 0.00006043996, 4.25883108339, 1589.07289528380 },
    { 0.00006437782, 0.30627119215, 536.80451209540 }
};

static const vsop_term_t vsop_lat_Jupiter_1[] =
{
    { 0.00078203446, 1.52377859742, 529.69096509460 }
};

static const vsop_series_t vsop_lat_Jupiter[] =
{
    { 6, vsop_lat_Jupiter_0 },
    { 1, vsop_lat_Jupiter_1 }
};

static const vsop_term_t vsop_rad_Jupiter_0[] =
{
    { 5.20887429326, 0.00000000000, 0.00000000000 },
    { 0.25209327119, 3.49108639871, 529.69096509460 },
    { 0.00610599976, 3.84115365948, 1059.38193018920 },
    { 0.00282029458, 2.57419881293, 632.78373931320 },
    { 0.00187647346, 2.07590383214, 522.57741809380 },
    { 0.00086792905, 0.71001145545, 419.48464387520 },
    { 0.00072062974, 0.21465724607, 536.80451209540 },
    { 0.00065517248, 5.97995884790, 316.39186965660 },
    { 0.00029134542, 1.67759379655, 103.09277421860 },
    { 0.00030135335, 2.16132003734, 949.17560896980 },
    { 0.00023453271, 3.54023522184, 735.87651353180 },
    { 0.00022283743, 4.19362594399, 1589.07289528380 },
    { 0.00023947298, 0.27458037480, 7.11354700080 },
    { 0.00013032614, 2.96042965363, 1162.47470440780 },
    { 0.00009703360, 1.90669633585, 206.18554843720 },
    { 0.00012749023, 2.71550286592, 1052.26838318840 },
    { 0.00007057931, 2.18184839926, 1265.56747862640 },
    { 0.00006137703, 6.26418240033, 846.08283475120 },
    { 0.00002616976, 2.00994012876, 1581.95934828300 }
};

static const vsop_term_t vsop_rad_Jupiter_1[] =
{
    { 0.01271801520, 2.64937512894, 529.69096509460 },
    { 0.00061661816, 3.00076460387, 1059.38193018920 },
    { 0.00053443713, 3.89717383175, 522.57741809380 },
    { 0.00031185171, 4.88276958012, 536.80451209540 },
    { 0.00041390269, 0.00000000000, 0.00000000000 }
};

static const vsop_series_t vsop_rad_Jupiter[] =
{
    { 19, vsop_rad_Jupiter_0 },
    { 5, vsop_rad_Jupiter_1 }
};

;
static const vsop_term_t vsop_lon_Saturn_0[] =
{
    { 0.87401354025, 0.00000000000, 0.00000000000 },
    { 0.11107659762, 3.96205090159, 213.29909543800 },
    { 0.01414150957, 4.58581516874, 7.11354700080 },
    { 0.00398379389, 0.52112032699, 206.18554843720 },
    { 0.00350769243, 3.30329907896, 426.59819087600 },
    { 0.00206816305, 0.24658372002, 103.09277421860 },
    { 0.00079271300, 3.84007056878, 220.41264243880 },
    { 0.00023990355, 4.66976924553, 110.20632121940 },
    { 0.00016573588, 0.43719228296, 419.48464387520 },
    { 0.00014906995, 5.76903183869, 316.39186965660 },
    { 0.00015820290, 0.93809155235, 632.78373931320 },
    { 0.00014609559, 1.56518472000, 3.93215326310 },
    { 0.00013160301, 4.44891291899, 14.22709400160 },
    { 0.00015053543, 2.71669915667, 639.89728631400 },
    { 0.00013005299, 5.98119023644, 11.04570026390 },
    { 0.00010725067, 3.12939523827, 202.25339517410 },
    { 0.00005863206, 0.23656938524, 529.69096509460 },
    { 0.00005227757, 4.20783365759, 3.18139373770 },
    { 0.00006126317, 1.76328667907, 277.03499374140 },
    { 0.00005019687, 3.17787728405, 433.71173787680 },
    { 0.00004592550, 0.61977744975, 199.07200143640 },
    { 0.00004005867, 2.24479718502, 63.73589830340 },
    { 0.00002953796, 0.98280366998, 95.97922721780 },
    { 0.00003873670, 3.22283226966, 138.51749687070 },
    { 0.00002461186, 2.03163875071, 735.87651353180 },
    { 0.00003269484, 0.77492638211, 949.17560896980 },
    { 0.00001758145, 3.26580109940, 522.57741809380 },
    { 0.00001640172, 5.50504453050, 846.08283475120 },
    { 0.00001391327, 4.02333150505, 323.50541665740 },
    { 0.00001580648, 4.37265307169, 309.27832265580 },
    { 0.00001123498, 2.83726798446, 415.55249061210 },
    { 0.00001017275, 3.71700135395, 227.52618943960 },
    { 0.00000848642, 3.19150170830, 209.36694217490 }
};

static const vsop_term_t vsop_lon_Saturn_1[] =
{
    { 213.29909521690, 0.00000000000, 0.00000000000 },
    { 0.01297370862, 1.82834923978, 213.29909543800 },
    { 0.00564345393, 2.88499717272, 7.11354700080 },
    { 0.00093734369, 1.06311793502, 426.59819087600 },
    { 0.00107674962, 2.27769131009, 206.18554843720 },
    { 0.00040244455, 2.04108104671, 220.41264243880 },
    { 0.00019941774, 1.27954390470, 103.09277421860 },
    { 0.00010511678, 2.74880342130, 14.22709400160 },
    { 0.00006416106, 0.38238295041, 639.89728631400 },
    { 0.00004848994, 2.43037610229, 419.48464387520 },
    { 0.00004056892, 2.92133209468, 110.20632121940 },
    { 0.00003768635, 3.64965330780, 3.93215326310 }
};

static const vsop_term_t vsop_lon_Saturn_2[] =
{
    { 0.00116441330, 1.17988132879, 7.11354700080 },
    { 0.00091841837, 0.07325195840, 213.29909543800 },
    { 0.00036661728, 0.00000000000, 0.00000000000 },
    { 0.00015274496, 4.06493179167, 206.18554843720 }
};

static const vsop_series_t vsop_lon_Saturn[] =
{
    { 33, vsop_lon_Saturn_0 },
    { 12, vsop_lon_Saturn_1 },
    { 4, vsop_lon_Saturn_2 }
};

static const vsop_term_t vsop_lat_Saturn_0[] =
{
    { 0.04330678039, 3.60284428399, 213.29909543800 },
    { 0.00240348302, 2.85238489373, 426.59819087600 },
    { 0.00084745939, 0.00000000000, 0.00000000000 },
    { 0.00030863357, 3.48441504555, 220.41264243880 },
    { 0.00034116062, 0.57297307557, 206.18554843720 },
    { 0.00014734070, 2.11846596715, 639.89728631400 },
    { 0.00009916667, 5.79003188904, 419.48464387520 },
    { 0.00006993564, 4.73604689720, 7.11354700080 },
    { 0.00004807588, 5.43305312061, 316.39186965660 }
};

static const vsop_term_t vsop_lat_Saturn_1[] =
{
    { 0.00198927992, 4.93901017903, 213.29909543800 },
    { 0.00036947916, 3.14159265359, 0.00000000000 },
    { 0.00017966989, 0.51979431110, 426.59819087600 }
};

static const vsop_series_t vsop_lat_Saturn[] =
{
    { 9, vsop_lat_Saturn_0 },
    { 3, vsop_lat_Saturn_1 }
};

static const vsop_term_t vsop_rad_Saturn_0[] =
{
    { 9.55758135486, 0.00000000000, 0.00000000000 },
    { 0.52921382865, 2.39226219573, 213.29909543800 },
    { 0.01873679867, 5.23549604660, 206.18554843720 },
    { 0.01464663929, 1.64763042902, 426.59819087600 },
    { 0.00821891141, 5.93520042303, 316.39186965660 },
    { 0.00547506923, 5.01532618980, 103.09277421860 },
    { 0.00371684650, 2.27114821115, 220.41264243880 },
    { 0.00361778765, 3.13904301847, 7.11354700080 },
    { 0.00140617506, 5.70406606781, 632.78373931320 },
    { 0.00108974848, 3.29313390175, 110.20632121940 },
    { 0.00069006962, 5.94099540992, 419.48464387520 },
    { 0.00061053367, 0.94037691801, 639.89728631400 },
    { 0.00048913294, 1.55733638681, 202.25339517410 },
    { 0.00034143772, 0.19519102597, 277.03499374140 },
    { 0.00032401773, 5.47084567016, 949.17560896980 },
    { 0.00020936596, 0.46349251129, 735.87651353180 },
    { 0.00009796004, 5.20477537945, 1265.56747862640 },
    { 0.00011993338, 5.98050967385, 846.08283475120 },
    { 0.00020839300, 1.52102476129, 433.71173787680 },
    { 0.00015298404, 3.05943814940, 529.69096509460 },
    { 0.00006465823, 0.17732249942, 1052.26838318840 },
    { 0.00011380257, 1.73105427040, 522.57741809380 },
    { 0.00003419618, 4.94550542171, 1581.95934828300 }
};

static const vsop_term_t vsop_rad_Saturn_1[] =
{
    { 0.06182981340, 0.25843511480, 213.29909543800 },
    { 0.00506577242, 0.71114625261, 206.18554843720 },
    { 0.00341394029, 5.79635741658, 426.59819087600 },
    { 0.00188491195, 0.47215589652, 220.41264243880 },
    { 0.00186261486, 3.14159265359, 0.00000000000 },
    { 0.00143891146, 1.40744822888, 7.11354700080 }
};

static const vsop_term_t vsop_rad_Saturn_2[] =
{
    { 0.00436902572, 4.78671677509, 213.29909543800 }
};

static const vsop_series_t vsop_rad_Saturn[] =
{
    { 23, vsop_rad_Saturn_0 },
    { 6, vsop_rad_Saturn_1 },
    { 1, vsop_rad_Saturn_2 }
};

;
static const vsop_term_t vsop_lon_Uranus_0[] =
{
    { 5.48129294297, 0.00000000000, 0.00000000000 },
    { 0.09260408234, 0.89106421507, 74.78159856730 },
    { 0.01504247898, 3.62719260920, 1.48447270830 },
    { 0.00365981674, 1.89962179044, 73.29712585900 },
    { 0.00272328168, 3.35823706307, 149.56319713460 },
    { 0.00070328461, 5.39254450063, 63.73589830340 },
    { 0.00068892678, 6.09292483287, 76.26607127560 },
    { 0.00061998615, 2.26952066061, 2.96894541660 },
    { 0.00061950719, 2.85098872691, 11.04570026390 },
    { 0.00026468770, 3.14152083966, 71.81265315070 },
    { 0.00025710476, 6.11379840493, 454.90936652730 },
    { 0.00021078850, 4.36059339067, 148.07872442630 },
    { 0.00017818647, 1.74436930289, 36.64856292950 },
    { 0.00014613507, 4.73732166022, 3.93215326310 },
    { 0.00011162509, 5.82681796350, 224.34479570190 },
    { 0.00010997910, 0.48865004018, 138.51749687070 },
    { 0.00009527478, 2.95516862826, 35.16409022120 },
    { 0.00007545601, 5.23626582400, 109.94568878850 },
    { 0.00004220241, 3.23328220918, 70.84944530420 },
    { 0.00004051900, 2.27755017300, 151.04766984290 },
    { 0.00003354596, 1.06549007380, 4.45341812490 },
    { 0.00002926718, 4.62903718891, 9.56122755560 },
    { 0.00003490340, 5.48306144511, 146.59425171800 },
    { 0.00003144069, 4.75199570434, 77.75054398390 },
    { 0.00002922333, 5.35235361027, 85.82729883120 },
    { 0.00002272788, 4.36600400036, 70.32818044240 },
    { 0.00002051219, 1.51773566586, 0.11187458460 },
    { 0.00002148602, 0.60745949945, 38.13303563780 },
    { 0.00001991643, 4.92437588682, 277.03499374140 },
    { 0.00001376226, 2.04283539351, 65.22037101170 },
    { 0.00001666902, 3.62744066769, 380.12776796000 },
    { 0.00001284107, 3.11347961505, 202.25339517410 },
    { 0.00001150429, 0.93343589092, 3.18139373770 },
    { 0.00001533221, 2.58594681212, 52.69019803950 },
    { 0.00001281604, 0.54271272721, 222.86032299360 },
    { 0.00001372139, 4.19641530878, 111.43016149680 },
    { 0.00001221029, 0.19900650030, 108.46121608020 },
    { 0.00000946181, 1.19253165736, 127.47179660680 },
    { 0.00001150989, 4.17898916639, 33.67961751290 }
};

static const vsop_term_t vsop_lon_Uranus_1[] =
{
    { 74.78159860910, 0.00000000000, 0.00000000000 },
    { 0.00154332863, 5.24158770553, 74.78159856730 },
    { 0.00024456474, 1.71260334156, 1.48447270830 },
    { 0.00009258442, 0.42829732350, 11.04570026390 },
    { 0.00008265977, 1.50218091379, 63.73589830340 },
    { 0.00009150160, 1.41213765216, 149.56319713460 }
};

static const vsop_series_t vsop_lon_Uranus[] =
{
    { 39, vsop_lon_Uranus_0 },
    { 6, vsop_lon_Uranus_1 }
};

static const vsop_term_t vsop_lat_Uranus_0[] =
{
    { 0.01346277648, 2.61877810547, 74.78159856730 },
    { 0.00062341400, 5.08111189648, 149.56319713460 },
    { 0.00061601196, 3.14159265359, 0.00000000000 },
    { 0.00009963722, 1.61603805646, 76.26607127560 },
    { 0.00009926160, 0.57630380333, 73.29712585900 }
};

static const vsop_term_t vsop_lat_Uranus_1[] =
{
    { 0.00034101978, 0.01321929936, 74.78159856730 }
};

static const vsop_series_t vsop_lat_Uranus[] =
{
    { 5, vsop_lat_Uranus_0 },
    { 1, vsop_lat_Uranus_1 }
};

static const vsop_term_t vsop_rad_Uranus_0[] =
{
    { 19.21264847206, 0.00000000000, 0.00000000000 },
    { 0.88784984413, 5.60377527014, 74.78159856730 },
    { 0.03440836062, 0.32836099706, 73.29712585900 },
    { 0.02055653860, 1.78295159330, 149.56319713460 },
    { 0.00649322410, 4.52247285911, 76.26607127560 },
    { 0.00602247865, 3.86003823674, 63.73589830340 },
    { 0.00496404167, 1.40139935333, 454.90936652730 },
    { 0.00338525369, 1.58002770318, 138.51749687070 },
    { 0.00243509114, 1.57086606044, 71.81265315070 },
    { 0.00190522303, 1.99809394714, 1.48447270830 },
    { 0.00161858838, 2.79137786799, 148.07872442630 },
    { 0.00143706183, 1.38368544947, 11.04570026390 },
    { 0.00093192405, 0.17437220467, 36.64856292950 },
    { 0.00071424548, 4.24509236074, 224.34479570190 },
    { 0.00089806014, 3.66105364565, 109.94568878850 },
    { 0.00039009723, 1.66971401684, 70.84944530420 },
    { 0.00046677296, 1.39976401694, 35.16409022120 },
    { 0.00039025624, 3.36234773834, 277.03499374140 },
    { 0.00036755274, 3.88649278513, 146.59425171800 },
    { 0.00030348723, 0.70100838798, 151.04766984290 },
    { 0.00029156413, 3.18056336700, 77.75054398390 },
    { 0.00022637073, 0.72518687029, 529.69096509460 },
    { 0.00011959076, 1.75043392140, 984.60033162190 },
    { 0.00025620756, 5.25656086672, 380.12776796000 }
};

static const vsop_term_t vsop_rad_Uranus_1[] =
{
    { 0.01479896629, 3.67205697578, 74.78159856730 }
};

static const vsop_series_t vsop_rad_Uranus[] =
{
    { 24, vsop_rad_Uranus_0 },
    { 1, vsop_rad_Uranus_1 }
};

;
static const vsop_term_t vsop_lon_Neptune_0[] =
{
    { 5.31188633046, 0.00000000000, 0.00000000000 },
    { 0.01798475530, 2.90101273890, 38.13303563780 },
    { 0.01019727652, 0.48580922867, 1.48447270830 },
    { 0.00124531845, 4.83008090676, 36.64856292950 },
    { 0.00042064466, 5.41054993053, 2.96894541660 },
    { 0.00037714584, 6.09221808686, 35.16409022120 },
    { 0.00033784738, 1.24488874087, 76.26607127560 },
    { 0.00016482741, 0.00007727998, 491.55792945680 },
    { 0.00009198584, 4.93747051954, 39.61750834610 },
    { 0.00008994250, 0.27462171806, 175.16605980020 }
};

static const vsop_term_t vsop_lon_Neptune_1[] =
{
    { 38.13303563957, 0.00000000000, 0.00000000000 },
    { 0.00016604172, 4.86323329249, 1.48447270830 },
    { 0.00015744045, 2.27887427527, 38.13303563780 }
};

static const vsop_series_t vsop_lon_Neptune[] =
{
    { 10, vsop_lon_Neptune_0 },
    { 3, vsop_lon_Neptune_1 }
};

static const vsop_term_t vsop_lat_Neptune_0[] =
{
    { 0.03088622933, 1.44104372644, 38.13303563780 },
    { 0.00027780087, 5.91271884599, 76.26607127560 },
    { 0.00027623609, 0.00000000000, 0.00000000000 },
    { 0.00015355489, 2.52123799551, 36.64856292950 },
    { 0.00015448133, 3.50877079215, 39.61750834610 }
};

static const vsop_series_t vsop_lat_Neptune[] =
{
    { 5, vsop_lat_Neptune_0 }
};

static const vsop_term_t vsop_rad_Neptune_0[] =
{
    { 30.07013205828, 0.00000000000, 0.00000000000 },
    { 0.27062259632, 1.32999459377, 38.13303563780 },
    { 0.01691764014, 3.25186135653, 36.64856292950 },
    { 0.00807830553, 5.18592878704, 1.48447270830 },
    { 0.00537760510, 4.52113935896, 35.16409022120 },
    { 0.00495725141, 1.57105641650, 491.55792945680 },
    { 0.00274571975, 1.84552258866, 175.16605980020 },
    { 0.00012012320, 1.92059384991, 1021.24889455140 },
    { 0.00121801746, 5.79754470298, 76.26607127560 },
    { 0.00100896068, 0.37702724930, 73.29712585900 },
    { 0.00135134092, 3.37220609835, 39.61750834610 },
    { 0.00007571796, 1.07149207335, 388.46515523820 }
};

static const vsop_series_t vsop_rad_Neptune[] =
{
    { 12, vsop_rad_Neptune_0 }
};

;

/** @cond DOXYGEN_SKIP */
#define VSOPFORMULA(x)    { ARRAYSIZE(x), x }
/** @endcond */

static const vsop_model_t vsop[] =
{
    { { VSOPFORMULA(vsop_lon_Mercury),  VSOPFORMULA(vsop_lat_Mercury),  VSOPFORMULA(vsop_rad_Mercury) } },
    { { VSOPFORMULA(vsop_lon_Venus),    VSOPFORMULA(vsop_lat_Venus),    VSOPFORMULA(vsop_rad_Venus)   } },
    { { VSOPFORMULA(vsop_lon_Earth),    VSOPFORMULA(vsop_lat_Earth),    VSOPFORMULA(vsop_rad_Earth)   } },
    { { VSOPFORMULA(vsop_lon_Mars),     VSOPFORMULA(vsop_lat_Mars),     VSOPFORMULA(vsop_rad_Mars)    } },
    { { VSOPFORMULA(vsop_lon_Jupiter),  VSOPFORMULA(vsop_lat_Jupiter),  VSOPFORMULA(vsop_rad_Jupiter) } },
    { { VSOPFORMULA(vsop_lon_Saturn),   VSOPFORMULA(vsop_lat_Saturn),   VSOPFORMULA(vsop_rad_Saturn)  } },
    { { VSOPFORMULA(vsop_lon_Uranus),   VSOPFORMULA(vsop_lat_Uranus),   VSOPFORMULA(vsop_rad_Uranus)  } },
    { { VSOPFORMULA(vsop_lon_Neptune),  VSOPFORMULA(vsop_lat_Neptune),  VSOPFORMULA(vsop_rad_Neptune) } }
};

/** @cond DOXYGEN_SKIP */
#define CalcEarth(time)     CalcVsop(&vsop[BODY_EARTH], (time))
#define LON_INDEX 0
#define LAT_INDEX 1
#define RAD_INDEX 2
/** @endcond */

static void VsopCoords(const vsop_model_t *model, double t, double sphere[3])
{
    int k, s, i;

    for (k=0; k < 3; ++k)
    {
        double tpower = 1.0;
        const vsop_formula_t *formula = &model->formula[k];
        sphere[k] = 0.0;
        for (s=0; s < formula->nseries; ++s)
        {
            double sum = 0.0;
            const vsop_series_t *series = &formula->series[s];
            for (i=0; i < series->nterms; ++i)
            {
                const vsop_term_t *term = &series->term[i];
                sum += term->amplitude * cos(term->phase + (t * term->frequency));
            }
            sphere[k] += tpower * sum;
            tpower *= t;
        }
    }
}


static terse_vector_t VsopRotate(const double ecl[3])
{
    terse_vector_t equ;

    /*
        X        +1.000000000000  +0.000000440360  -0.000000190919   X
        Y     =  -0.000000479966  +0.917482137087  -0.397776982902   Y
        Z FK5     0.000000000000  +0.397776982902  +0.917482137087   Z VSOP87A
    */

    equ.x = ecl[0] + 0.000000440360*ecl[1] - 0.000000190919*ecl[2];
    equ.y = -0.000000479966*ecl[0] + 0.917482137087*ecl[1] - 0.397776982902*ecl[2];
    equ.z = 0.397776982902*ecl[1] + 0.917482137087*ecl[2];

    return equ;
}


static void VsopSphereToRect(double lon, double lat, double radius, double pos[3])
{
    double r_coslat = radius * cos(lat);
    pos[0] = r_coslat * cos(lon);
    pos[1] = r_coslat * sin(lon);
    pos[2] = radius * sin(lat);
}

static const double DAYS_PER_MILLENNIUM = 365250.0;


static astro_vector_t CalcVsop(const vsop_model_t *model, astro_time_t time)
{
    double t = time.tt / DAYS_PER_MILLENNIUM;
    double sphere[3];       /* lon, lat, rad */
    double eclip[3];
    astro_vector_t vector;
    terse_vector_t pos;

    /* Calculate the VSOP "B" trigonometric series to obtain ecliptic spherical coordinates. */
    VsopCoords(model, t, sphere);

    /* Convert ecliptic spherical coordinates to ecliptic Cartesian coordinates. */
    VsopSphereToRect(sphere[LON_INDEX], sphere[LAT_INDEX], sphere[RAD_INDEX], eclip);

    /* Convert ecliptic Cartesian coordinates to equatorial Cartesian coordinates. */
    pos = VsopRotate(eclip);

    /* Package the position as astro_vector_t. */
    vector.status = ASTRO_SUCCESS;
    vector.t = time;
    vector.x = pos.x;
    vector.y = pos.y;
    vector.z = pos.z;

    return vector;
}


static void VsopDeriv(const vsop_model_t *model, double t, double deriv[3])
{
    int k, s, i;

    for (k=0; k < 3; ++k)
    {
        double tpower = 1.0;        /* t^s */
        double dpower = 0.0;        /* t^(s-1) */
        const vsop_formula_t *formula = &model->formula[k];
        deriv[k] = 0.0;
        for (s=0; s < formula->nseries; ++s)
        {
            double sin_sum = 0.0;
            double cos_sum = 0.0;
            const vsop_series_t *series = &formula->series[s];
            for (i=0; i < series->nterms; ++i)
            {
                const vsop_term_t *term = &series->term[i];
                double angle = term->phase + (t * term->frequency);
                sin_sum += term->amplitude * term->frequency * sin(angle);
                if (s > 0)
                    cos_sum += term->amplitude * cos(angle);
            }
            deriv[k] += (s * dpower * cos_sum) - (tpower * sin_sum);
            dpower = tpower;
            tpower *= t;
        }
    }
}


static body_state_t CalcVsopPosVel(const vsop_model_t *model, double tt)
{
    body_state_t state;
    double t = tt / DAYS_PER_MILLENNIUM;
    double sphere[3];   /* lon, lat, r */
    double deriv[3];    /* d(lon)/dt, d(lat)/dt, dr/dt */
    double eclip[3];
    double dr_dt, dlat_dt, dlon_dt;
    double r, coslat, coslon, sinlat, sinlon;

    state.tt = tt;
    VsopCoords(model, t, sphere);
    VsopSphereToRect(sphere[LON_INDEX], sphere[LAT_INDEX], sphere[RAD_INDEX], eclip);
    state.r = VsopRotate(eclip);

    VsopDeriv(model, t, deriv);

    /* Use spherical coords and spherical derivatives to calculate */
    /* the velocity vector in rectangular coordinates. */

    /* Calculate mnemonic variables to help keep the math straight. */
    coslon = cos(sphere[LON_INDEX]);
    sinlon = sin(sphere[LON_INDEX]);
    coslat = cos(sphere[LAT_INDEX]);
    sinlat = sin(sphere[LAT_INDEX]);
    r = sphere[RAD_INDEX];
    dlon_dt = deriv[LON_INDEX];
    dlat_dt = deriv[LAT_INDEX];
    dr_dt   = deriv[RAD_INDEX];

    /* vx = dx/dt */
    eclip[0] = (dr_dt * coslat * coslon) - (r * sinlat * coslon * dlat_dt) - (r * coslat * sinlon * dlon_dt);

    /* vy = dy/dt */
    eclip[1] = (dr_dt * coslat * sinlon) - (r * sinlat * sinlon * dlat_dt) + (r * coslat * coslon * dlon_dt);

    /* vz = dz/dt */
    eclip[2] = (dr_dt * sinlat) + (r * coslat * dlat_dt);

    /* Rotate the velocity vector from ecliptic to equatorial coordinates. */
    state.v = VsopRotate(eclip);

    /* Convert speed units from [AU/millennium] to [AU/day]. */
    VecScale(&state.v, 1 / DAYS_PER_MILLENNIUM);

    return state;
}


static double VsopHelioDistance(const vsop_model_t *model, astro_time_t time)
{
    int s, i;
    double t = time.tt / DAYS_PER_MILLENNIUM;
    double distance = 0.0;
    double tpower = 1.0;
    const vsop_formula_t *formula = &model->formula[2];     /* [2] is the distance part of the formula */

    /*
        The caller only wants to know the distance between the planet and the Sun.
        So we only need to calculate the radial component of the spherical coordinates.
    */

    for (s=0; s < formula->nseries; ++s)
    {
        double sum = 0.0;
        const vsop_series_t *series = &formula->series[s];
        for (i=0; i < series->nterms; ++i)
        {
            const vsop_term_t *term = &series->term[i];
            sum += term->amplitude * cos(term->phase + (t * term->frequency));
        }
        distance += tpower * sum;
        tpower *= t;
    }

    return distance;
}


static void AdjustBarycenter(astro_vector_t *ssb, astro_time_t time, astro_body_t body, double planet_gm)
{
    astro_vector_t planet;
    double shift;

    shift = planet_gm / (planet_gm + SUN_GM);
    planet = CalcVsop(&vsop[body], time);
    ssb->x += shift * planet.x;
    ssb->y += shift * planet.y;
    ssb->z += shift * planet.z;
}


static astro_vector_t CalcSolarSystemBarycenter(astro_time_t time)
{
    astro_vector_t ssb;

    ssb.status = ASTRO_SUCCESS;
    ssb.t = time;
    ssb.x = ssb.y = ssb.z = 0.0;

    AdjustBarycenter(&ssb, time, BODY_JUPITER, JUPITER_GM);
    AdjustBarycenter(&ssb, time, BODY_SATURN,  SATURN_GM);
    AdjustBarycenter(&ssb, time, BODY_URANUS,  URANUS_GM);
    AdjustBarycenter(&ssb, time, BODY_NEPTUNE, NEPTUNE_GM);

    return ssb;
}

/*------------------ begin Pluto integrator ------------------*/

/** @cond DOXYGEN_SKIP */
typedef struct
{
    double          tt;   /* J2000 terrestrial time [days] */
    terse_vector_t  r;    /* position [au] */
    terse_vector_t  v;    /* velocity [au/day] */
    terse_vector_t  a;    /* acceleration [au/day^2] */
} body_grav_calc_t;
/** @endcond */

#define PLUTO_NUM_STATES  41
#define PLUTO_TIME_STEP   36500

static const body_state_t PlutoStateTable[] =
{
    {  -730000.0, {-26.1182072321076, -14.3761681778250,   3.3844025152995}, { 1.6339372163656e-03, -2.7861699588508e-03, -1.3585880229445e-03} }
,   {  -693500.0, { 43.6599275018261,  15.7782921408811,  -8.2269833881374}, {-2.5043046295860e-04,  2.1163039457238e-03,  7.3466073583102e-04} }
,   {  -657000.0, {-17.0086014985033,  33.0590743876420,  15.4080189624259}, {-1.9676551946049e-03, -1.8337707766770e-03,  2.0125441459959e-05} }
,   {  -620500.0, { 26.9005106893171, -21.5285596810214, -14.7987712668075}, { 2.2939261196998e-03,  1.7431871970059e-03, -1.4585639832643e-04} }
,   {  -584000.0, { 20.2303809506997,  43.2669666571891,   7.3829660919234}, {-1.9754081700585e-03,  5.3457141292226e-04,  7.5929169129793e-04} }
,   {  -547500.0, {-22.5571440338751, -19.2958112538447,   0.7806423603826}, { 2.1494578646505e-03, -2.4266772630044e-03, -1.4013084013574e-03} }
,   {  -511000.0, { 43.0236236810360,  19.6179542007347,  -6.8406553041565}, {-4.7729923671058e-04,  2.0208979483877e-03,  7.7191815992131e-04} }
,   {  -474500.0, {-20.4245105862934,  29.5157679318005,  15.3408675727018}, {-1.8003167284198e-03, -2.1025226687937e-03, -1.1262333332859e-04} }
,   {  -438000.0, { 30.7746921076872, -18.2366370153037, -14.9455358798963}, { 2.0113162005465e-03,  1.9353827024189e-03, -2.0937793168297e-06} }
,   {  -401500.0, { 16.7235440456361,  44.0505598318603,   8.6886113939440}, {-2.0565226049264e-03,  3.2710694138777e-04,  7.2006155046579e-04} }
,   {  -365000.0, {-18.4891734360057, -23.1428732331142,  -1.6436720878799}, { 2.5524223225832e-03, -2.0035792463879e-03, -1.3910737531294e-03} }
,   {  -328500.0, { 42.0853950560734,  22.9742531259520,  -5.5131410205412}, {-6.7105845193949e-04,  1.9177289500465e-03,  7.9770011059534e-04} }
,   {  -292000.0, {-23.2753639151193,  25.8185142987694,  15.0553815885983}, {-1.6062295460975e-03, -2.3395961498533e-03, -2.4377362639479e-04} }
,   {  -255500.0, { 33.9015793210130, -14.9421228983498, -14.8664994855707}, { 1.7455105487563e-03,  2.0655068871494e-03,  1.1695000657630e-04} }
,   {  -219000.0, { 13.3770189322702,  44.4442211120183,   9.8260227015847}, {-2.1171882923251e-03,  1.3114714542921e-04,  6.7884578840323e-04} }
,   {  -182500.0, {-14.1723844533379, -26.0054690135836,  -3.8387026446526}, { 2.8419751785822e-03, -1.5579441656564e-03, -1.3408416711060e-03} }
,   {  -146000.0, { 40.9468572586403,  25.9049735920209,  -4.2563362404988}, {-8.3652705194051e-04,  1.8129497136404e-03,  8.1564228273060e-04} }
,   {  -109500.0, {-25.5839689598009,  22.0699164999425,  14.5902026036780}, {-1.3923977856331e-03, -2.5442249745422e-03, -3.7169906721828e-04} }
,   {   -73000.0, { 36.4035708396756, -11.7473067389593, -14.6304139635223}, { 1.5037714418941e-03,  2.1500325702247e-03,  2.1523781242948e-04} }
,   {   -36500.0, { 10.2436041239517,  44.5280986402285,  10.8048664487066}, {-2.1615839201823e-03, -5.1418983893534e-05,  6.3687060751430e-04} }
,   {        0.0, { -9.8753695807739, -27.9789262247367,  -5.7537118247043}, { 3.0287533248818e-03, -1.1276087003636e-03, -1.2651326732361e-03} }
,   {    36500.0, { 39.7009143866164,  28.4327664903825,  -3.0906026170881}, {-9.7720559866138e-04,  1.7121518344796e-03,  8.2822409843551e-04} }
,   {    73000.0, {-27.3620419812795,  18.4265651225706,  13.9975343005914}, {-1.1690934621340e-03, -2.7143131627458e-03, -4.9312695340367e-04} }
,   {   109500.0, { 38.3556091850032,  -8.7643800131842, -14.2951819118807}, { 1.2922798115839e-03,  2.2032141141126e-03,  2.9606522103424e-04} }
,   {   146000.0, {  7.3929490279056,  44.3826789515344,  11.6295002148543}, {-2.1932815453830e-03, -2.1751799585364e-04,  5.9556516201114e-04} }
,   {   182500.0, { -5.8649529029432, -29.1987619981354,  -7.3502494912123}, { 3.1339384323665e-03, -7.4205968379701e-04, -1.1783357537604e-03} }
,   {   219000.0, { 38.4269476345329,  30.5667598351632,  -2.0378379641214}, {-1.0958945370084e-03,  1.6194885149659e-03,  8.3705272532546e-04} }
,   {   255500.0, {-28.6586488201636,  15.0309000931701,  13.3365724093667}, {-9.4611899595408e-04, -2.8506813871559e-03, -6.0508645822989e-04} }
,   {   292000.0, { 39.8319806717528,  -6.0784057667647, -13.9098153586562}, { 1.1117769689167e-03,  2.2362097830152e-03,  3.6230548231153e-04} }
,   {   328500.0, {  4.8371523764030,  44.0723119541530,  12.3146147867802}, {-2.2164547537724e-03, -3.6790365636785e-04,  5.5542723844616e-04} }
,   {   365000.0, { -2.2619763759487, -29.8581508706765,  -8.6502366418978}, { 3.1821176368396e-03, -4.0915169873994e-04, -1.0895893040652e-03} }
,   {   401500.0, { 37.1576590087419,  32.3528396259588,  -1.0950381786229}, {-1.1988412606830e-03,  1.5356290902995e-03,  8.4339118209852e-04} }
,   {   438000.0, {-29.5767402292299,  11.8635359435865,  12.6313230398719}, {-7.2292830060955e-04, -2.9587820140709e-03, -7.0824296450300e-04} }
,   {   474500.0, { 40.9541099577599,  -3.6589805945370, -13.4994699563950}, { 9.5387298337127e-04,  2.2572135462477e-03,  4.1826529781128e-04} }
,   {   511000.0, {  2.4859523114116,  43.6181887566155,  12.8914184596699}, {-2.2339745420393e-03, -5.1034757181916e-04,  5.1485330196245e-04} }
,   {   547500.0, {  1.0594791441638, -30.1357921778687,  -9.7458684762963}, { 3.1921591684898e-03, -1.1305312796150e-04, -9.9954096945965e-04} }
,   {   584000.0, { 35.8778640130144,  33.8942263660709,  -0.2245246362769}, {-1.2941245730845e-03,  1.4560427668319e-03,  8.4762160640137e-04} }
,   {   620500.0, {-30.2026537318923,   8.7794211940578,  11.8609238187578}, {-4.9002221381806e-04, -3.0438768469137e-03, -8.0605935262763e-04} }
,   {   657000.0, { 41.8536204011376,  -1.3790965838042, -13.0624345337527}, { 8.0674627557124e-04,  2.2702374399791e-03,  4.6832587475465e-04} }
,   {   693500.0, {  0.2468843977112,  43.0303960481227,  13.3909343344167}, {-2.2436121787266e-03, -6.5238074250728e-04,  4.7172729553196e-04} }
,   {   730000.0, {  4.2432528370899, -30.1182016908248, -10.7074412313491}, { 3.1725847067411e-03,  1.6098461202270e-04, -9.0672150593868e-04} }
};


static terse_vector_t UpdatePosition(double dt, terse_vector_t r, terse_vector_t v, terse_vector_t a)
{
    r.x += (v.x + a.x*dt/2) * dt;
    r.y += (v.y + a.y*dt/2) * dt;
    r.z += (v.z + a.z*dt/2) * dt;
    return r;
}


static body_state_t AdjustBarycenterPosVel(body_state_t *ssb, double tt, astro_body_t body, double planet_gm)
{
    body_state_t planet;
    double shift;

    /*
        This function does 2 important things:
        1. Adjusts 'ssb' by the effect of one major body on the Solar System Barycenter.
        2, Returns the heliocentric position of that major body.
    */

    shift = planet_gm / (planet_gm + SUN_GM);
    planet = CalcVsopPosVel(&vsop[body], tt);
    VecIncr(&ssb->r, VecMul(shift, planet.r));
    VecIncr(&ssb->v, VecMul(shift, planet.v));

    return planet;
}


static void MajorBodyBary(body_state_t bary[5], double tt)
{
    int p;

    /* bary[0] starts out receiving the Solar System Barycenter. */
    bary[0].tt = tt;
    bary[0].r = VecZero;
    bary[0].v = VecZero;

    /* Calculate heliocentric planet positions and SSB. */
    bary[1] = AdjustBarycenterPosVel(&bary[0], tt, BODY_JUPITER, JUPITER_GM);
    bary[2] = AdjustBarycenterPosVel(&bary[0], tt, BODY_SATURN,  SATURN_GM);
    bary[3] = AdjustBarycenterPosVel(&bary[0], tt, BODY_URANUS,  URANUS_GM);
    bary[4] = AdjustBarycenterPosVel(&bary[0], tt, BODY_NEPTUNE, NEPTUNE_GM);

    for (p=1; p < 5; ++p)
    {
        /* Convert major body [pos, vel] from heliocentric to barycentric. */
        VecDecr(&bary[p].r, bary[0].r);
        VecDecr(&bary[p].v, bary[0].v);
    }

    /* Convert heliocentric SSB to barycentric Sun. */
    VecScale(&bary[0].r, -1.0);
    VecScale(&bary[0].v, -1.0);
}


static void AddAcceleration(terse_vector_t *acc, terse_vector_t small_pos, double gm, terse_vector_t major_pos)
{
    double dx, dy, dz, r2, pull;

    dx = major_pos.x - small_pos.x;
    dy = major_pos.y - small_pos.y;
    dz = major_pos.z - small_pos.z;

    r2 = dx*dx + dy*dy + dz*dz;
    pull = gm / (r2 * sqrt(r2));

    acc->x += dx * pull;
    acc->y += dy * pull;
    acc->z += dz * pull;
}


static terse_vector_t SmallBodyAcceleration(terse_vector_t small_pos, const body_state_t bary[5])
{
    terse_vector_t acc = VecZero;

    /* Use barycentric coordinates of the Sun and major planets to calculate gravitational accelerations. */
    AddAcceleration(&acc, small_pos, SUN_GM,     bary[0].r);
    AddAcceleration(&acc, small_pos, JUPITER_GM, bary[1].r);
    AddAcceleration(&acc, small_pos, SATURN_GM,  bary[2].r);
    AddAcceleration(&acc, small_pos, URANUS_GM,  bary[3].r);
    AddAcceleration(&acc, small_pos, NEPTUNE_GM, bary[4].r);

    return acc;
}


body_grav_calc_t GravSim(           /* out: [pos, vel, acc] of the simulated body at time tt2 */
    body_state_t bary2[5],          /* out: major body barycentric positions at tt2 */
    double tt2,                     /* in:  a target time to be calculated (either before or after tt1 */
    const body_grav_calc_t *calc1)  /* in:  [pos, vel, acc] of the simulated body at time tt1 */
{
    body_grav_calc_t calc2;
    terse_vector_t approx_pos;
    terse_vector_t acc;
    const double dt = tt2 - calc1->tt;

    /* Calculate where the major bodies (Sun, Jupiter...Neptune) will be at the next time step. */
    MajorBodyBary(bary2, tt2);

    /* Estimate position of small body as if current acceleration applies across the whole time interval. */
    /* approx_pos = pos1 + vel1*dt + (1/2)acc*dt^2 */
    approx_pos = UpdatePosition(dt, calc1->r, calc1->v, calc1->a);

    /* Calculate acceleration experienced by small body at approximate next location. */
    acc = SmallBodyAcceleration(approx_pos, bary2);

    /* Calculate the average acceleration of the endpoints. */
    /* This becomes our estimate of the mean effective acceleration over the whole interval. */
    acc = VecMean(acc, calc1->a);

    /* Refine the estimates of [pos, vel, acc] at tt2 using the mean acceleration. */
    calc2.r = UpdatePosition(dt, calc1->r, calc1->v, acc);
    calc2.v = VecAdd(calc1->v, VecMul(dt, acc));
    calc2.a = SmallBodyAcceleration(calc2.r, bary2);
    calc2.tt = tt2;
    return calc2;
}


#define PLUTO_DT 250
#if PLUTO_TIME_STEP % PLUTO_DT != 0
    #error Invalid combination of Pluto time step, time increment.
#endif

#define PLUTO_NSTEPS    ((PLUTO_TIME_STEP / PLUTO_DT) + 1)

/** @cond DOXYGEN_SKIP */
typedef struct
{
    body_grav_calc_t   step[PLUTO_NSTEPS];
}
body_segment_t;
/** @endcond */


/* FIXFIXFIX - Using a global is not thread-safe. Either add thread-locks or change API to accept a cache pointer. */
static body_segment_t *pluto_cache[PLUTO_NUM_STATES-1];


static int ClampIndex(double frac, int nsteps)
{
    int index = (int) floor(frac);
    if (index < 0)
        return 0;
    if (index >= nsteps)
        return nsteps-1;
    return index;
}


static body_grav_calc_t GravFromState(body_state_t bary[5], const body_state_t *state)
{
    body_grav_calc_t calc;

    MajorBodyBary(bary, state->tt);

    calc.tt = state->tt;
    calc.r  = VecAdd(state->r, bary[0].r);      /* convert heliocentric to barycentric */
    calc.v  = VecAdd(state->v, bary[0].v);      /* convert heliocentric to barycentric */
    calc.a  = SmallBodyAcceleration(calc.r, bary);

    return calc;
}


static astro_status_t GetSegment(int *seg_index, body_segment_t *cache[], double tt)
{
    int i;
    body_segment_t reverse;
    body_segment_t *seg;
    body_state_t bary[5];
    double step_tt, ramp;

    if (tt < PlutoStateTable[0].tt || tt > PlutoStateTable[PLUTO_NUM_STATES-1].tt)
    {
        /* We don't bother calculating a segment. Let the caller crawl backward/forward to this time. */
        *seg_index = -1;
        return ASTRO_SUCCESS;
    }

    /* See if we have a segment that straddles the requested time. */
    /* If so, return it. Otherwise, calculate it and return it. */

    *seg_index = ClampIndex((tt - PlutoStateTable[0].tt) / PLUTO_TIME_STEP, PLUTO_NUM_STATES-1);
    if (cache[*seg_index] == NULL)
    {
        /* Allocate memory for the segment (about 11K each). */
        seg = cache[*seg_index] = calloc(1, sizeof(body_segment_t));
        if (seg == NULL)
            return ASTRO_OUT_OF_MEMORY;

        /* Calculate the segment. */
        /* Pick the pair of bracketing body states to fill the segment. */

        /* Each endpoint is exact. */
        seg->step[0] = GravFromState(bary, &PlutoStateTable[*seg_index]);
        seg->step[PLUTO_NSTEPS-1] = GravFromState(bary, &PlutoStateTable[*seg_index + 1]);

        /* Simulate forwards from the lower time bound. */
        step_tt = seg->step[0].tt;
        for (i=1; i < PLUTO_NSTEPS-1; ++i)
            seg->step[i] = GravSim(bary, step_tt += PLUTO_DT, &seg->step[i-1]);

        /* Simulate backwards from the upper time bound. */
        step_tt = seg->step[PLUTO_NSTEPS-1].tt;
        reverse.step[PLUTO_NSTEPS-1] = seg->step[PLUTO_NSTEPS-1];
        for (i=PLUTO_NSTEPS-2; i > 0; --i)
            reverse.step[i] = GravSim(bary, step_tt -= PLUTO_DT, &reverse.step[i+1]);

        /* Fade-mix the two series so that there are no discontinuities. */
        for (i=PLUTO_NSTEPS-2; i > 0; --i)
        {
            ramp = (double)i / (PLUTO_NSTEPS-1);
            seg->step[i].r = VecRamp(seg->step[i].r, reverse.step[i].r, ramp);
            seg->step[i].v = VecRamp(seg->step[i].v, reverse.step[i].v, ramp);
            seg->step[i].a = VecRamp(seg->step[i].a, reverse.step[i].a, ramp);
        }
    }

    return ASTRO_SUCCESS;
}


static terse_vector_t CalcPlutoOneWay(body_state_t bary[5], const body_state_t *init_state, double target_tt, double dt)
{
    body_grav_calc_t calc;
    int i, n;

    calc = GravFromState(bary, init_state);
    n = (int) ceil((target_tt - calc.tt) / dt);
    for (i=0; i < n; ++i)
        calc = GravSim(bary, (i+1 == n) ? target_tt : (calc.tt + dt), &calc);

    return calc.r;
}


static astro_vector_t CalcPluto(astro_time_t time)
{
    terse_vector_t acc, ra, rb, r;
    body_state_t bary[5];
    const body_segment_t *seg;
    int seg_index, left;
    const body_grav_calc_t *s1;
    const body_grav_calc_t *s2;
    astro_status_t status;

    status = GetSegment(&seg_index, pluto_cache, time.tt);
    if (status != ASTRO_SUCCESS)
        return VecError(status, time);

    if (seg_index < 0)
    {
        /* The target time is outside the year range 0000..4000. */
        /* Calculate it by crawling backward from 0000 or forward from 4000. */
        /* FIXFIXFIX - This is super slow. Could optimize this with extra caching if needed. */
        if (time.tt < PlutoStateTable[0].tt)
            r = CalcPlutoOneWay(bary, &PlutoStateTable[0], time.tt, -PLUTO_DT);
        else
            r = CalcPlutoOneWay(bary, &PlutoStateTable[PLUTO_NUM_STATES-1], time.tt, +PLUTO_DT);
    }
    else
    {
        seg = pluto_cache[seg_index];
        left = ClampIndex((time.tt - seg->step[0].tt) / PLUTO_DT, PLUTO_NSTEPS-1);
        s1 = &seg->step[left];
        s2 = &seg->step[left+1];

        /* Find mean acceleration vector over the interval. */
        acc = VecMean(s1->a, s2->a);

        /* Use Newtonian mechanics to extrapolate away from t1 in the positive time direction. */
        ra = UpdatePosition(time.tt - s1->tt, s1->r, s1->v, acc);

        /* Use Newtonian mechanics to extrapolate away from t2 in the negative time direction. */
        rb = UpdatePosition(time.tt - s2->tt, s2->r, s2->v, acc);

        /* Use fade in/out idea to blend the two position estimates. */
        r = VecRamp(ra, rb, (time.tt - s1->tt)/PLUTO_DT);
        MajorBodyBary(bary, time.tt);
    }

    /* Convert barycentric coordinates back to heliocentric coordinates. */
    return PublicVec(time, VecSub(r, bary[0].r));
}

/*------------------ end Pluto integrator ------------------*/


/**
 * @brief Calculates heliocentric Cartesian coordinates of a body in the J2000 equatorial system.
 *
 * This function calculates the position of the given celestial body as a vector,
 * using the center of the Sun as the origin.  The result is expressed as a Cartesian
 * vector in the J2000 equatorial system: the coordinates are based on the mean equator
 * of the Earth at noon UTC on 1 January 2000.
 *
 * The position is not corrected for light travel time or aberration.
 * This is different from the behavior of #Astronomy_GeoVector.
 *
 * If given an invalid value for `body`, this function will fail. The caller should always check
 * the `status` field inside the returned #astro_vector_t for `ASTRO_SUCCESS` (success)
 * or any other value (failure) before trusting the resulting vector.
 *
 * @param body
 *      A body for which to calculate a heliocentric position: the Sun, Moon, any of the planets,
 *      the Solar System Barycenter (SSB), or the Earth Moon Barycenter (EMB).
 * @param time  The date and time for which to calculate the position.
 * @return      A heliocentric position vector of the center of the given body.
 */
astro_vector_t Astronomy_HelioVector(astro_body_t body, astro_time_t time)
{
    astro_vector_t vector, earth;

    switch (body)
    {
    case BODY_SUN:
        vector.status = ASTRO_SUCCESS;
        vector.x = 0.0;
        vector.y = 0.0;
        vector.z = 0.0;
        vector.t = time;
        return vector;

    case BODY_MERCURY:
    case BODY_VENUS:
    case BODY_EARTH:
    case BODY_MARS:
    case BODY_JUPITER:
    case BODY_SATURN:
    case BODY_URANUS:
    case BODY_NEPTUNE:
        return CalcVsop(&vsop[body], time);

    case BODY_PLUTO:
        return CalcPluto(time);

    case BODY_MOON:
        vector = Astronomy_GeoMoon(time);
        earth = CalcEarth(time);
        vector.x += earth.x;
        vector.y += earth.y;
        vector.z += earth.z;
        return vector;

    case BODY_EMB:
        vector = Astronomy_GeoMoon(time);
        earth = CalcEarth(time);
        vector.x = earth.x + (vector.x / (1.0 + EARTH_MOON_MASS_RATIO));
        vector.y = earth.y + (vector.y / (1.0 + EARTH_MOON_MASS_RATIO));
        vector.z = earth.z + (vector.z / (1.0 + EARTH_MOON_MASS_RATIO));
        return vector;

    case BODY_SSB:
        return CalcSolarSystemBarycenter(time);

    default:
        return VecError(ASTRO_INVALID_BODY, time);
    }
}

/**
 * @brief Calculates the distance from a body to the Sun at a given time.
 *
 * Given a date and time, this function calculates the distance between
 * the center of `body` and the center of the Sun.
 * For the planets Mercury through Neptune, this function is significantly
 * more efficient than calling #Astronomy_HelioVector followed by #Astronomy_VectorLength.
 *
 * @param body
 *      A body for which to calculate a heliocentric distance: the Sun, Moon, or any of the planets.
 *
 * @param time
 *      The date and time for which to calculate the heliocentric distance.
 *
 * @return
 *      If successful, an #astro_func_result_t structure whose `status` is `ASTRO_SUCCESS`
 *      and whose `value` holds the heliocentric distance in AU.
 *      Otherwise, `status` reports an error condition.
 */
astro_func_result_t Astronomy_HelioDistance(astro_body_t body, astro_time_t time)
{
    astro_vector_t vector;
    astro_func_result_t result;

    switch (body)
    {
    case BODY_SUN:
        result.status = ASTRO_SUCCESS;
        result.value = 0.0;
        return result;

    case BODY_MERCURY:
    case BODY_VENUS:
    case BODY_EARTH:
    case BODY_MARS:
    case BODY_JUPITER:
    case BODY_SATURN:
    case BODY_URANUS:
    case BODY_NEPTUNE:
        result.status = ASTRO_SUCCESS;
        result.value = VsopHelioDistance(&vsop[body], time);
        return result;

    default:
        /* For non-VSOP objects, fall back to taking the length of the heliocentric vector. */
        vector = Astronomy_HelioVector(body, time);
        if (vector.status != ASTRO_SUCCESS)
            return FuncError(vector.status);
        result.status = ASTRO_SUCCESS;
        result.value = Astronomy_VectorLength(vector);
        return result;
    }
}


/**
 * @brief Calculates geocentric Cartesian coordinates of a body in the J2000 equatorial system.
 *
 * This function calculates the position of the given celestial body as a vector,
 * using the center of the Earth as the origin.  The result is expressed as a Cartesian
 * vector in the J2000 equatorial system: the coordinates are based on the mean equator
 * of the Earth at noon UTC on 1 January 2000.
 *
 * If given an invalid value for `body`, this function will fail. The caller should always check
 * the `status` field inside the returned #astro_vector_t for `ASTRO_SUCCESS` (success)
 * or any other value (failure) before trusting the resulting vector.
 *
 * Unlike #Astronomy_HelioVector, this function always corrects for light travel time.
 * This means the position of the body is "back-dated" by the amount of time it takes
 * light to travel from that body to an observer on the Earth.
 *
 * Also, the position can optionally be corrected for
 * [aberration](https://en.wikipedia.org/wiki/Aberration_of_light), an effect
 * causing the apparent direction of the body to be shifted due to transverse
 * movement of the Earth with respect to the rays of light coming from that body.
 *
 * @param body          A body for which to calculate a heliocentric position: the Sun, Moon, or any of the planets.
 * @param time          The date and time for which to calculate the position.
 * @param aberration    `ABERRATION` to correct for aberration, or `NO_ABERRATION` to leave uncorrected.
 * @return              A geocentric position vector of the center of the given body.
 */
astro_vector_t Astronomy_GeoVector(astro_body_t body, astro_time_t time, astro_aberration_t aberration)
{
    astro_vector_t vector;
    astro_vector_t earth;
    astro_time_t ltime;
    astro_time_t ltime2;
    double dt;
    int iter;

    if (aberration != ABERRATION && aberration != NO_ABERRATION)
        return VecError(ASTRO_INVALID_PARAMETER, time);

    switch (body)
    {
    case BODY_EARTH:
        /* The Earth's geocentric coordinates are always (0,0,0). */
        vector.status = ASTRO_SUCCESS;
        vector.x = 0.0;
        vector.y = 0.0;
        vector.z = 0.0;
        break;

    case BODY_MOON:
        vector = Astronomy_GeoMoon(time);
        break;

    default:
        /* For all other bodies, apply light travel time correction. */

        if (aberration == NO_ABERRATION)
        {
            /* No aberration, so calculate Earth's position once, at the time of observation. */
            earth = CalcEarth(time);
            if (earth.status != ASTRO_SUCCESS)
                return earth;
        }

        ltime = time;
        for (iter=0; iter < 10; ++iter)
        {
            vector = Astronomy_HelioVector(body, ltime);
            if (vector.status != ASTRO_SUCCESS)
                return vector;

            if (aberration == ABERRATION)
            {
                /*
                    Include aberration, so make a good first-order approximation
                    by backdating the Earth's position also.
                    This is confusing, but it works for objects within the Solar System
                    because the distance the Earth moves in that small amount of light
                    travel time (a few minutes to a few hours) is well approximated
                    by a line segment that substends the angle seen from the remote
                    body viewing Earth. That angle is pretty close to the aberration
                    angle of the moving Earth viewing the remote body.
                    In other words, both of the following approximate the aberration angle:
                        (transverse distance Earth moves) / (distance to body)
                        (transverse speed of Earth) / (speed of light).
                */
                earth = CalcEarth(ltime);
                if (earth.status != ASTRO_SUCCESS)
                    return earth;
            }

            /* Convert heliocentric vector to geocentric vector. */
            vector.x -= earth.x;
            vector.y -= earth.y;
            vector.z -= earth.z;

            ltime2 = Astronomy_AddDays(time, -Astronomy_VectorLength(vector) / C_AUDAY);
            dt = fabs(ltime2.tt - ltime.tt);
            if (dt < 1.0e-9)
                goto finished;  /* Ensures we patch 'vector.t' with current time, not ante-dated time. */

            ltime = ltime2;
        }
        return VecError(ASTRO_NO_CONVERGE, time);   /* light travel time solver did not converge */
    }

finished:
    vector.t = time;
    return vector;
}

/**
 * @brief   Calculates equatorial coordinates of a celestial body as seen by an observer on the Earth's surface.
 *
 * Calculates topocentric equatorial coordinates in one of two different systems:
 * J2000 or true-equator-of-date, depending on the value of the `equdate` parameter.
 * Equatorial coordinates include right ascension, declination, and distance in astronomical units.
 *
 * This function corrects for light travel time: it adjusts the apparent location
 * of the observed body based on how long it takes for light to travel from the body to the Earth.
 *
 * This function corrects for *topocentric parallax*, meaning that it adjusts for the
 * angular shift depending on where the observer is located on the Earth. This is most
 * significant for the Moon, because it is so close to the Earth. However, parallax corection
 * has a small effect on the apparent positions of other bodies.
 *
 * Correction for aberration is optional, using the `aberration` parameter.
 *
 * @param body          The celestial body to be observed. Not allowed to be `BODY_EARTH`.
 * @param time          The date and time at which the observation takes place.
 * @param observer      A location on or near the surface of the Earth.
 * @param equdate       Selects the date of the Earth's equator in which to express the equatorial coordinates.
 * @param aberration    Selects whether or not to correct for aberration.
 */
astro_equatorial_t Astronomy_Equator(
    astro_body_t body,
    astro_time_t *time,
    astro_observer_t observer,
    astro_equator_date_t equdate,
    astro_aberration_t aberration)
{
    astro_equatorial_t equ;
    astro_vector_t gc;
    double gc_observer[3];
    double j2000[3];
    double temp[3];
    double datevect[3];

    geo_pos(time, observer, gc_observer);
    gc = Astronomy_GeoVector(body, *time, aberration);
    if (gc.status != ASTRO_SUCCESS)
        return EquError(gc.status);

    j2000[0] = gc.x - gc_observer[0];
    j2000[1] = gc.y - gc_observer[1];
    j2000[2] = gc.z - gc_observer[2];

    switch (equdate)
    {
    case EQUATOR_OF_DATE:
        precession(0.0, j2000, time->tt, temp);
        nutation(time, 0, temp, datevect);
        equ = vector2radec(datevect);
        return equ;

    case EQUATOR_J2000:
        equ = vector2radec(j2000);
        return equ;

    default:
        return EquError(ASTRO_INVALID_PARAMETER);
    }
}

/**
 * @brief Calculates the apparent location of a body relative to the local horizon of an observer on the Earth.
 *
 * Given a date and time, the geographic location of an observer on the Earth, and
 * equatorial coordinates (right ascension and declination) of a celestial body,
 * this function returns horizontal coordinates (azimuth and altitude angles) for the body
 * relative to the horizon at the geographic location.
 *
 * The right ascension `ra` and declination `dec` passed in must be *equator of date*
 * coordinates, based on the Earth's true equator at the date and time of the observation.
 * Otherwise the resulting horizontal coordinates will be inaccurate.
 * Equator of date coordinates can be obtained by calling #Astronomy_Equator, passing in
 * `EQUATOR_OF_DATE` as its `equdate` parameter. It is also recommended to enable
 * aberration correction by passing in `ABERRATION` as the `aberration` parameter.
 *
 * This function optionally corrects for atmospheric refraction.
 * For most uses, it is recommended to pass `REFRACTION_NORMAL` in the `refraction` parameter to
 * correct for optical lensing of the Earth's atmosphere that causes objects
 * to appear somewhat higher above the horizon than they actually are.
 * However, callers may choose to avoid this correction by passing in `REFRACTION_NONE`.
 * If refraction correction is enabled, the azimuth, altitude, right ascension, and declination
 * in the #astro_horizon_t structure returned by this function will all be corrected for refraction.
 * If refraction is disabled, none of these four coordinates will be corrected; in that case,
 * the right ascension and declination in the returned structure will be numerically identical
 * to the respective `ra` and `dec` values passed in.
 *
 * @param time
 *      The date and time of the observation.
 *
 * @param observer
 *      The geographic location of the observer.
 *
 * @param ra
 *      The right ascension of the body in sidereal hours.
 *      See function remarks for more details.
 *
 * @param dec
 *      The declination of the body in degrees. See function remarks for more details.
 *
 * @param refraction
 *      Selects whether to correct for atmospheric refraction, and if so, which model to use.
 *      The recommended value for most uses is `REFRACTION_NORMAL`.
 *      See function remarks for more details.
 *
 * @return
 *      The body's apparent horizontal coordinates and equatorial coordinates, both optionally corrected for refraction.
 */
astro_horizon_t Astronomy_Horizon(
    astro_time_t *time, astro_observer_t observer, double ra, double dec, astro_refraction_t refraction)
{
    astro_horizon_t hor;
    double uze[3], une[3], uwe[3];
    double uz[3], un[3], uw[3];
    double p[3], pz, pn, pw, proj;
    double az, zd;
    double spin_angle;

    double sinlat = sin(observer.latitude * DEG2RAD);
    double coslat = cos(observer.latitude * DEG2RAD);
    double sinlon = sin(observer.longitude * DEG2RAD);
    double coslon = cos(observer.longitude * DEG2RAD);
    double sindc = sin(dec * DEG2RAD);
    double cosdc = cos(dec * DEG2RAD);
    double sinra = sin(ra * 15 * DEG2RAD);
    double cosra = cos(ra * 15 * DEG2RAD);

    uze[0] = coslat * coslon;
    uze[1] = coslat * sinlon;
    uze[2] = sinlat;

    une[0] = -sinlat * coslon;
    une[1] = -sinlat * sinlon;
    une[2] = coslat;

    uwe[0] = sinlon;
    uwe[1] = -coslon;
    uwe[2] = 0.0;

    spin_angle = -15.0 * sidereal_time(time);
    spin(spin_angle, uze, uz);
    spin(spin_angle, une, un);
    spin(spin_angle, uwe, uw);

    p[0] = cosdc * cosra;
    p[1] = cosdc * sinra;
    p[2] = sindc;

    pz = p[0]*uz[0] + p[1]*uz[1] + p[2]*uz[2];
    pn = p[0]*un[0] + p[1]*un[1] + p[2]*un[2];
    pw = p[0]*uw[0] + p[1]*uw[1] + p[2]*uw[2];

    proj = sqrt(pn*pn + pw*pw);
    az = 0.0;
    if (proj > 0.0)
    {
        az = -atan2(pw, pn) * RAD2DEG;
        if (az < 0)
            az += 360;
        else if (az >= 360)
            az -= 360;
    }
    zd = atan2(proj, pz) * RAD2DEG;
    hor.ra = ra;
    hor.dec = dec;

    if (refraction == REFRACTION_NORMAL || refraction == REFRACTION_JPLHOR)
    {
        double zd0, refr;

        zd0 = zd;
        refr = Astronomy_Refraction(refraction, 90.0 - zd);
        zd -= refr;

        if (refr > 0.0 && zd > 3.0e-4)
        {
            int j;
            double sinzd = sin(zd * DEG2RAD);
            double coszd = cos(zd * DEG2RAD);
            double sinzd0 = sin(zd0 * DEG2RAD);
            double coszd0 = cos(zd0 * DEG2RAD);
            double pr[3];

            for (j=0; j<3; ++j)
                pr[j] = ((p[j] - coszd0 * uz[j]) / sinzd0)*sinzd + uz[j]*coszd;

            proj = sqrt(pr[0]*pr[0] + pr[1]*pr[1]);
            if (proj > 0)
            {
                hor.ra = atan2(pr[1], pr[0]) * (RAD2DEG / 15.0);
                if (hor.ra < 0.0)
                    hor.ra += 24.0;
                else if (hor.ra >= 24.0)
                    hor.ra -= 24.0;
            }
            else
            {
                hor.ra = 0.0;
            }
            hor.dec = atan2(pr[2], proj) * RAD2DEG;
        }
    }

    hor.azimuth = az;
    hor.altitude = 90.0 - zd;
    return hor;
}

/**
 * @brief Calculates geocentric ecliptic coordinates for the Sun.
 *
 * This function calculates the position of the Sun as seen from the Earth.
 * The returned value includes both Cartesian and spherical coordinates.
 * The x-coordinate and longitude values in the returned structure are based
 * on the *true equinox of date*: one of two points in the sky where the instantaneous
 * plane of the Earth's equator at the given date and time (the *equatorial plane*)
 * intersects with the plane of the Earth's orbit around the Sun (the *ecliptic plane*).
 * By convention, the apparent location of the Sun at the March equinox is chosen
 * as the longitude origin and x-axis direction, instead of the one for September.
 *
 * `Astronomy_SunPosition` corrects for precession and nutation of the Earth's axis
 * in order to obtain the exact equatorial plane at the given time.
 *
 * This function can be used for calculating changes of seasons: equinoxes and solstices.
 * In fact, the function #Astronomy_Seasons does use this function for that purpose.
 *
 * @param time
 *      The date and time for which to calculate the Sun's position.
 *
 * @return
 *      The ecliptic coordinates of the Sun using the Earth's true equator of date.
 */
astro_ecliptic_t Astronomy_SunPosition(astro_time_t time)
{
    astro_time_t adjusted_time;
    astro_vector_t earth2000;
    double sun2000[3];
    double stemp[3];
    double sun_ofdate[3];
    double true_obliq;

    /* Correct for light travel time from the Sun. */
    /* Otherwise season calculations (equinox, solstice) will all be early by about 8 minutes! */
    adjusted_time = Astronomy_AddDays(time, -1.0 / C_AUDAY);

    earth2000 = CalcEarth(adjusted_time);
    if (earth2000.status != ASTRO_SUCCESS)
        return EclError(earth2000.status);

    /* Convert heliocentric location of Earth to geocentric location of Sun. */
    sun2000[0] = -earth2000.x;
    sun2000[1] = -earth2000.y;
    sun2000[2] = -earth2000.z;

    /* Convert to equatorial Cartesian coordinates of date. */
    precession(0.0, sun2000, adjusted_time.tt, stemp);
    nutation(&adjusted_time, 0, stemp, sun_ofdate);

    /* Convert equatorial coordinates to ecliptic coordinates. */
    true_obliq = DEG2RAD * e_tilt(&adjusted_time).tobl;
    return RotateEquatorialToEcliptic(sun_ofdate, true_obliq);
}

/**
 * @brief Converts J2000 equatorial Cartesian coordinates to J2000 ecliptic coordinates.
 *
 * Given coordinates relative to the Earth's equator at J2000 (the instant of noon UTC
 * on 1 January 2000), this function converts those coordinates to J2000 ecliptic coordinates,
 * which are relative to the plane of the Earth's orbit around the Sun.
 *
 * @param equ
 *      Equatorial coordinates in the J2000 frame of reference.
 *      You can call #Astronomy_GeoVector to obtain suitable equatorial coordinates.
 *
 * @return
 *      Ecliptic coordinates in the J2000 frame of reference.
 */
astro_ecliptic_t Astronomy_Ecliptic(astro_vector_t equ)
{
    /* Based on NOVAS functions equ2ecl() and equ2ecl_vec(). */
    static const double ob2000 = 0.40909260059599012;   /* mean obliquity of the J2000 ecliptic in radians */
    double pos[3];

    if (equ.status != ASTRO_SUCCESS)
        return EclError(equ.status);

    pos[0] = equ.x;
    pos[1] = equ.y;
    pos[2] = equ.z;

    return RotateEquatorialToEcliptic(pos, ob2000);
}

/**
 * @brief   Calculates heliocentric ecliptic longitude of a body based on the J2000 equinox.
 *
 * This function calculates the angle around the plane of the Earth's orbit
 * of a celestial body, as seen from the center of the Sun.
 * The angle is measured prograde (in the direction of the Earth's orbit around the Sun)
 * in degrees from the J2000 equinox. The ecliptic longitude is always in the range [0, 360).
 *
 * @param body
 *      A body other than the Sun.
 *
 * @param time
 *      The date and time at which the body's ecliptic longitude is to be calculated.
 *
 * @return
 *      On success, returns a structure whose `status` is `ASTRO_SUCCESS` and whose
 *      `angle` holds the ecliptic longitude in degrees.
 *      On failure, `status` holds a value other than `ASTRO_SUCCESS`.
 */
astro_angle_result_t Astronomy_EclipticLongitude(astro_body_t body, astro_time_t time)
{
    astro_vector_t hv;
    astro_ecliptic_t eclip;
    astro_angle_result_t result;

    if (body == BODY_SUN)
        return AngleError(ASTRO_INVALID_BODY);      /* cannot calculate heliocentric longitude of the Sun */

    hv = Astronomy_HelioVector(body, time);
    eclip = Astronomy_Ecliptic(hv);     /* checks for errors in hv, so we don't have to here */
    if (eclip.status != ASTRO_SUCCESS)
        return AngleError(eclip.status);

    result.angle = eclip.elon;
    result.status = ASTRO_SUCCESS;
    return result;
}

static astro_ecliptic_t RotateEquatorialToEcliptic(const double pos[3], double obliq_radians)
{
    astro_ecliptic_t ecl;
    double cos_ob, sin_ob;
    double xyproj;

    cos_ob = cos(obliq_radians);
    sin_ob = sin(obliq_radians);

    ecl.ex = +pos[0];
    ecl.ey = +pos[1]*cos_ob + pos[2]*sin_ob;
    ecl.ez = -pos[1]*sin_ob + pos[2]*cos_ob;

    xyproj = sqrt(ecl.ex*ecl.ex + ecl.ey*ecl.ey);
    if (xyproj > 0.0)
    {
        ecl.elon = RAD2DEG * atan2(ecl.ey, ecl.ex);
        if (ecl.elon < 0.0)
            ecl.elon += 360.0;
    }
    else
        ecl.elon = 0.0;

    ecl.elat = RAD2DEG * atan2(ecl.ez, xyproj);
    ecl.status = ASTRO_SUCCESS;
    return ecl;
}

static astro_func_result_t sun_offset(void *context, astro_time_t time)
{
    astro_func_result_t result;
    double targetLon = *((double *)context);
    astro_ecliptic_t ecl = Astronomy_SunPosition(time);
    if (ecl.status != ASTRO_SUCCESS)
        return FuncError(ecl.status);
    result.value = LongitudeOffset(ecl.elon - targetLon);
    result.status = ASTRO_SUCCESS;
    return result;
}

/**
 * @brief
 *      Searches for the time when the Sun reaches an apparent ecliptic longitude as seen from the Earth.
 *
 * This function finds the moment in time, if any exists in the given time window,
 * that the center of the Sun reaches a specific ecliptic longitude as seen from the center of the Earth.
 *
 * This function can be used to determine equinoxes and solstices.
 * However, it is usually more convenient and efficient to call #Astronomy_Seasons
 * to calculate all equinoxes and solstices for a given calendar year.
 *
 * The function searches the window of time specified by `startTime` and `startTime+limitDays`.
 * The search will return an error if the Sun never reaches the longitude `targetLon` or
 * if the window is so large that the longitude ranges more than 180 degrees within it.
 * It is recommended to keep the window smaller than 10 days when possible.
 *
 * @param targetLon
 *      The desired ecliptic longitude in degrees, relative to the true equinox of date.
 *      This may be any value in the range [0, 360), although certain values have
 *      conventional meanings:
 *      0 = March equinox, 90 = June solstice, 180 = September equinox, 270 = December solstice.
 *
 * @param startTime
 *      The date and time for starting the search for the desired longitude event.
 *
 * @param limitDays
 *      The real-valued number of days, which when added to `startTime`, limits the
 *      range of time over which the search looks.
 *      It is recommended to keep this value between 1 and 10 days.
 *      See function remarks for more details.
 *
 * @return
 *      If successful, the `status` field in the returned structure will contain `ASTRO_SUCCESS`
 *      and the `time` field will contain the date and time the Sun reaches the target longitude.
 *      Any other value indicates an error.
 *      See remarks in #Astronomy_Search (which this function calls) for more information about possible error codes.
 */
astro_search_result_t Astronomy_SearchSunLongitude(
    double targetLon,
    astro_time_t startTime,
    double limitDays)
{
    astro_time_t t2 = Astronomy_AddDays(startTime, limitDays);
    return Astronomy_Search(sun_offset, &targetLon, startTime, t2, 1.0);
}

/** @cond DOXYGEN_SKIP */
#define CALLFUNC(f,t)  \
    do { \
        funcres = func(context, (t)); \
        if (funcres.status != ASTRO_SUCCESS) return SearchError(funcres.status); \
        (f) = funcres.value; \
    } while(0)
/** @endcond */

/**
 * @brief Searches for a time at which a function's value increases through zero.
 *
 * Certain astronomy calculations involve finding a time when an event occurs.
 * Often such events can be defined as the root of a function:
 * the time at which the function's value becomes zero.
 *
 * `Astronomy_Search` finds the *ascending root* of a function: the time at which
 * the function's value becomes zero while having a positive slope. That is, as time increases,
 * the function transitions from a negative value, through zero at a specific moment,
 * to a positive value later. The goal of the search is to find that specific moment.
 *
 * The search function is specified by two parameters: `func` and `context`.
 * The `func` parameter is a pointer to the function itself, which accepts a time
 * and a context containing any other arguments needed to evaluate the function.
 * The `context` parameter supplies that context for the given search.
 * As an example, a caller may wish to find the moment a celestial body reaches a certain
 * ecliptic longitude. In that case, the caller might create a structure that contains
 * an #astro_body_t member to specify the body and a `double` to hold the target longitude.
 * The function would cast the pointer `context` passed in as a pointer to that structure type.
 * It could subtract the target longitude from the actual longitude at a given time;
 * thus the difference would equal zero at the moment in time the planet reaches the
 * desired longitude.
 *
 * The `func` returns an #astro_func_result_t structure every time it is called.
 * If the returned structure has a value of `status` other than `ASTRO_SUCCESS`,
 * the search immediately fails and reports that same error code in the `status`
 * returned by `Astronomy_Search`. Otherwise, `status` is `ASTRO_SUCCESS` and
 * `value` is the value of the function, and the search proceeds until it either
 * finds the ascending root or fails for some reason.
 *
 * The search calls `func` repeatedly to rapidly narrow in on any ascending
 * root within the time window specified by `t1` and `t2`. The search never
 * reports a solution outside this time window.
 *
 * `Astronomy_Search` uses a combination of bisection and quadratic interpolation
 * to minimize the number of function calls. However, it is critical that the
 * supplied time window be small enough that there cannot be more than one root
 * (ascedning or descending) within it; otherwise the search can fail.
 * Beyond that, it helps to make the time window as small as possible, ideally
 * such that the function itself resembles a smooth parabolic curve within that window.
 *
 * If an ascending root is not found, or more than one root
 * (ascending and/or descending) exists within the window `t1`..`t2`,
 * the search will fail with status code `ASTRO_SEARCH_FAILURE`.
 *
 * If the search does not converge within 20 iterations, it will fail
 * with status code `ASTRO_NO_CONVERGE`.
 *
 * @param func
 *      The function for which to find the time of an ascending root.
 *      See function remarks for more details.
 *
 * @param context
 *      Any ancillary data needed by the function `func` to calculate a value.
 *      The data type varies depending on the function passed in.
 *      For example, the function may involve a specific celestial body that
 *      must be specified somehow.
 *
 * @param t1
 *      The lower time bound of the search window.
 *      See function remarks for more details.
 *
 * @param t2
 *      The upper time bound of the search window.
 *      See function remarks for more details.
 *
 * @param dt_tolerance_seconds
 *      Specifies an amount of time in seconds within which a bounded ascending root
 *      is considered accurate enough to stop. A typical value is 1 second.
 *
 * @return
 *      If successful, the returned structure has `status` equal to `ASTRO_SUCCESS`
 *      and `time` set to a value within `dt_tolerance_seconds` of an ascending root.
 *      On success, the `time` value will always be in the inclusive range [`t1`, `t2`].
 *      If the search fails, `status` will be set to a value other than `ASTRO_SUCCESS`.
 *      See function remarks for more details.
 */
astro_search_result_t Astronomy_Search(
    astro_search_func_t func,
    void *context,
    astro_time_t t1,
    astro_time_t t2,
    double dt_tolerance_seconds)
{
    astro_search_result_t result;
    astro_time_t tmid;
    astro_time_t tq;
    astro_func_result_t funcres;
    double f1, f2, fmid=0.0, fq, dt_days, dt, dt_guess;
    double q_x, q_ut, q_df_dt;
    const int iter_limit = 20;
    int iter = 0;
    int calc_fmid = 1;

    dt_days = fabs(dt_tolerance_seconds / SECONDS_PER_DAY);
    CALLFUNC(f1, t1);
    CALLFUNC(f2, t2);

    for(;;)
    {
        if (++iter > iter_limit)
            return SearchError(ASTRO_NO_CONVERGE);

        dt = (t2.tt - t1.tt) / 2.0;
        tmid = Astronomy_AddDays(t1, dt);
        if (fabs(dt) < dt_days)
        {
            /* We are close enough to the event to stop the search. */
            result.time = tmid;
            result.status = ASTRO_SUCCESS;
            return result;
        }

        if (calc_fmid)
            CALLFUNC(fmid, tmid);
        else
            calc_fmid = 1;      /* we already have the correct value of fmid from the previous loop */

        /* Quadratic interpolation: */
        /* Try to find a parabola that passes through the 3 points we have sampled: */
        /* (t1,f1), (tmid,fmid), (t2,f2) */

        if (QuadInterp(tmid.ut, t2.ut - tmid.ut, f1, fmid, f2, &q_x, &q_ut, &q_df_dt))
        {
            tq = Astronomy_TimeFromDays(q_ut);
            CALLFUNC(fq, tq);
            if (q_df_dt != 0.0)
            {
                dt_guess = fabs(fq / q_df_dt);
                if (dt_guess < dt_days)
                {
                    /* The estimated time error is small enough that we can quit now. */
                    result.time = tq;
                    result.status = ASTRO_SUCCESS;
                    return result;
                }

                /* Try guessing a tighter boundary with the interpolated root at the center. */
                dt_guess *= 1.2;
                if (dt_guess < dt/10.0)
                {
                    astro_time_t tleft = Astronomy_AddDays(tq, -dt_guess);
                    astro_time_t tright = Astronomy_AddDays(tq, +dt_guess);
                    if ((tleft.ut - t1.ut)*(tleft.ut - t2.ut) < 0)
                    {
                        if ((tright.ut - t1.ut)*(tright.ut - t2.ut) < 0)
                        {
                            double fleft, fright;
                            CALLFUNC(fleft, tleft);
                            CALLFUNC(fright, tright);
                            if (fleft<0.0 && fright>=0.0)
                            {
                                f1 = fleft;
                                f2 = fright;
                                t1 = tleft;
                                t2 = tright;
                                fmid = fq;
                                calc_fmid = 0;  /* save a little work -- no need to re-calculate fmid next time around the loop */
                                continue;
                            }
                        }
                    }
                }
            }
        }

        /* After quadratic interpolation attempt. */
        /* Now just divide the region in two parts and pick whichever one appears to contain a root. */
        if (f1 < 0.0 && fmid >= 0.0)
        {
            t2 = tmid;
            f2 = fmid;
            continue;
        }

        if (fmid < 0.0 && f2 >= 0.0)
        {
            t1 = tmid;
            f1 = fmid;
            continue;
        }

        /* Either there is no ascending zero-crossing in this range */
        /* or the search window is too wide (more than one zero-crossing). */
        return SearchError(ASTRO_SEARCH_FAILURE);
    }
}

static int QuadInterp(
    double tm, double dt, double fa, double fm, double fb,
    double *out_x, double *out_t, double *out_df_dt)
{
    double Q, R, S;
    double u, ru, x1, x2;

    Q = (fb + fa)/2.0 - fm;
    R = (fb - fa)/2.0;
    S = fm;

    if (Q == 0.0)
    {
        /* This is a line, not a parabola. */
        if (R == 0.0)
            return 0;       /* This is a HORIZONTAL line... can't make progress! */
        *out_x = -S / R;
        if (*out_x < -1.0 || *out_x > +1.0)
            return 0;   /* out of bounds */
    }
    else
    {
        /* This really is a parabola. Find roots x1, x2. */
        u = R*R - 4*Q*S;
        if (u <= 0.0)
            return 0;   /* can't solve if imaginary, or if vertex of parabola is tangent. */

        ru = sqrt(u);
        x1 = (-R + ru) / (2.0 * Q);
        x2 = (-R - ru) / (2.0 * Q);
        if (-1.0 <= x1 && x1 <= +1.0)
        {
            if (-1.0 <= x2 && x2 <= +1.0)
                return 0;   /* two roots are within bounds; we require a unique zero-crossing. */
            *out_x = x1;
        }
        else if (-1.0 <= x2 && x2 <= +1.0)
            *out_x = x2;
        else
            return 0;   /* neither root is within bounds */
    }

    *out_t = tm + (*out_x)*dt;
    *out_df_dt = (2*Q*(*out_x) + R) / dt;
    return 1;   /* success */
}

static astro_status_t FindSeasonChange(double targetLon, int year, int month, int day, astro_time_t *time)
{
    astro_time_t startTime = Astronomy_MakeTime(year, month, day, 0, 0, 0.0);
    astro_search_result_t result = Astronomy_SearchSunLongitude(targetLon, startTime, 4.0);
    *time = result.time;
    return result.status;
}

/**
 * @brief Finds both equinoxes and both solstices for a given calendar year.
 *
 * The changes of seasons are defined by solstices and equinoxes.
 * Given a calendar year number, this function calculates the
 * March and September equinoxes and the June and December solstices.
 *
 * The equinoxes are the moments twice each year when the plane of the
 * Earth's equator passes through the center of the Sun. In other words,
 * the Sun's declination is zero at both equinoxes.
 * The March equinox defines the beginning of spring in the northern hemisphere
 * and the beginning of autumn in the southern hemisphere.
 * The September equinox defines the beginning of autumn in the northern hemisphere
 * and the beginning of spring in the southern hemisphere.
 *
 * The solstices are the moments twice each year when one of the Earth's poles
 * is most tilted toward the Sun. More precisely, the Sun's declination reaches
 * its minimum value at the December solstice, which defines the beginning of
 * winter in the northern hemisphere and the beginning of summer in the southern
 * hemisphere. The Sun's declination reaches its maximum value at the June solstice,
 * which defines the beginning of summer in the northern hemisphere and the beginning
 * of winter in the southern hemisphere.
 *
 * @param year
 *      The calendar year number for which to calculate equinoxes and solstices.
 *      The value may be any integer, but only the years 1800 through 2100 have been
 *      validated for accuracy: unit testing against data from the
 *      United States Naval Observatory confirms that all equinoxes and solstices
 *      for that range of years are within 2 minutes of the correct time.
 *
 * @return
 *      The times of the four seasonal changes in the given calendar year.
 *      This function should always succeed. However, to be safe, callers
 *      should check the `status` field of the returned structure to make sure
 *      it contains `ASTRO_SUCCESS`. Any failures indicate a bug in the algorithm
 *      and should be [reported as an issue](https://github.com/cosinekitty/astronomy/issues).
 */
astro_seasons_t Astronomy_Seasons(int year)
{
    astro_seasons_t seasons;
    astro_status_t  status;

    seasons.status = ASTRO_SUCCESS;

    status = FindSeasonChange(  0, year,  3, 19, &seasons.mar_equinox);
    if (status != ASTRO_SUCCESS) seasons.status = status;

    status = FindSeasonChange( 90, year,  6, 19, &seasons.jun_solstice);
    if (status != ASTRO_SUCCESS) seasons.status = status;

    status = FindSeasonChange(180, year,  9, 21, &seasons.sep_equinox);
    if (status != ASTRO_SUCCESS) seasons.status = status;

    status = FindSeasonChange(270, year, 12, 20, &seasons.dec_solstice);
    if (status != ASTRO_SUCCESS) seasons.status = status;

    return seasons;
}

/**
 * @brief   Returns the angle between the given body and the Sun, as seen from the Earth.
 *
 * This function calculates the angular separation between the given body and the Sun,
 * as seen from the center of the Earth. This angle is helpful for determining how
 * easy it is to see the body away from the glare of the Sun.
 *
 * @param body
 *      The celestial body whose angle from the Sun is to be measured.
 *      Not allowed to be `BODY_EARTH`.
 *
 * @param time
 *      The time at which the observation is made.
 *
 * @return
 *      If successful, the returned structure contains `ASTRO_SUCCESS` in the `status` field
 *      and `angle` holds the angle in degrees between the Sun and the specified body as
 *      seen from the center of the Earth.
 *      If an error occurs, the `status` field contains a value other than `ASTRO_SUCCESS`
 *      that indicates the error condition.
 */
astro_angle_result_t Astronomy_AngleFromSun(astro_body_t body, astro_time_t time)
{
    astro_vector_t sv, bv;

    if (body == BODY_EARTH)
        return AngleError(ASTRO_EARTH_NOT_ALLOWED);

    sv = Astronomy_GeoVector(BODY_SUN, time, ABERRATION);
    if (sv.status != ASTRO_SUCCESS)
        return AngleError(sv.status);

    bv = Astronomy_GeoVector(body, time, ABERRATION);
    if (bv.status != ASTRO_SUCCESS)
        return AngleError(bv.status);

    return AngleBetween(sv, bv);
}

/**
 * @brief
 *      Determines visibility of a celestial body relative to the Sun, as seen from the Earth.
 *
 * This function returns an #astro_elongation_t structure, which provides the following
 * information about the given celestial body at the given time:
 *
 * - `visibility` is an enumerated type that specifies whether the body is more easily seen
 *    in the morning before sunrise, or in the evening after sunset.
 *
 * - `elongation` is the angle in degrees between two vectors: one from the center of the Earth to the
 *    center of the Sun, the other from the center of the Earth to the center of the specified body.
 *    This angle indicates how far away the body is from the glare of the Sun.
 *    The elongation angle is always in the range [0, 180].
 *
 * - `ecliptic_separation` is the absolute value of the difference between the body's ecliptic longitude
 *   and the Sun's ecliptic longitude, both as seen from the center of the Earth. This angle measures
 *   around the plane of the Earth's orbit, and ignores how far above or below that plane the body is.
 *   The ecliptic separation is measured in degrees and is always in the range [0, 180].
 *
 * @param body
 *      The celestial body whose visibility is to be calculated.
 *
 * @param time
 *      The date and time of the observation.
 *
 * @return
 *      If successful, the `status` field in the returned structure contains `ASTRO_SUCCESS`
 *      and all the other fields in the structure are valid. On failure, `status` contains
 *      some other value as an error code and the other fields contain invalid values.
 */
astro_elongation_t Astronomy_Elongation(astro_body_t body, astro_time_t time)
{
    astro_elongation_t result;
    astro_angle_result_t angres;

    angres = Astronomy_LongitudeFromSun(body, time);
    if (angres.status != ASTRO_SUCCESS)
        return ElongError(angres.status);

    if (angres.angle > 180.0)
    {
        result.visibility = VISIBLE_MORNING;
        result.ecliptic_separation = 360.0 - angres.angle;
    }
    else
    {
        result.visibility = VISIBLE_EVENING;
        result.ecliptic_separation = angres.angle;
    }

    angres = Astronomy_AngleFromSun(body, time);
    if (angres.status != ASTRO_SUCCESS)
        return ElongError(angres.status);

    result.elongation = angres.angle;
    result.time = time;
    result.status = ASTRO_SUCCESS;

    return result;
}

static astro_func_result_t neg_elong_slope(void *context, astro_time_t time)
{
    static const double dt = 0.1;
    astro_angle_result_t e1, e2;
    astro_func_result_t result;
    astro_body_t body = *((astro_body_t *)context);
    astro_time_t t1 = Astronomy_AddDays(time, -dt/2.0);
    astro_time_t t2 = Astronomy_AddDays(time, +dt/2.0);

    e1 = Astronomy_AngleFromSun(body, t1);
    if (e1.status != ASTRO_SUCCESS)
        return FuncError(e1.status);

    e2 = Astronomy_AngleFromSun(body, t2);
    if (e2.status)
        return FuncError(e2.status);

    result.value = (e1.angle - e2.angle)/dt;
    result.status = ASTRO_SUCCESS;
    return result;
}

/**
 * @brief
 *      Finds a date and time when Mercury or Venus reaches its maximum angle from the Sun as seen from the Earth.
 *
 * Mercury and Venus are are often difficult to observe because they are closer to the Sun than the Earth is.
 * Mercury especially is almost always impossible to see because it gets lost in the Sun's glare.
 * The best opportunities for spotting Mercury, and the best opportunities for viewing Venus through
 * a telescope without atmospheric interference, are when these planets reach maximum elongation.
 * These are events where the planets reach the maximum angle from the Sun as seen from the Earth.
 *
 * This function solves for those times, reporting the next maximum elongation event's date and time,
 * the elongation value itself, the relative longitude with the Sun, and whether the planet is best
 * observed in the morning or evening. See #Astronomy_Elongation for more details about the returned structure.
 *
 * @param body
 *      Either `BODY_MERCURY` or `BODY_VENUS`. Any other value will fail with the error `ASTRO_INVALID_BODY`.
 *      To find the best viewing opportunites for planets farther from the Sun than the Earth is (Mars through Pluto)
 *      use #Astronomy_SearchRelativeLongitude to find the next opposition event.
 *
 * @param startTime
 *      The date and time at which to begin the search. The maximum elongation event found will always
 *      be the first one that occurs after this date and time.
 *
 * @return
 *      If successful, the `status` field of the returned structure will be `ASTRO_SUCCESS`
 *      and the other structure fields will be valid. Otherwise, `status` will contain
 *      some other value indicating an error.
 */
astro_elongation_t Astronomy_SearchMaxElongation(astro_body_t body, astro_time_t startTime)
{
    double s1, s2;
    int iter;
    astro_angle_result_t plon, elon;
    astro_time_t t_start;
    double rlon, rlon_lo, rlon_hi, adjust_days;
    astro_func_result_t syn;
    astro_search_result_t search1, search2, searchx;
    astro_time_t t1, t2;
    astro_func_result_t m1, m2;

    /* Determine the range of relative longitudes within which maximum elongation can occur for this planet. */
    switch (body)
    {
    case BODY_MERCURY:
        s1 = 50.0;
        s2 = 85.0;
        break;

    case BODY_VENUS:
        s1 = 40.0;
        s2 = 50.0;
        break;

    default:
        /* SearchMaxElongation works for Mercury and Venus only. */
        return ElongError(ASTRO_INVALID_BODY);
    }

    syn = SynodicPeriod(body);
    if (syn.status != ASTRO_SUCCESS)
        return ElongError(syn.status);

    iter = 0;
    while (++iter <= 2)
    {
        plon = Astronomy_EclipticLongitude(body, startTime);
        if (plon.status != ASTRO_SUCCESS)
            return ElongError(plon.status);

        elon = Astronomy_EclipticLongitude(BODY_EARTH, startTime);
        if (elon.status != ASTRO_SUCCESS)
            return ElongError(elon.status);

        rlon = LongitudeOffset(plon.angle - elon.angle);    /* clamp to (-180, +180] */

        /* The slope function is not well-behaved when rlon is near 0 degrees or 180 degrees */
        /* because there is a cusp there that causes a discontinuity in the derivative. */
        /* So we need to guard against searching near such times. */
        if (rlon >= -s1 && rlon < +s1)
        {
            /* Seek to the window [+s1, +s2]. */
            adjust_days = 0.0;
            /* Search forward for the time t1 when rel lon = +s1. */
            rlon_lo = +s1;
            /* Search forward for the time t2 when rel lon = +s2. */
            rlon_hi = +s2;
        }
        else if (rlon > +s2 || rlon < -s2)
        {
            /* Seek to the next search window at [-s2, -s1]. */
            adjust_days = 0.0;
            /* Search forward for the time t1 when rel lon = -s2. */
            rlon_lo = -s2;
            /* Search forward for the time t2 when rel lon = -s1. */
            rlon_hi = -s1;
        }
        else if (rlon >= 0.0)
        {
            /* rlon must be in the middle of the window [+s1, +s2]. */
            /* Search BACKWARD for the time t1 when rel lon = +s1. */
            adjust_days = -syn.value / 4.0;
            rlon_lo = +s1;
            rlon_hi = +s2;
            /* Search forward from t1 to find t2 such that rel lon = +s2. */
        }
        else
        {
            /* rlon must be in the middle of the window [-s2, -s1]. */
            /* Search BACKWARD for the time t1 when rel lon = -s2. */
            adjust_days = -syn.value / 4.0;
            rlon_lo = -s2;
            /* Search forward from t1 to find t2 such that rel lon = -s1. */
            rlon_hi = -s1;
        }

        t_start = Astronomy_AddDays(startTime, adjust_days);

        search1 = Astronomy_SearchRelativeLongitude(body, rlon_lo, t_start);
        if (search1.status != ASTRO_SUCCESS)
            return ElongError(search1.status);
        t1 = search1.time;

        search2 = Astronomy_SearchRelativeLongitude(body, rlon_hi, t1);
        if (search2.status != ASTRO_SUCCESS)
            return ElongError(search2.status);
        t2 = search2.time;

        /* Now we have a time range [t1,t2] that brackets a maximum elongation event. */
        /* Confirm the bracketing. */
        m1 = neg_elong_slope(&body, t1);
        if (m1.status != ASTRO_SUCCESS)
            return ElongError(m1.status);

        if (m1.value >= 0)
            return ElongError(ASTRO_INTERNAL_ERROR);    /* there is a bug in the bracketing algorithm! */

        m2 = neg_elong_slope(&body, t2);
        if (m2.status != ASTRO_SUCCESS)
            return ElongError(m2.status);

        if (m2.value <= 0)
            return ElongError(ASTRO_INTERNAL_ERROR);    /* there is a bug in the bracketing algorithm! */

        /* Use the generic search algorithm to home in on where the slope crosses from negative to positive. */
        searchx = Astronomy_Search(neg_elong_slope, &body, t1, t2, 10.0);
        if (searchx.status != ASTRO_SUCCESS)
            return ElongError(searchx.status);

        if (searchx.time.tt >= startTime.tt)
            return Astronomy_Elongation(body, searchx.time);

        /* This event is in the past (earlier than startTime). */
        /* We need to search forward from t2 to find the next possible window. */
        /* We never need to search more than twice. */
        startTime = Astronomy_AddDays(t2, 1.0);
    }

    return ElongError(ASTRO_SEARCH_FAILURE);
}

/**
 * @brief
 *      Returns a body's ecliptic longitude with respect to the Sun, as seen from the Earth.
 *
 * This function can be used to determine where a planet appears around the ecliptic plane
 * (the plane of the Earth's orbit around the Sun) as seen from the Earth,
 * relative to the Sun's apparent position.
 *
 * The angle starts at 0 when the body and the Sun are at the same ecliptic longitude
 * as seen from the Earth. The angle increases in the prograde direction
 * (the direction that the planets orbit the Sun and the Moon orbits the Earth).
 *
 * When the angle is 180 degrees, it means the Sun and the body appear on opposite sides
 * of the sky for an Earthly observer. When `body` is a planet whose orbit around the
 * Sun is farther than the Earth's, 180 degrees indicates opposition. For the Moon,
 * it indicates a full moon.
 *
 * The angle keeps increasing up to 360 degrees as the body's apparent prograde
 * motion continues relative to the Sun. When the angle reaches 360 degrees, it starts
 * over at 0 degrees.
 *
 * Values between 0 and 180 degrees indicate that the body is visible in the evening sky
 * after sunset.  Values between 180 degrees and 360 degrees indicate that the body
 * is visible in the morning sky before sunrise.
 *
 * @param body
 *      The celestial body for which to find longitude from the Sun.
 *
 * @param time
 *      The date and time of the observation.
 *
 * @return
 *      On success, the `status` field in the returned structure holds `ASTRO_SUCCESS` and
 *      the `angle` field holds a value in the range [0, 360).
 *      On failure, the `status` field contains some other value indicating an error condition.
 */
astro_angle_result_t Astronomy_LongitudeFromSun(astro_body_t body, astro_time_t time)
{
    astro_vector_t sv, bv;
    astro_ecliptic_t se, be;
    astro_angle_result_t result;

    if (body == BODY_EARTH)
        return AngleError(ASTRO_EARTH_NOT_ALLOWED);

    sv = Astronomy_GeoVector(BODY_SUN, time, NO_ABERRATION);
    se = Astronomy_Ecliptic(sv);        /* checks for errors in sv */
    if (se.status != ASTRO_SUCCESS)
        return AngleError(se.status);

    bv = Astronomy_GeoVector(body, time, NO_ABERRATION);
    be = Astronomy_Ecliptic(bv);        /* checks for errors in bv */
    if (be.status != ASTRO_SUCCESS)
        return AngleError(be.status);

    result.status = ASTRO_SUCCESS;
    result.angle = NormalizeLongitude(be.elon - se.elon);
    return result;
}

/**
 * @brief
 *      Returns the Moon's phase as an angle from 0 to 360 degrees.
 *
 * This function determines the phase of the Moon using its apparent
 * ecliptic longitude relative to the Sun, as seen from the center of the Earth.
 * Certain values of the angle have conventional definitions:
 *
 * - 0 = new moon
 * - 90 = first quarter
 * - 180 = full moon
 * - 270 = third quarter
 *
 * @param time
 *      The date and time of the observation.
 *
 * @return
 *      On success, the function returns the angle as described in the function remarks
 *      in the `angle` field and `ASTRO_SUCCESS` in the `status` field.
 *      The function should always succeed, but it is a good idea for callers to check
 *      the `status` field in the returned structure.
 *      Any other value in `status` indicates a failure that should be
 *      [reported as an issue](https://github.com/cosinekitty/astronomy/issues).
 */
astro_angle_result_t Astronomy_MoonPhase(astro_time_t time)
{
    return Astronomy_LongitudeFromSun(BODY_MOON, time);
}

static astro_func_result_t moon_offset(void *context, astro_time_t time)
{
    astro_func_result_t result;
    double targetLon = *((double *)context);
    astro_angle_result_t angres = Astronomy_MoonPhase(time);
    if (angres.status != ASTRO_SUCCESS)
        return FuncError(angres.status);
    result.value = LongitudeOffset(angres.angle - targetLon);
    result.status = ASTRO_SUCCESS;
    return result;
}

/**
 * @brief
 *      Searches for the time that the Moon reaches a specified phase.
 *
 * Lunar phases are conventionally defined in terms of the Moon's geocentric ecliptic
 * longitude with respect to the Sun's geocentric ecliptic longitude.
 * When the Moon and the Sun have the same longitude, that is defined as a new moon.
 * When their longitudes are 180 degrees apart, that is defined as a full moon.
 *
 * This function searches for any value of the lunar phase expressed as an
 * angle in degrees in the range [0, 360).
 *
 * If you want to iterate through lunar quarters (new moon, first quarter, full moon, third quarter)
 * it is much easier to call the functions #Astronomy_SearchMoonQuarter and #Astronomy_NextMoonQuarter.
 * This function is useful for finding general phase angles outside those four quarters.
 *
 * @param targetLon
 *      The difference in geocentric longitude between the Sun and Moon
 *      that specifies the lunar phase being sought. This can be any value
 *      in the range [0, 360).  Certain values have conventional names:
 *      0 = new moon, 90 = first quarter, 180 = full moon, 270 = third quarter.
 *
 * @param startTime
 *      The beginning of the time window in which to search for the Moon reaching the specified phase.
 *
 * @param limitDays
 *      The number of days after `startTime` that limits the time window for the search.
 *
 * @return
 *      On success, the `status` field in the returned structure holds `ASTRO_SUCCESS` and
 *      the `time` field holds the date and time when the Moon reaches the target longitude.
 *      On failure, `status` holds some other value as an error code.
 *      One possible error code is `ASTRO_NO_MOON_QUARTER` if `startTime` and `limitDays`
 *      do not enclose the desired event. See remarks in #Astronomy_Search for other possible
 *      error codes.
 */
astro_search_result_t Astronomy_SearchMoonPhase(double targetLon, astro_time_t startTime, double limitDays)
{
    /*
        To avoid discontinuities in the moon_offset function causing problems,
        we need to approximate when that function will next return 0.
        We probe it with the start time and take advantage of the fact
        that every lunar phase repeats roughly every 29.5 days.
        There is a surprising uncertainty in the quarter timing,
        due to the eccentricity of the moon's orbit.
        I have seen up to 0.826 days away from the simple prediction.
        To be safe, we take the predicted time of the event and search
        +/-0.9 days around it (a 1.8-day wide window).
        Return ASTRO_NO_MOON_QUARTER if the final result goes beyond limitDays after startTime.
    */
    const double uncertainty = 0.9;
    astro_func_result_t funcres;
    double ya, est_dt, dt1, dt2;
    astro_time_t t1, t2;

    funcres = moon_offset(&targetLon, startTime);
    if (funcres.status != ASTRO_SUCCESS)
        return SearchError(funcres.status);

    ya = funcres.value;
    if (ya > 0.0) ya -= 360.0;  /* force searching forward in time, not backward */
    est_dt = -(MEAN_SYNODIC_MONTH * ya) / 360.0;
    dt1 = est_dt - uncertainty;
    if (dt1 > limitDays)
        return SearchError(ASTRO_NO_MOON_QUARTER);    /* not possible for moon phase to occur within specified window (too short) */
    dt2 = est_dt + uncertainty;
    if (limitDays < dt2)
        dt2 = limitDays;
    t1 = Astronomy_AddDays(startTime, dt1);
    t2 = Astronomy_AddDays(startTime, dt2);
    return Astronomy_Search(moon_offset, &targetLon, t1, t2, 1.0);
}

/**
 * @brief
 *      Finds the first lunar quarter after the specified date and time.
 *
 * A lunar quarter is one of the following four lunar phase events:
 * new moon, first quarter, full moon, third quarter.
 * This function finds the lunar quarter that happens soonest
 * after the specified date and time.
 *
 * To continue iterating through consecutive lunar quarters, call this function once,
 * followed by calls to #Astronomy_NextMoonQuarter as many times as desired.
 *
 * @param startTime
 *      The date and time at which to start the search.
 *
 * @return
 *      This function should always succeed, indicated by the `status` field
 *      in the returned structure holding `ASTRO_SUCCESS`. Any other value indicates
 *      an internal error, which should be [reported as an issue](https://github.com/cosinekitty/astronomy/issues).
 *      To be safe, calling code should always check the `status` field for errors.
 */
astro_moon_quarter_t Astronomy_SearchMoonQuarter(astro_time_t startTime)
{
    astro_moon_quarter_t mq;
    astro_angle_result_t angres;
    astro_search_result_t srchres;

    /* Determine what the next quarter phase will be. */
    angres = Astronomy_MoonPhase(startTime);
    if (angres.status != ASTRO_SUCCESS)
        return MoonQuarterError(angres.status);

    mq.quarter = (1 + (int)floor(angres.angle / 90.0)) % 4;
    srchres = Astronomy_SearchMoonPhase(90.0 * mq.quarter, startTime, 10.0);
    if (srchres.status != ASTRO_SUCCESS)
        return MoonQuarterError(srchres.status);

    mq.status = ASTRO_SUCCESS;
    mq.time = srchres.time;
    return mq;
}

/**
 * @brief
 *      Continues searching for lunar quarters from a previous search.
 *
 * After calling #Astronomy_SearchMoonQuarter, this function can be called
 * one or more times to continue finding consecutive lunar quarters.
 * This function finds the next consecutive moon quarter event after the one passed in as the parameter `mq`.
 *
 * @param mq
 *      A value returned by a prior call to #Astronomy_SearchMoonQuarter or #Astronomy_NextMoonQuarter.
 *
 * @return
 *      If `mq` is valid, this function should always succeed, indicated by the `status` field
 *      in the returned structure holding `ASTRO_SUCCESS`. Any other value indicates
 *      an internal error, which (after confirming that `mq` is valid) should be
 *      [reported as an issue](https://github.com/cosinekitty/astronomy/issues).
 *      To be safe, calling code should always check the `status` field for errors.
 */
astro_moon_quarter_t Astronomy_NextMoonQuarter(astro_moon_quarter_t mq)
{
    astro_time_t time;
    astro_moon_quarter_t next_mq;

    if (mq.status != ASTRO_SUCCESS)
        return MoonQuarterError(ASTRO_INVALID_PARAMETER);

    /* Skip 6 days past the previous found moon quarter to find the next one. */
    /* This is less than the minimum possible increment. */
    /* So far I have seen the interval well contained by the range (6.5, 8.3) days. */

    time = Astronomy_AddDays(mq.time, 6.0);
    next_mq = Astronomy_SearchMoonQuarter(time);
    if (next_mq.status == ASTRO_SUCCESS)
    {
        /* Verify that we found the expected moon quarter. */
        if (next_mq.quarter != (1 + mq.quarter) % 4)
            return MoonQuarterError(ASTRO_WRONG_MOON_QUARTER);  /* internal error! we found the wrong moon quarter */
    }
    return next_mq;
}

static astro_func_result_t rlon_offset(astro_body_t body, astro_time_t time, int direction, double targetRelLon)
{
    astro_func_result_t result;
    astro_angle_result_t plon, elon;
    double diff;

    plon = Astronomy_EclipticLongitude(body, time);
    if (plon.status != ASTRO_SUCCESS)
        return FuncError(plon.status);

    elon = Astronomy_EclipticLongitude(BODY_EARTH, time);
    if (elon.status != ASTRO_SUCCESS)
        return FuncError(elon.status);

    diff = direction * (elon.angle - plon.angle);
    result.value = LongitudeOffset(diff - targetRelLon);
    result.status = ASTRO_SUCCESS;
    return result;
}

/**
 * @brief
 *      Searches for the time when the Earth and another planet are separated by a specified angle
 *      in ecliptic longitude, as seen from the Sun.
 *
 * A relative longitude is the angle between two bodies measured in the plane of the Earth's orbit
 * (the ecliptic plane). The distance of the bodies above or below the ecliptic plane is ignored.
 * If you imagine the shadow of the body cast onto the ecliptic plane, and the angle measured around
 * that plane from one body to the other in the direction the planets orbit the Sun, you will get an
 * angle somewhere between 0 and 360 degrees. This is the relative longitude.
 *
 * Given a planet other than the Earth in `body` and a time to start the search in `startTime`,
 * this function searches for the next time that the relative longitude measured from the planet
 * to the Earth is `targetRelLon`.
 *
 * Certain astronomical events are defined in terms of relative longitude between the Earth and another planet:
 *
 * - When the relative longitude is 0 degrees, it means both planets are in the same direction from the Sun.
 *   For planets that orbit closer to the Sun (Mercury and Venus), this is known as *inferior conjunction*,
 *   a time when the other planet becomes very difficult to see because of being lost in the Sun's glare.
 *   (The only exception is in the rare event of a transit, when we see the silhouette of the planet passing
 *   between the Earth and the Sun.)
 *
 * - When the relative longitude is 0 degrees and the other planet orbits farther from the Sun,
 *   this is known as *opposition*.  Opposition is when the planet is closest to the Earth, and
 *   also when it is visible for most of the night, so it is considered the best time to observe the planet.
 *
 * - When the relative longitude is 180 degrees, it means the other planet is on the opposite side of the Sun
 *   from the Earth. This is called *superior conjunction*. Like inferior conjunction, the planet is
 *   very difficult to see from the Earth. Superior conjunction is possible for any planet other than the Earth.
 *
 * @param body
 *      A planet other than the Earth. If `body` is not a planet other than the Earth, an error occurs.
 *
 * @param targetRelLon
 *      The desired relative longitude, expressed in degrees. Must be in the range [0, 360).
 *
 * @param startTime
 *      The date and time at which to begin the search.
 *
 * @return
 *      If successful, the `status` field in the returned structure will contain `ASTRO_SUCCESS`
 *      and `time` will hold the date and time of the relative longitude event.
 *      Otherwise `status` will hold some other value that indicates an error condition.
 */
astro_search_result_t Astronomy_SearchRelativeLongitude(astro_body_t body, double targetRelLon, astro_time_t startTime)
{
    astro_search_result_t result;
    astro_func_result_t syn;
    astro_func_result_t error_angle;
    double prev_angle;
    astro_time_t time;
    int iter, direction;

    if (body == BODY_EARTH)
        return SearchError(ASTRO_EARTH_NOT_ALLOWED);

    if (body == BODY_MOON || body == BODY_SUN)
        return SearchError(ASTRO_INVALID_BODY);

    syn = SynodicPeriod(body);
    if (syn.status != ASTRO_SUCCESS)
        return SearchError(syn.status);

    direction = IsSuperiorPlanet(body) ? +1 : -1;

    /* Iterate until we converge on the desired event. */
    /* Calculate the error angle, which will be a negative number of degrees, */
    /* meaning we are "behind" the target relative longitude. */

    error_angle = rlon_offset(body, startTime, direction, targetRelLon);
    if (error_angle.status != ASTRO_SUCCESS)
        return SearchError(error_angle.status);

    if (error_angle.value > 0)
        error_angle.value -= 360;    /* force searching forward in time */

    time = startTime;
    for (iter = 0; iter < 100; ++iter)
    {
        /* Estimate how many days in the future (positive) or past (negative) */
        /* we have to go to get closer to the target relative longitude. */
        double day_adjust = (-error_angle.value/360.0) * syn.value;
        time = Astronomy_AddDays(time, day_adjust);
        if (fabs(day_adjust) * SECONDS_PER_DAY < 1.0)
        {
            result.time = time;
            result.status = ASTRO_SUCCESS;
            return result;
        }

        prev_angle = error_angle.value;
        error_angle = rlon_offset(body, time, direction, targetRelLon);
        if (error_angle.status != ASTRO_SUCCESS)
            return SearchError(error_angle.status);

        if (fabs(prev_angle) < 30.0 && (prev_angle != error_angle.value))
        {
            /* Improve convergence for Mercury/Mars (eccentric orbits) */
            /* by adjusting the synodic period to more closely match the */
            /* variable speed of both planets in this part of their respective orbits. */
            double ratio = prev_angle / (prev_angle - error_angle.value);
            if (ratio > 0.5 && ratio < 2.0)
                syn.value *= ratio;
        }
    }

    return SearchError(ASTRO_NO_CONVERGE);
}

/**
 * @brief
 *      Searches for the time when a celestial body reaches a specified hour angle as seen by an observer on the Earth.
 *
 * The *hour angle* of a celestial body indicates its position in the sky with respect
 * to the Earth's rotation. The hour angle depends on the location of the observer on the Earth.
 * The hour angle is 0 when the body reaches its highest angle above the horizon in a given day.
 * The hour angle increases by 1 unit for every sidereal hour that passes after that point, up
 * to 24 sidereal hours when it reaches the highest point again. So the hour angle indicates
 * the number of hours that have passed since the most recent time that the body has culminated,
 * or reached its highest point.
 *
 * This function searches for the next time a celestial body reaches the given hour angle
 * after the date and time specified by `startTime`.
 * To find when a body culminates, pass 0 for `hourAngle`.
 * To find when a body reaches its lowest point in the sky, pass 12 for `hourAngle`.
 *
 * Note that, especially close to the Earth's poles, a body as seen on a given day
 * may always be above the horizon or always below the horizon, so the caller cannot
 * assume that a culminating object is visible nor that an object is below the horizon
 * at its minimum altitude.
 *
 * On success, the function reports the date and time, along with the horizontal coordinates
 * of the body at that time, as seen by the given observer.
 *
 * @param body
 *      The celestial body, which can the Sun, the Moon, or any planet other than the Earth.
 *
 * @param observer
 *      Indicates a location on or near the surface of the Earth where the observer is located.
 *      Call #Astronomy_MakeObserver to create an observer structure.
 *
 * @param hourAngle
 *      An hour angle value in the range [0, 24) indicating the number of sidereal hours after the
 *      body's most recent culmination.
 *
 * @param startTime
 *      The date and time at which to start the search.
 *
 * @return
 *      If successful, the `status` field in the returned structure holds `ASTRO_SUCCESS`
 *      and the other structure fields are valid. Otherwise, `status` holds some other value
 *      that indicates an error condition.
 */
astro_hour_angle_t Astronomy_SearchHourAngle(
    astro_body_t body,
    astro_observer_t observer,
    double hourAngle,
    astro_time_t startTime)
{
    int iter = 0;
    astro_time_t time;
    astro_equatorial_t ofdate;
    astro_hour_angle_t result;
    double delta_sidereal_hours, delta_days, gast;

    if (body < MIN_BODY || body > MAX_BODY)
        return HourAngleError(ASTRO_INVALID_BODY);

    if (body == BODY_EARTH)
        return HourAngleError(ASTRO_EARTH_NOT_ALLOWED);

    if (hourAngle < 0.0 || hourAngle >= 24.0)
        return HourAngleError(ASTRO_INVALID_PARAMETER);

    time = startTime;
    for(;;)
    {
        ++iter;

        /* Calculate Greenwich Apparent Sidereal Time (GAST) at the given time. */
        gast = sidereal_time(&time);

        /* Obtain equatorial coordinates of date for the body. */
        ofdate = Astronomy_Equator(body, &time, observer, EQUATOR_OF_DATE, ABERRATION);
        if (ofdate.status != ASTRO_SUCCESS)
            return HourAngleError(ofdate.status);

        /* Calculate the adjustment needed in sidereal time */
        /* to bring the hour angle to the desired value. */

        delta_sidereal_hours = fmod((hourAngle + ofdate.ra - observer.longitude/15) - gast, 24.0);
        if (iter == 1)
        {
            /* On the first iteration, always search forward in time. */
            if (delta_sidereal_hours < 0)
                delta_sidereal_hours += 24;
        }
        else
        {
            /* On subsequent iterations, we make the smallest possible adjustment, */
            /* either forward or backward in time. */
            if (delta_sidereal_hours < -12.0)
                delta_sidereal_hours += 24.0;
            else if (delta_sidereal_hours > +12.0)
                delta_sidereal_hours -= 24.0;
        }

        /* If the error is tolerable (less than 0.1 seconds), the search has succeeded. */
        if (fabs(delta_sidereal_hours) * 3600.0 < 0.1)
        {
            result.hor = Astronomy_Horizon(&time, observer, ofdate.ra, ofdate.dec, REFRACTION_NORMAL);
            result.time = time;
            result.status = ASTRO_SUCCESS;
            return result;
        }

        /* We need to loop another time to get more accuracy. */
        /* Update the terrestrial time (in solar days) adjusting by sidereal time (sidereal hours). */
        delta_days = (delta_sidereal_hours / 24.0) * SOLAR_DAYS_PER_SIDEREAL_DAY;
        time = Astronomy_AddDays(time, delta_days);
    }
}

/** @cond DOXYGEN_SKIP */
typedef struct
{
    astro_body_t        body;
    int                 direction;
    astro_observer_t    observer;
    double              body_radius_au;
}
context_peak_altitude_t;
/** @endcond */

static astro_func_result_t peak_altitude(void *context, astro_time_t time)
{
    astro_func_result_t result;
    astro_equatorial_t ofdate;
    astro_horizon_t hor;
    const context_peak_altitude_t *p = context;

    /*
        Return the angular altitude above or below the horizon
        of the highest part (the peak) of the given object.
        This is defined as the apparent altitude of the center of the body plus
        the body's angular radius.
        The 'direction' parameter controls whether the angle is measured
        positive above the horizon or positive below the horizon,
        depending on whether the caller wants rise times or set times, respectively.
    */

    ofdate = Astronomy_Equator(p->body, &time, p->observer, EQUATOR_OF_DATE, ABERRATION);
    if (ofdate.status != ASTRO_SUCCESS)
        return FuncError(ofdate.status);

    /* We calculate altitude without refraction, then add fixed refraction near the horizon. */
    /* This gives us the time of rise/set without the extra work. */
    hor = Astronomy_Horizon(&time, p->observer, ofdate.ra, ofdate.dec, REFRACTION_NONE);
    result.value = p->direction * (hor.altitude + RAD2DEG*(p->body_radius_au / ofdate.dist) + REFRACTION_NEAR_HORIZON);
    result.status = ASTRO_SUCCESS;
    return result;
}

/**
 * @brief
 *      Searches for the next time a celestial body rises or sets as seen by an observer on the Earth.
 *
 * This function finds the next rise or set time of the Sun, Moon, or planet other than the Earth.
 * Rise time is when the body first starts to be visible above the horizon.
 * For example, sunrise is the moment that the top of the Sun first appears to peek above the horizon.
 * Set time is the moment when the body appears to vanish below the horizon.
 *
 * This function corrects for typical atmospheric refraction, which causes celestial
 * bodies to appear higher above the horizon than they would if the Earth had no atmosphere.
 * It also adjusts for the apparent angular radius of the observed body (significant only for the Sun and Moon).
 *
 * Note that rise or set may not occur in every 24 hour period.
 * For example, near the Earth's poles, there are long periods of time where
 * the Sun stays below the horizon, never rising.
 * Also, it is possible for the Moon to rise just before midnight but not set during the subsequent 24-hour day.
 * This is because the Moon sets nearly an hour later each day due to orbiting the Earth a
 * significant amount during each rotation of the Earth.
 * Therefore callers must not assume that the function will always succeed.
 *
 * @param body
 *      The Sun, Moon, or any planet other than the Earth.
 *
 * @param observer
 *      The location where observation takes place.
 *      You can create an observer structure by calling #Astronomy_MakeObserver.
 *
 * @param direction
 *      Either `DIRECTION_RISE` to find a rise time or `DIRECTION_SET` to find a set time.
 *
 * @param startTime
 *      The date and time at which to start the search.
 *
 * @param limitDays
 *      Limits how many days to search for a rise or set time.
 *      To limit a rise or set time to the same day, you can use a value of 1 day.
 *      In cases where you want to find the next rise or set time no matter how far
 *      in the future (for example, for an observer near the south pole), you can
 *      pass in a larger value like 365.
 *
 * @return
 *      On success, the `status` field in the returned structure contains `ASTRO_SUCCESS`
 *      and the `time` field contains the date and time of the rise or set time as requested.
 *      If the `status` field contains `ASTRO_SEARCH_FAILURE`, it means the rise or set
 *      event does not occur within `limitDays` days of `startTime`. This is a normal condition,
 *      not an error. Any other value of `status` indicates an error of some kind.
 */
astro_search_result_t Astronomy_SearchRiseSet(
    astro_body_t body,
    astro_observer_t observer,
    astro_direction_t direction,
    astro_time_t startTime,
    double limitDays)
{
    context_peak_altitude_t context;
    double ha_before, ha_after;
    astro_time_t time_start, time_before;
    astro_func_result_t alt_before, alt_after;
    astro_hour_angle_t evt_before, evt_after;

    if (body == BODY_EARTH)
        return SearchError(ASTRO_EARTH_NOT_ALLOWED);

    switch (direction)
    {
    case DIRECTION_RISE:
        ha_before = 12.0;   /* minimum altitude (bottom) happens BEFORE the body rises. */
        ha_after = 0.0;     /* maximum altitude (culmination) happens AFTER the body rises. */
        break;

    case DIRECTION_SET:
        ha_before = 0.0;    /* culmination happens BEFORE the body sets. */
        ha_after = 12.0;    /* bottom happens AFTER the body sets. */
        break;

    default:
        return SearchError(ASTRO_INVALID_PARAMETER);
    }

    /* Set up the context structure for the search function 'peak_altitude'. */
    context.body = body;
    context.direction = (int)direction;
    context.observer = observer;
    switch (body)
    {
    case BODY_SUN:  context.body_radius_au = SUN_RADIUS_AU;                 break;
    case BODY_MOON: context.body_radius_au = MOON_EQUATORIAL_RADIUS_AU;     break;
    default:        context.body_radius_au = 0.0;                           break;
    }

    /*
        See if the body is currently above/below the horizon.
        If we are looking for next rise time and the body is below the horizon,
        we use the current time as the lower time bound and the next culmination
        as the upper bound.
        If the body is above the horizon, we search for the next bottom and use it
        as the lower bound and the next culmination after that bottom as the upper bound.
        The same logic applies for finding set times, only we swap the hour angles.
    */

    time_start = startTime;
    alt_before = peak_altitude(&context, time_start);
    if (alt_before.status != ASTRO_SUCCESS)
        return SearchError(alt_before.status);

    if (alt_before.value > 0.0)
    {
        /* We are past the sought event, so we have to wait for the next "before" event (culm/bottom). */
        evt_before = Astronomy_SearchHourAngle(body, observer, ha_before, time_start);
        if (evt_before.status != ASTRO_SUCCESS)
            return SearchError(evt_before.status);

        time_before = evt_before.time;

        alt_before = peak_altitude(&context, time_before);
        if (alt_before.status != ASTRO_SUCCESS)
            return SearchError(alt_before.status);
    }
    else
    {
        /* We are before or at the sought event, so we find the next "after" event (bottom/culm), */
        /* and use the current time as the "before" event. */
        time_before = time_start;
    }

    evt_after = Astronomy_SearchHourAngle(body, observer, ha_after, time_before);
    if (evt_after.status != ASTRO_SUCCESS)
        return SearchError(evt_after.status);

    alt_after = peak_altitude(&context, evt_after.time);
    if (alt_after.status != ASTRO_SUCCESS)
        return SearchError(alt_after.status);

    for(;;)
    {
        if (alt_before.value <= 0.0 && alt_after.value > 0.0)
        {
            /* Search between evt_before and evt_after for the desired event. */
            astro_search_result_t result = Astronomy_Search(peak_altitude, &context, time_before, evt_after.time, 1.0);

            /* ASTRO_SEARCH_FAILURE is a special error that indicates a normal lack of finding a solution. */
            /* If successful, or any other error, return immediately. */
            if (result.status != ASTRO_SEARCH_FAILURE)
                return result;
        }

        /* If we didn't find the desired event, use evt_after.time to find the next before-event. */
        evt_before = Astronomy_SearchHourAngle(body, observer, ha_before, evt_after.time);
        if (evt_before.status != ASTRO_SUCCESS)
            return SearchError(evt_before.status);

        evt_after = Astronomy_SearchHourAngle(body, observer, ha_after, evt_before.time);
        if (evt_after.status != ASTRO_SUCCESS)
            return SearchError(evt_after.status);

        if (evt_before.time.ut >= time_start.ut + limitDays)
            return SearchError(ASTRO_SEARCH_FAILURE);

        time_before = evt_before.time;

        alt_before = peak_altitude(&context, evt_before.time);
        if (alt_before.status != ASTRO_SUCCESS)
            return SearchError(alt_before.status);

        alt_after = peak_altitude(&context, evt_after.time);
        if (alt_after.status != ASTRO_SUCCESS)
            return SearchError(alt_after.status);
    }
}

static double MoonMagnitude(double phase, double helio_dist, double geo_dist)
{
    /* https://astronomy.stackexchange.com/questions/10246/is-there-a-simple-analytical-formula-for-the-lunar-phase-brightness-curve */
    double rad = phase * DEG2RAD;
    double rad2 = rad * rad;
    double rad4 = rad2 * rad2;
    double mag = -12.717 + 1.49*fabs(rad) + 0.0431*rad4;
    double moon_mean_distance_au = 385000.6 / KM_PER_AU;
    double geo_au = geo_dist / moon_mean_distance_au;
    mag += 5*log10(helio_dist * geo_au);
    return mag;
}

static astro_status_t SaturnMagnitude(
    double phase,
    double helio_dist,
    double geo_dist,
    astro_vector_t gc,
    astro_time_t time,
    double *mag,
    double *ring_tilt)
{
    astro_ecliptic_t eclip;
    double ir, Nr, lat, lon, tilt, sin_tilt;

    *mag = *ring_tilt = NAN;

    /* Based on formulas by Paul Schlyter found here: */
    /* http://www.stjarnhimlen.se/comp/ppcomp.html#15 */

    /* We must handle Saturn's rings as a major component of its visual magnitude. */
    /* Find geocentric ecliptic coordinates of Saturn. */
    eclip = Astronomy_Ecliptic(gc);
    if (eclip.status != ASTRO_SUCCESS)
        return eclip.status;

    ir = DEG2RAD * 28.06;   /* tilt of Saturn's rings to the ecliptic, in radians */
    Nr = DEG2RAD * (169.51 + (3.82e-5 * time.tt));    /* ascending node of Saturn's rings, in radians */

    /* Find tilt of Saturn's rings, as seen from Earth. */
    lat = DEG2RAD * eclip.elat;
    lon = DEG2RAD * eclip.elon;
    tilt = asin(sin(lat)*cos(ir) - cos(lat)*sin(ir)*sin(lon-Nr));
    sin_tilt = sin(fabs(tilt));

    *mag = -9.0 + 0.044*phase;
    *mag += sin_tilt*(-2.6 + 1.2*sin_tilt);
    *mag += 5.0 * log10(helio_dist * geo_dist);

    *ring_tilt = RAD2DEG * tilt;

    return ASTRO_SUCCESS;
}

static astro_status_t VisualMagnitude(
    astro_body_t body,
    double phase,
    double helio_dist,
    double geo_dist,
    double *mag)
{
    /* For Mercury and Venus, see:  https://iopscience.iop.org/article/10.1086/430212 */
    double c0, c1=0, c2=0, c3=0, x;
    *mag = NAN;
    switch (body)
    {
    case BODY_MERCURY:  c0 = -0.60, c1 = +4.98, c2 = -4.88, c3 = +3.02; break;
    case BODY_VENUS:
        if (phase < 163.6)
            c0 = -4.47, c1 = +1.03, c2 = +0.57, c3 = +0.13;
        else
            c0 = 0.98, c1 = -1.02;
        break;
    case BODY_MARS:        c0 = -1.52, c1 = +1.60;   break;
    case BODY_JUPITER:     c0 = -9.40, c1 = +0.50;   break;
    case BODY_URANUS:      c0 = -7.19, c1 = +0.25;   break;
    case BODY_NEPTUNE:     c0 = -6.87;               break;
    case BODY_PLUTO:       c0 = -1.00, c1 = +4.00;   break;
    default: return ASTRO_INVALID_BODY;
    }

    x = phase / 100;
    *mag = c0 + x*(c1 + x*(c2 + x*c3));
    *mag += 5.0 * log10(helio_dist * geo_dist);
    return ASTRO_SUCCESS;
}

/**
 * @brief
 *      Finds visual magnitude, phase angle, and other illumination information about a celestial body.
 *
 * This function calculates information about how bright a celestial body appears from the Earth,
 * reported as visual magnitude, which is a smaller (or even negative) number for brighter objects
 * and a larger number for dimmer objects.
 *
 * For bodies other than the Sun, it reports a phase angle, which is the angle in degrees between
 * the Sun and the Earth, as seen from the center of the body. Phase angle indicates what fraction
 * of the body appears illuminated as seen from the Earth. For example, when the phase angle is
 * near zero, it means the body appears "full" as seen from the Earth.  A phase angle approaching
 * 180 degrees means the body appears as a thin crescent as seen from the Earth.  A phase angle
 * of 90 degrees means the body appears "half full".
 * For the Sun, the phase angle is always reported as 0; the Sun emits light rather than reflecting it,
 * so it doesn't have a phase angle.
 *
 * When the body is Saturn, the returned structure contains a field `ring_tilt` that holds
 * the tilt angle in degrees of Saturn's rings as seen from the Earth. A value of 0 means
 * the rings appear edge-on, and are thus nearly invisible from the Earth. The `ring_tilt` holds
 * 0 for all bodies other than Saturn.
 *
 * @param body
 *      The Sun, Moon, or any planet other than the Earth.
 *
 * @param time
 *      The date and time of the observation.
 *
 * @return
 *      On success, the `status` field of the return structure holds `ASTRO_SUCCESS`
 *      and the other structure fields are valid.
 *      Any other value indicates an error, in which case the remaining structure fields are not valid.
 */
astro_illum_t Astronomy_Illumination(astro_body_t body, astro_time_t time)
{
    astro_vector_t earth;   /* vector from Sun to Earth */
    astro_vector_t hc;      /* vector from Sun to body */
    astro_vector_t gc;      /* vector from Earth to body */
    double mag;             /* visual magnitude */
    astro_angle_result_t phase;     /* phase angle in degrees between Earth and Sun as seen from body */
    double helio_dist;      /* distance from Sun to body */
    double geo_dist;        /* distance from Earth to body */
    double ring_tilt = 0.0; /* Saturn's ring tilt (0 for all other bodies) */
    astro_illum_t illum;
    astro_status_t status;

    if (body == BODY_EARTH)
        return IllumError(ASTRO_EARTH_NOT_ALLOWED);

    earth = CalcEarth(time);
    if (earth.status != ASTRO_SUCCESS)
        return IllumError(earth.status);

    if (body == BODY_SUN)
    {
        gc.status = ASTRO_SUCCESS;
        gc.t = time;
        gc.x = -earth.x;
        gc.y = -earth.y;
        gc.z = -earth.z;

        hc.status = ASTRO_SUCCESS;
        hc.t = time;
        hc.x = 0.0;
        hc.y = 0.0;
        hc.z = 0.0;

        /* The Sun emits light instead of reflecting it, */
        /* so we report a placeholder phase angle of 0. */
        phase.status = ASTRO_SUCCESS;
        phase.angle = 0.0;
    }
    else
    {
        if (body == BODY_MOON)
        {
            /* For extra numeric precision, use geocentric Moon formula directly. */
            gc = Astronomy_GeoMoon(time);
            if (gc.status != ASTRO_SUCCESS)
                return IllumError(gc.status);

            hc.status = ASTRO_SUCCESS;
            hc.t = time;
            hc.x = earth.x + gc.x;
            hc.y = earth.y + gc.y;
            hc.z = earth.z + gc.z;
        }
        else
        {
            /* For planets, the heliocentric vector is more direct to calculate. */
            hc = Astronomy_HelioVector(body, time);
            if (hc.status != ASTRO_SUCCESS)
                return IllumError(hc.status);

            gc.status = ASTRO_SUCCESS;
            gc.t = time;
            gc.x = hc.x - earth.x;
            gc.y = hc.y - earth.y;
            gc.z = hc.z - earth.z;
        }

        phase = AngleBetween(gc, hc);
        if (phase.status != ASTRO_SUCCESS)
            return IllumError(phase.status);
    }

    geo_dist = Astronomy_VectorLength(gc);
    helio_dist = Astronomy_VectorLength(hc);

    switch (body)
    {
    case BODY_SUN:
        mag = -0.17 + 5.0*log10(geo_dist / AU_PER_PARSEC);
        break;

    case BODY_MOON:
        mag = MoonMagnitude(phase.angle, helio_dist, geo_dist);
        break;

    case BODY_SATURN:
        status = SaturnMagnitude(phase.angle, helio_dist, geo_dist, gc, time, &mag, &ring_tilt);
        if (status != ASTRO_SUCCESS)
            return IllumError(status);
        break;

    default:
        status = VisualMagnitude(body, phase.angle, helio_dist, geo_dist, &mag);
        break;
    }

    illum.status = ASTRO_SUCCESS;
    illum.time = time;
    illum.mag = mag;
    illum.phase_angle = phase.angle;
    illum.helio_dist = helio_dist;
    illum.ring_tilt = ring_tilt;

    return illum;
}

static astro_func_result_t mag_slope(void *context, astro_time_t time)
{
    /*
        The Search() function finds a transition from negative to positive values.
        The derivative of magnitude y with respect to time t (dy/dt)
        is negative as an object gets brighter, because the magnitude numbers
        get smaller. At peak magnitude dy/dt = 0, then as the object gets dimmer,
        dy/dt > 0.
    */
    static const double dt = 0.01;
    astro_illum_t y1, y2;
    astro_body_t body = *((astro_body_t *)context);
    astro_time_t t1 = Astronomy_AddDays(time, -dt/2);
    astro_time_t t2 = Astronomy_AddDays(time, +dt/2);
    astro_func_result_t result;

    y1 = Astronomy_Illumination(body, t1);
    if (y1.status != ASTRO_SUCCESS)
        return FuncError(y1.status);

    y2 = Astronomy_Illumination(body, t2);
    if (y2.status != ASTRO_SUCCESS)
        return FuncError(y2.status);

    result.value = (y2.mag - y1.mag) / dt;
    result.status = ASTRO_SUCCESS;
    return result;
}

/**
 * @brief
 *      Searches for the date and time Venus will next appear brightest as seen from the Earth.
 *
 * This function searches for the date and time Venus appears brightest as seen from the Earth.
 * Currently only Venus is supported for the `body` parameter, though this could change in the future.
 * Mercury's peak magnitude occurs at superior conjunction, when it is virtually impossible to see from the Earth,
 * so peak magnitude events have little practical value for that planet.
 * Planets other than Venus and Mercury reach peak magnitude at opposition, which can
 * be found using #Astronomy_SearchRelativeLongitude.
 * The Moon reaches peak magnitude at full moon, which can be found using
 * #Astronomy_SearchMoonQuarter or #Astronomy_SearchMoonPhase.
 * The Sun reaches peak magnitude at perihelion, which occurs each year in January.
 * However, the difference is minor and has little practical value.
 *
 * @param body
 *      Currently only `BODY_VENUS` is allowed. Any other value results in the error `ASTRO_INVALID_BODY`.
 *      See function remarks for more details.
 *
 * @param startTime
 *      The date and time to start searching for the next peak magnitude event.
 *
 * @return
 *      See documentation about the return value from #Astronomy_Illumination.
 */
astro_illum_t Astronomy_SearchPeakMagnitude(astro_body_t body, astro_time_t startTime)
{
    /* s1 and s2 are relative longitudes within which peak magnitude of Venus can occur. */
    static const double s1 = 10.0;
    static const double s2 = 30.0;
    int iter;
    astro_angle_result_t plon, elon;
    astro_search_result_t t1, t2, tx;
    astro_func_result_t syn, m1, m2;
    astro_time_t t_start;
    double rlon, rlon_lo, rlon_hi, adjust_days;

    if (body != BODY_VENUS)
        return IllumError(ASTRO_INVALID_BODY);

    iter = 0;
    while (++iter <= 2)
    {
        /* Find current heliocentric relative longitude between the */
        /* inferior planet and the Earth. */
        plon = Astronomy_EclipticLongitude(body, startTime);
        if (plon.status != ASTRO_SUCCESS)
            return IllumError(plon.status);

        elon = Astronomy_EclipticLongitude(BODY_EARTH, startTime);
        if (elon.status != ASTRO_SUCCESS)
            return IllumError(elon.status);

        rlon = LongitudeOffset(plon.angle - elon.angle);    /* clamp to (-180, +180]. */

        /* The slope function is not well-behaved when rlon is near 0 degrees or 180 degrees */
        /* because there is a cusp there that causes a discontinuity in the derivative. */
        /* So we need to guard against searching near such times. */

        if (rlon >= -s1 && rlon < +s1)
        {
            /* Seek to the window [+s1, +s2]. */
            adjust_days = 0.0;
            /* Search forward for the time t1 when rel lon = +s1. */
            rlon_lo = +s1;
            /* Search forward for the time t2 when rel lon = +s2. */
            rlon_hi = +s2;
        }
        else if (rlon >= +s2 || rlon < -s2)
        {
            /* Seek to the next search window at [-s2, -s1]. */
            adjust_days = 0.0;
            /* Search forward for the time t1 when rel lon = -s2. */
            rlon_lo = -s2;
            /* Search forward for the time t2 when rel lon = -s1. */
            rlon_hi = -s1;
        }
        else if (rlon >= 0)
        {
            /* rlon must be in the middle of the window [+s1, +s2]. */
            /* Search BACKWARD for the time t1 when rel lon = +s1. */
            syn = SynodicPeriod(body);
            if (syn.status != ASTRO_SUCCESS)
                return IllumError(syn.status);
            adjust_days = -syn.value / 4;
            rlon_lo = +s1;
            /* Search forward from t1 to find t2 such that rel lon = +s2. */
            rlon_hi = +s2;
        }
        else
        {
            /* rlon must be in the middle of the window [-s2, -s1]. */
            /* Search BACKWARD for the time t1 when rel lon = -s2. */
            syn = SynodicPeriod(body);
            if (syn.status != ASTRO_SUCCESS)
                return IllumError(syn.status);
            adjust_days = -syn.value / 4;
            rlon_lo = -s2;
            /* Search forward from t1 to find t2 such that rel lon = -s1. */
            rlon_hi = -s1;
        }
        t_start = Astronomy_AddDays(startTime, adjust_days);
        t1 = Astronomy_SearchRelativeLongitude(body, rlon_lo, t_start);
        if (t1.status != ASTRO_SUCCESS)
            return IllumError(t1.status);
        t2 = Astronomy_SearchRelativeLongitude(body, rlon_hi, t1.time);
        if (t2.status != ASTRO_SUCCESS)
            return IllumError(t2.status);

        /* Now we have a time range [t1,t2] that brackets a maximum magnitude event. */
        /* Confirm the bracketing. */
        m1 = mag_slope(&body, t1.time);
        if (m1.status != ASTRO_SUCCESS)
            return IllumError(m1.status);
        if (m1.value >= 0.0)
            return IllumError(ASTRO_INTERNAL_ERROR);    /* should never happen! */

        m2 = mag_slope(&body, t2.time);
        if (m2.status != ASTRO_SUCCESS)
            return IllumError(m2.status);
        if (m2.value <= 0.0)
            return IllumError(ASTRO_INTERNAL_ERROR);    /* should never happen! */

        /* Use the generic search algorithm to home in on where the slope crosses from negative to positive. */
        tx = Astronomy_Search(mag_slope, &body, t1.time, t2.time, 10.0);
        if (tx.status != ASTRO_SUCCESS)
            return IllumError(tx.status);

        if (tx.time.tt >= startTime.tt)
            return Astronomy_Illumination(body, tx.time);

        /* This event is in the past (earlier than startTime). */
        /* We need to search forward from t2 to find the next possible window. */
        /* We never need to search more than twice. */
        startTime = Astronomy_AddDays(t2.time, 1.0);
    }

    return IllumError(ASTRO_SEARCH_FAILURE);
}

static double MoonDistance(astro_time_t t)
{
    double lon, lat, dist;
    CalcMoon(t.tt / 36525.0, &lon, &lat, &dist);
    return dist;
}

static astro_func_result_t moon_distance_slope(void *context, astro_time_t time)
{
    static const double dt = 0.001;
    astro_time_t t1 = Astronomy_AddDays(time, -dt/2.0);
    astro_time_t t2 = Astronomy_AddDays(time, +dt/2.0);
    double dist1, dist2;
    int direction = *((int *)context);
    astro_func_result_t result;

    dist1 = MoonDistance(t1);
    dist2 = MoonDistance(t2);
    result.value = direction * (dist2 - dist1) / dt;
    result.status = ASTRO_SUCCESS;
    return result;
}

/**
 * @brief
 *      Finds the date and time of the Moon's closest distance (perigee)
 *      or farthest distance (apogee) with respect to the Earth.
 *
 * Given a date and time to start the search in `startTime`, this function finds the
 * next date and time that the center of the Moon reaches the closest or farthest point
 * in its orbit with respect to the center of the Earth, whichever comes first
 * after `startTime`.
 *
 * The closest point is called *perigee* and the farthest point is called *apogee*.
 * The word *apsis* refers to either event.
 *
 * To iterate through consecutive alternating perigee and apogee events, call `Astronomy_SearchLunarApsis`
 * once, then use the return value to call #Astronomy_NextLunarApsis. After that,
 * keep feeding the previous return value from `Astronomy_NextLunarApsis` into another
 * call of `Astronomy_NextLunarApsis` as many times as desired.
 *
 * @param startTime
 *      The date and time at which to start searching for the next perigee or apogee.
 *
 * @return
 *      If successful, the `status` field in the returned structure holds `ASTRO_SUCCESS`,
 *      `time` holds the date and time of the next lunar apsis, `kind` holds either
 *      `APSIS_PERICENTER` for perigee or `APSIS_APOCENTER` for apogee, and the distance
 *      values `dist_au` (astronomical units) and `dist_km` (kilometers) are valid.
 *      If the function fails, `status` holds some value other than `ASTRO_SUCCESS` that
 *      indicates what went wrong, and the other structure fields are invalid.
 */
astro_apsis_t Astronomy_SearchLunarApsis(astro_time_t startTime)
{
    astro_time_t t1, t2;
    astro_search_result_t search;
    astro_func_result_t m1, m2;
    int positive_direction = +1;
    int negative_direction = -1;
    const double increment = 5.0;   /* number of days to skip in each iteration */
    astro_apsis_t result;
    int iter;

    /*
        Check the rate of change of the distance dr/dt at the start time.
        If it is positive, the Moon is currently getting farther away,
        so start looking for apogee.
        Conversely, if dr/dt < 0, start looking for perigee.
        Either way, the polarity of the slope will change, so the product will be negative.
        Handle the crazy corner case of exactly touching zero by checking for m1*m2 <= 0.
    */

    t1 = startTime;
    m1 = moon_distance_slope(&positive_direction, t1);
    if (m1.status != ASTRO_SUCCESS)
        return ApsisError(m1.status);

    for (iter=0; iter * increment < 2.0 * MEAN_SYNODIC_MONTH; ++iter)
    {
        t2 = Astronomy_AddDays(t1, increment);
        m2 = moon_distance_slope(&positive_direction, t2);
        if (m2.status != ASTRO_SUCCESS)
            return ApsisError(m2.status);

        if (m1.value * m2.value <= 0.0)
        {
            /* There is a change of slope polarity within the time range [t1, t2]. */
            /* Therefore this time range contains an apsis. */
            /* Figure out whether it is perigee or apogee. */

            if (m1.value < 0.0 || m2.value > 0.0)
            {
                /* We found a minimum-distance event: perigee. */
                /* Search the time range for the time when the slope goes from negative to positive. */
                search = Astronomy_Search(moon_distance_slope, &positive_direction, t1, t2, 1.0);
                result.kind = APSIS_PERICENTER;
            }
            else if (m1.value > 0.0 || m2.value < 0.0)
            {
                /* We found a maximum-distance event: apogee. */
                /* Search the time range for the time when the slope goes from positive to negative. */
                search = Astronomy_Search(moon_distance_slope, &negative_direction, t1, t2, 1.0);
                result.kind = APSIS_APOCENTER;
            }
            else
            {
                /* This should never happen. It should not be possible for both slopes to be zero. */
                return ApsisError(ASTRO_INTERNAL_ERROR);
            }

            if (search.status != ASTRO_SUCCESS)
                return ApsisError(search.status);

            result.status = ASTRO_SUCCESS;
            result.time = search.time;
            result.dist_au = MoonDistance(search.time);
            result.dist_km = result.dist_au * KM_PER_AU;
            return result;
        }

        /* We have not yet found a slope polarity change. Keep searching. */
        t1 = t2;
        m1 = m2;
    }

    /* It should not be possible to fail to find an apsis within 2 synodic months. */
    return ApsisError(ASTRO_INTERNAL_ERROR);
}

/**
 * @brief
 *      Finds the next lunar perigee or apogee event in a series.
 *
 * This function requires an #astro_apsis_t value obtained from a call
 * to #Astronomy_SearchLunarApsis or `Astronomy_NextLunarApsis`. Given
 * an apogee event, this function finds the next perigee event, and vice versa.
 *
 * See #Astronomy_SearchLunarApsis for more details.
 *
 * @param apsis
 *      An apsis event obtained from a call to #Astronomy_SearchLunarApsis or `Astronomy_NextLunarApsis`.
 *      See #Astronomy_SearchLunarApsis for more details.
 *
 * @return
 *      Same as the return value for #Astronomy_SearchLunarApsis.
 */
astro_apsis_t Astronomy_NextLunarApsis(astro_apsis_t apsis)
{
    static const double skip = 11.0;    /* number of days to skip to start looking for next apsis event */
    astro_apsis_t next;
    astro_time_t time;

    if (apsis.status != ASTRO_SUCCESS)
        return ApsisError(ASTRO_INVALID_PARAMETER);

    if (apsis.kind != APSIS_APOCENTER && apsis.kind != APSIS_PERICENTER)
        return ApsisError(ASTRO_INVALID_PARAMETER);

    time = Astronomy_AddDays(apsis.time, skip);
    next = Astronomy_SearchLunarApsis(time);
    if (next.status == ASTRO_SUCCESS)
    {
        /* Verify that we found the opposite apsis from the previous one. */
        if (next.kind + apsis.kind != 1)
            return ApsisError(ASTRO_INTERNAL_ERROR);
    }
    return next;
}


/** @cond DOXYGEN_SKIP */
typedef struct
{
    int direction;
    astro_body_t body;
}
planet_distance_context_t;
/** @endcond */


static astro_func_result_t planet_distance_slope(void *context, astro_time_t time)
{
    static const double dt = 0.001;
    const planet_distance_context_t *pc = context;
    astro_time_t t1 = Astronomy_AddDays(time, -dt/2.0);
    astro_time_t t2 = Astronomy_AddDays(time, +dt/2.0);
    astro_func_result_t dist1, dist2, result;

    dist1 = Astronomy_HelioDistance(pc->body, t1);
    if (dist1.status != ASTRO_SUCCESS)
        return dist1;

    dist2 = Astronomy_HelioDistance(pc->body, t2);
    if (dist2.status != ASTRO_SUCCESS)
        return dist2;

    result.value = pc->direction * (dist2.value - dist1.value) / dt;
    result.status = ASTRO_SUCCESS;
    return result;
}

static astro_apsis_t PlanetExtreme(
    astro_body_t body,
    astro_apsis_kind_t kind,
    astro_time_t start_time,
    double dayspan)
{
    astro_apsis_t apsis;
    const double direction = (kind == APSIS_APOCENTER) ? +1.0 : -1.0;
    const int npoints = 10;
    int i, best_i;
    double interval;
    double dist, best_dist;
    astro_time_t time;
    astro_func_result_t result;

    for(;;)
    {
        interval = dayspan / (npoints - 1);

        if (interval < 1.0 / 1440.0)    /* iterate until uncertainty is less than one minute */
        {
            apsis.status = ASTRO_SUCCESS;
            apsis.kind = kind;
            apsis.time = Astronomy_AddDays(start_time, interval / 2.0);
            result = Astronomy_HelioDistance(body, apsis.time);
            if (result.status != ASTRO_SUCCESS)
                return ApsisError(result.status);
            apsis.dist_au = result.value;
            apsis.dist_km = apsis.dist_au * KM_PER_AU;
            return apsis;
        }

        best_i = -1;
        best_dist = 0.0;
        for (i=0; i < npoints; ++i)
        {
            time = Astronomy_AddDays(start_time, i * interval);
            result = Astronomy_HelioDistance(body, time);
            if (result.status != ASTRO_SUCCESS)
                return ApsisError(result.status);
            dist = direction * result.value;
            if (i==0 || dist > best_dist)
            {
                best_i = i;
                best_dist = dist;
            }
        }

        /* Narrow in on the extreme point. */
        start_time = Astronomy_AddDays(start_time, (best_i - 1) * interval);
        dayspan = 2.0 * interval;
    }
}


static astro_apsis_t BruteSearchPlanetApsis(astro_body_t body, astro_time_t startTime)
{
    const int npoints = 100;
    int i;
    astro_time_t t1, t2, time, t_min, t_max;
    double dist, max_dist, min_dist;
    astro_apsis_t perihelion, aphelion;
    double interval;
    double period;
    astro_func_result_t result;

    /*
        Neptune is a special case for two reasons:
        1. Its orbit is nearly circular (low orbital eccentricity).
        2. It is so distant from the Sun that the orbital period is very long.
        Put together, this causes wobbling of the Sun around the Solar System Barycenter (SSB)
        to be so significant that there are 3 local minima in the distance-vs-time curve
        near each apsis. Therefore, unlike for other planets, we can't use an optimized
        algorithm for finding dr/dt = 0.
        Instead, we use a dumb, brute-force algorithm of sampling and finding min/max
        heliocentric distance.

        There is a similar problem in the TOP2013 model for Pluto:
        Its position vector has high-frequency oscillations that confuse the
        slope-based determination of apsides.
    */

    /*
        Rewind approximately 30 degrees in the orbit,
        then search forward for 270 degrees.
        This is a very cautious way to prevent missing an apsis.
        Typically we will find two apsides, and we pick whichever
        apsis is ealier, but after startTime.
        Sample points around this orbital arc and find when the distance
        is greatest and smallest.
    */
    period = PlanetOrbitalPeriod(body);
    t1 = Astronomy_AddDays(startTime, period * ( -30.0 / 360.0));
    t2 = Astronomy_AddDays(startTime, period * (+270.0 / 360.0));
    t_min = t_max = t1;
    min_dist = max_dist = -1.0;     /* prevent warning about uninitialized variables */
    interval = (t2.ut - t1.ut) / (npoints - 1.0);

    for (i=0; i < npoints; ++i)
    {
        double ut = t1.ut + (i * interval);
        time = Astronomy_TimeFromDays(ut);
        result = Astronomy_HelioDistance(body, time);
        if (result.status != ASTRO_SUCCESS)
            return ApsisError(result.status);
        dist = result.value;
        if (i == 0)
        {
            max_dist = min_dist = dist;
        }
        else
        {
            if (dist > max_dist)
            {
                max_dist = dist;
                t_max = time;
            }
            if (dist < min_dist)
            {
                min_dist = dist;
                t_min = time;
            }
        }
    }

    t1 = Astronomy_AddDays(t_min, -2 * interval);
    perihelion = PlanetExtreme(body, APSIS_PERICENTER, t1, 4 * interval);

    t1 = Astronomy_AddDays(t_max, -2 * interval);
    aphelion = PlanetExtreme(body, APSIS_APOCENTER, t1, 4 * interval);

    if (perihelion.status == ASTRO_SUCCESS && perihelion.time.tt >= startTime.tt)
    {
        if (aphelion.status == ASTRO_SUCCESS && aphelion.time.tt >= startTime.tt)
        {
            /* Perihelion and aphelion are both valid. Pick the one that comes first. */
            if (aphelion.time.tt < perihelion.time.tt)
                return aphelion;
        }
        return perihelion;
    }

    if (aphelion.status == ASTRO_SUCCESS && aphelion.time.tt >= startTime.tt)
        return aphelion;

    return ApsisError(ASTRO_FAIL_APSIS);
}


/**
 * @brief
 *      Finds the date and time of a planet's perihelion (closest approach to the Sun)
 *      or aphelion (farthest distance from the Sun) after a given time.
 *
 * Given a date and time to start the search in `startTime`, this function finds the
 * next date and time that the center of the specified planet reaches the closest or farthest point
 * in its orbit with respect to the center of the Sun, whichever comes first
 * after `startTime`.
 *
 * The closest point is called *perihelion* and the farthest point is called *aphelion*.
 * The word *apsis* refers to either event.
 *
 * To iterate through consecutive alternating perihelion and aphelion events,
 * call `Astronomy_SearchPlanetApsis` once, then use the return value to call
 * #Astronomy_NextPlanetApsis. After that, keep feeding the previous return value
 * from `Astronomy_NextPlanetApsis` into another call of `Astronomy_NextPlanetApsis`
 * as many times as desired.
 *
 * @param body
 *      The planet for which to find the next perihelion/aphelion event.
 *      Not allowed to be `BODY_SUN` or `BODY_MOON`.
 *
 * @param startTime
 *      The date and time at which to start searching for the next perihelion or aphelion.
 *
 * @return
 *      If successful, the `status` field in the returned structure holds `ASTRO_SUCCESS`,
 *      `time` holds the date and time of the next planetary apsis, `kind` holds either
 *      `APSIS_PERICENTER` for perihelion or `APSIS_APOCENTER` for aphelion, and the distance
 *      values `dist_au` (astronomical units) and `dist_km` (kilometers) are valid.
 *      If the function fails, `status` holds some value other than `ASTRO_SUCCESS` that
 *      indicates what went wrong, and the other structure fields are invalid.
 */
astro_apsis_t Astronomy_SearchPlanetApsis(astro_body_t body, astro_time_t startTime)
{
    astro_time_t t1, t2;
    astro_search_result_t search;
    astro_func_result_t m1, m2;
    planet_distance_context_t context;
    astro_apsis_t result;
    int iter;
    double orbit_period_days;
    double increment;   /* number of days to skip in each iteration */
    astro_func_result_t dist;

    if (body == BODY_NEPTUNE || body == BODY_PLUTO)
        return BruteSearchPlanetApsis(body, startTime);

    orbit_period_days = PlanetOrbitalPeriod(body);
    if (orbit_period_days == 0.0)
        return ApsisError(ASTRO_INVALID_BODY);      /* The body must be a planet. */

    increment = orbit_period_days / 6.0;

    context.body = body;

    t1 = startTime;
    context.direction = +1;
    m1 = planet_distance_slope(&context, t1);
    if (m1.status != ASTRO_SUCCESS)
        return ApsisError(m1.status);

    for (iter=0; iter * increment < 2.0 * orbit_period_days; ++iter)
    {
        t2 = Astronomy_AddDays(t1, increment);
        context.direction = +1;
        m2 = planet_distance_slope(&context, t2);
        if (m2.status != ASTRO_SUCCESS)
            return ApsisError(m2.status);

        if (m1.value * m2.value <= 0.0)
        {
            /* There is a change of slope polarity within the time range [t1, t2]. */
            /* Therefore this time range contains an apsis. */
            /* Figure out whether it is perihelion or aphelion. */

            if (m1.value < 0.0 || m2.value > 0.0)
            {
                /* We found a minimum-distance event: perihelion. */
                /* Search the time range for the time when the slope goes from negative to positive. */
                context.direction = +1;
                result.kind = APSIS_PERICENTER;
            }
            else if (m1.value > 0.0 || m2.value < 0.0)
            {
                /* We found a maximum-distance event: aphelion. */
                /* Search the time range for the time when the slope goes from positive to negative. */
                context.direction = -1;
                result.kind = APSIS_APOCENTER;
            }
            else
            {
                /* This should never happen. It should not be possible for both slopes to be zero. */
                return ApsisError(ASTRO_INTERNAL_ERROR);
            }

            search = Astronomy_Search(planet_distance_slope, &context, t1, t2, 1.0);
            if (search.status != ASTRO_SUCCESS)
                return ApsisError(search.status);

            dist = Astronomy_HelioDistance(body, search.time);
            if (dist.status != ASTRO_SUCCESS)
                return ApsisError(dist.status);

            result.status = ASTRO_SUCCESS;
            result.time = search.time;
            result.dist_au = dist.value;
            result.dist_km = dist.value * KM_PER_AU;
            return result;
        }

        /* We have not yet found a slope polarity change. Keep searching. */
        t1 = t2;
        m1 = m2;
    }

    /* It should not be possible to fail to find an apsis within 2 orbits. */
    return ApsisError(ASTRO_INTERNAL_ERROR);
}

/**
 * @brief
 *      Finds the next planetary perihelion or aphelion event in a series.
 *
 * This function requires an #astro_apsis_t value obtained from a call
 * to #Astronomy_SearchPlanetApsis or `Astronomy_NextPlanetApsis`.
 * Given an aphelion event, this function finds the next perihelion event, and vice versa.
 *
 * See #Astronomy_SearchPlanetApsis for more details.
 *
 * @param body
 *      The planet for which to find the next perihelion/aphelion event.
 *      Not allowed to be `BODY_SUN` or `BODY_MOON`.
 *      Must match the body passed into the call that produced the `apsis` parameter.
 *
 * @param apsis
 *      An apsis event obtained from a call to #Astronomy_SearchPlanetApsis or `Astronomy_NextPlanetApsis`.
 *
 * @return
 *      Same as the return value for #Astronomy_SearchPlanetApsis.
 */
astro_apsis_t Astronomy_NextPlanetApsis(astro_body_t body, astro_apsis_t apsis)
{
    double skip;    /* number of days to skip to start looking for next apsis event */
    astro_apsis_t next;
    astro_time_t time;

    if (apsis.status != ASTRO_SUCCESS)
        return ApsisError(ASTRO_INVALID_PARAMETER);

    if (apsis.kind != APSIS_APOCENTER && apsis.kind != APSIS_PERICENTER)
        return ApsisError(ASTRO_INVALID_PARAMETER);

    skip = 0.25 * PlanetOrbitalPeriod(body);        /* skip 1/4 of an orbit before starting search again */
    if (skip <= 0.0)
        return ApsisError(ASTRO_INVALID_BODY);      /* body must be a planet */

    time = Astronomy_AddDays(apsis.time, skip);
    next = Astronomy_SearchPlanetApsis(body, time);
    if (next.status == ASTRO_SUCCESS)
    {
        /* Verify that we found the opposite apsis from the previous one. */
        if (next.kind + apsis.kind != 1)
            return ApsisError(ASTRO_INTERNAL_ERROR);
    }
    return next;
}


/**
 * @brief Calculates the inverse of a rotation matrix.
 *
 * Given a rotation matrix that performs some coordinate transform,
 * this function returns the matrix that reverses that trasnform.
 *
 * @param rotation
 *      The rotation matrix to be inverted.
 *
 * @return
 *      A rotation matrix that performs the opposite transformation.
 */
astro_rotation_t Astronomy_InverseRotation(astro_rotation_t rotation)
{
    astro_rotation_t inverse;

    if (rotation.status != ASTRO_SUCCESS)
        return RotationErr(ASTRO_INVALID_PARAMETER);

    inverse.status = ASTRO_SUCCESS;
    inverse.rot[0][0] = rotation.rot[0][0];
    inverse.rot[0][1] = rotation.rot[1][0];
    inverse.rot[0][2] = rotation.rot[2][0];
    inverse.rot[1][0] = rotation.rot[0][1];
    inverse.rot[1][1] = rotation.rot[1][1];
    inverse.rot[1][2] = rotation.rot[2][1];
    inverse.rot[2][0] = rotation.rot[0][2];
    inverse.rot[2][1] = rotation.rot[1][2];
    inverse.rot[2][2] = rotation.rot[2][2];

    return inverse;
}

/**
 * @brief Creates a rotation based on applying one rotation followed by another.
 *
 * Given two rotation matrices, returns a combined rotation matrix that is
 * equivalent to rotating based on the first matrix, followed by the second.
 *
 * @param a
 *      The first rotation to apply.
 *
 * @param b
 *      The second rotation to apply.
 *
 * @return
 *      The combined rotation matrix.
 */
astro_rotation_t Astronomy_CombineRotation(astro_rotation_t a, astro_rotation_t b)
{
    astro_rotation_t c;

    if (a.status != ASTRO_SUCCESS || b.status != ASTRO_SUCCESS)
        return RotationErr(ASTRO_INVALID_PARAMETER);

    /*
        Use matrix multiplication: c = b*a.
        We put 'b' on the left and 'a' on the right because,
        just like when you use a matrix M to rotate a vector V,
        you put the M on the left in the product M*V.
        We can think of this as 'b' rotating all the 3 column vectors in 'a'.
    */
    c.rot[0][0] = b.rot[0][0]*a.rot[0][0] + b.rot[1][0]*a.rot[0][1] + b.rot[2][0]*a.rot[0][2];
    c.rot[1][0] = b.rot[0][0]*a.rot[1][0] + b.rot[1][0]*a.rot[1][1] + b.rot[2][0]*a.rot[1][2];
    c.rot[2][0] = b.rot[0][0]*a.rot[2][0] + b.rot[1][0]*a.rot[2][1] + b.rot[2][0]*a.rot[2][2];
    c.rot[0][1] = b.rot[0][1]*a.rot[0][0] + b.rot[1][1]*a.rot[0][1] + b.rot[2][1]*a.rot[0][2];
    c.rot[1][1] = b.rot[0][1]*a.rot[1][0] + b.rot[1][1]*a.rot[1][1] + b.rot[2][1]*a.rot[1][2];
    c.rot[2][1] = b.rot[0][1]*a.rot[2][0] + b.rot[1][1]*a.rot[2][1] + b.rot[2][1]*a.rot[2][2];
    c.rot[0][2] = b.rot[0][2]*a.rot[0][0] + b.rot[1][2]*a.rot[0][1] + b.rot[2][2]*a.rot[0][2];
    c.rot[1][2] = b.rot[0][2]*a.rot[1][0] + b.rot[1][2]*a.rot[1][1] + b.rot[2][2]*a.rot[1][2];
    c.rot[2][2] = b.rot[0][2]*a.rot[2][0] + b.rot[1][2]*a.rot[2][1] + b.rot[2][2]*a.rot[2][2];

    c.status = ASTRO_SUCCESS;
    return c;
}

/**
 * @brief Converts spherical coordinates to Cartesian coordinates.
 *
 * Given spherical coordinates and a time at which they are valid,
 * returns a vector of Cartesian coordinates. The returned value
 * includes the time, as required by the type #astro_vector_t.
 *
 * @param sphere
 *      Spherical coordinates to be converted.
 *
 * @param time
 *      The time that should be included in the return value.
 *
 * @return
 *      The vector form of the supplied spherical coordinates.
 */
astro_vector_t Astronomy_VectorFromSphere(astro_spherical_t sphere, astro_time_t time)
{
    astro_vector_t vector;
    double radlat, radlon, rcoslat;

    if (sphere.status != ASTRO_SUCCESS)
        return VecError(ASTRO_INVALID_PARAMETER, time);

    radlat = sphere.lat * DEG2RAD;
    radlon = sphere.lon * DEG2RAD;
    rcoslat = sphere.dist * cos(radlat);

    vector.status = ASTRO_SUCCESS;
    vector.t = time;
    vector.x = rcoslat * cos(radlon);
    vector.y = rcoslat * sin(radlon);
    vector.z = sphere.dist * sin(radlat);

    return vector;
}


/**
 * @brief Converts Cartesian coordinates to spherical coordinates.
 *
 * Given a Cartesian vector, returns latitude, longitude, and distance.
 *
 * @param vector
 *      Cartesian vector to be converted to spherical coordinates.
 *
 * @return
 *      Spherical coordinates that are equivalent to the given vector.
 */
astro_spherical_t Astronomy_SphereFromVector(astro_vector_t vector)
{
    double xyproj;
    astro_spherical_t sphere;

    xyproj = vector.x*vector.x + vector.y*vector.y;
    sphere.dist = sqrt(xyproj + vector.z*vector.z);
    if (xyproj == 0.0)
    {
        if (vector.z == 0.0)
        {
            /* Indeterminate coordinates; pos vector has zero length. */
            return SphereError(ASTRO_INVALID_PARAMETER);
        }

        sphere.lon = 0.0;
        sphere.lat = (vector.z < 0.0) ? -90.0 : +90.0;
    }
    else
    {
        sphere.lon = RAD2DEG * atan2(vector.y, vector.x);
        if (sphere.lon < 0.0)
            sphere.lon += 360.0;

        sphere.lat = RAD2DEG * atan2(vector.z, sqrt(xyproj));
    }

    sphere.status = ASTRO_SUCCESS;
    return sphere;
}


/**
 * @brief
 *      Given angular equatorial coordinates in `equ`, calculates equatorial vector.
 *
 * @param equ
 *      Angular equatorial coordinates to be converted to a vector.
 *
 * @param time
 *      The date and time of the observation. This is needed because the returned
 *      vector requires a valid time value when passed to certain other functions.
 *
 * @return
 *      A vector in the equatorial system.
 */
astro_vector_t Astronomy_VectorFromEquator(astro_equatorial_t equ, astro_time_t time)
{
    astro_spherical_t sphere;

    if (equ.status != ASTRO_SUCCESS)
        return VecError(ASTRO_INVALID_PARAMETER, time);

    sphere.status = ASTRO_SUCCESS;
    sphere.lat = equ.dec;
    sphere.lon = 15.0 * equ.ra;     /* convert sidereal hours to degrees */
    sphere.dist = equ.dist;

    return Astronomy_VectorFromSphere(sphere, time);
}


/**
 * @brief
 *      Given an equatorial vector, calculates equatorial angular coordinates.
 *
 * @param vector
 *      A vector in an equatorial coordinate system.
 *
 * @return
 *      Angular coordinates expressed in the same equatorial system as `vector`.
 */
astro_equatorial_t Astronomy_EquatorFromVector(astro_vector_t vector)
{
    astro_equatorial_t equ;
    astro_spherical_t sphere;

    sphere = Astronomy_SphereFromVector(vector);
    if (sphere.status != ASTRO_SUCCESS)
        return EquError(sphere.status);

    equ.status = ASTRO_SUCCESS;
    equ.dec = sphere.lat;
    equ.ra = sphere.lon / 15.0;     /* convert degrees to sidereal hours */
    equ.dist = sphere.dist;

    return equ;
}


static double ToggleAzimuthDirection(double az)
{
    az = 360.0 - az;
    if (az >= 360.0)
        az -= 360.0;
    else if (az < 0.0)
        az += 360.0;
    return az;
}

/**
 * @brief Converts Cartesian coordinates to horizontal coordinates.
 *
 * Given a horizontal Cartesian vector, returns horizontal azimuth and altitude.
 *
 * *IMPORTANT:* This function differs from #Astronomy_SphereFromVector in two ways:
 * - `Astronomy_SphereFromVector` returns a `lon` value that represents azimuth defined counterclockwise
 *   from north (e.g., west = +90), but this function represents a clockwise rotation
 *   (e.g., east = +90). The difference is because `Astronomy_SphereFromVector` is intended
 *   to preserve the vector "right-hand rule", while this function defines azimuth in a more
 *   traditional way as used in navigation and cartography.
 * - This function optionally corrects for atmospheric refraction, while `Astronomy_SphereFromVector`
 *   does not.
 *
 * The returned structure contains the azimuth in `lon`.
 * It is measured in degrees clockwise from north: east = +90 degrees, west = +270 degrees.
 *
 * The altitude is stored in `lat`.
 *
 * The distance to the observed object is stored in `dist`,
 * and is expressed in astronomical units (AU).
 *
 * @param vector
 *      Cartesian vector to be converted to horizontal coordinates.
 *
 * @param refraction
 *      `REFRACTION_NORMAL`: correct altitude for atmospheric refraction (recommended).
 *      `REFRACTION_NONE`: no atmospheric refraction correction is performed.
 *      `REFRACTION_JPLHOR`: for JPL Horizons compatibility testing only; not recommended for normal use.
 *
 * @return
 *      If successful, `status` holds `ASTRO_SUCCESS` and the other fields are valid as described
 *      in the function remarks.
 *      Otherwise `status` holds an error code and the other fields are undefined.
 */
astro_spherical_t Astronomy_HorizonFromVector(astro_vector_t vector, astro_refraction_t refraction)
{
    astro_spherical_t sphere;

    sphere = Astronomy_SphereFromVector(vector);
    if (sphere.status == ASTRO_SUCCESS)
    {
        /* Convert azimuth from counterclockwise-from-north to clockwise-from-north. */
        sphere.lon = ToggleAzimuthDirection(sphere.lon);
        sphere.lat += Astronomy_Refraction(refraction, sphere.lat);
    }

    return sphere;
}


/**
 * @brief
 *      Given apparent angular horizontal coordinates in `sphere`, calculate horizontal vector.
 *
 * @param sphere
 *      A structure that contains apparent horizontal coordinates:
 *      `lat` holds the refracted azimuth angle,
 *      `lon` holds the azimuth in degrees clockwise from north,
 *      and `dist` holds the distance from the observer to the object in AU.
 *
 * @param time
 *      The date and time of the observation. This is needed because the returned
 *      #astro_vector_t structure requires a valid time value when passed to certain other functions.
 *
 * @param refraction
 *      The refraction option used to model atmospheric lensing. See #Astronomy_Refraction.
 *      This specifies how refraction is to be removed from the altitude stored in `sphere.lat`.
 *
 * @return
 *      A vector in the horizontal system: `x` = north, `y` = west, and `z` = zenith (up).
 */
astro_vector_t Astronomy_VectorFromHorizon(astro_spherical_t sphere, astro_time_t time, astro_refraction_t refraction)
{
    if (sphere.status != ASTRO_SUCCESS)
        return VecError(ASTRO_INVALID_PARAMETER, time);

    /* Convert azimuth from clockwise-from-north to counterclockwise-from-north. */
    sphere.lon = ToggleAzimuthDirection(sphere.lon);

    /* Reverse any applied refraction. */
    sphere.lat += Astronomy_InverseRefraction(refraction, sphere.lat);

    return Astronomy_VectorFromSphere(sphere, time);
}


/**
 * @brief
 *      Calculates the amount of "lift" to an altitude angle caused by atmospheric refraction.
 *
 * Given an altitude angle and a refraction option, calculates
 * the amount of "lift" caused by atmospheric refraction.
 * This is the number of degrees higher in the sky an object appears
 * due to the lensing of the Earth's atmosphere.
 *
 * @param refraction
 *      The option selecting which refraction correction to use.
 *      If `REFRACTION_NORMAL`, uses a well-behaved refraction model that works well for
 *      all valid values (-90 to +90) of `altitude`.
 *      If `REFRACTION_JPLHOR`, this function returns a compatible value with the JPL Horizons tool.
 *      If any other value (including `REFRACTION_NONE`), this function returns 0.
 *
 * @param altitude
 *      An altitude angle in a horizontal coordinate system. Must be a value between -90 and +90.
 *
 * @return
 *      The angular adjustment in degrees to be added to the altitude angle to correct for atmospheric lensing.
 */
double Astronomy_Refraction(astro_refraction_t refraction, double altitude)
{
    double refr, hd;

    if (altitude < -90.0 || altitude > +90.0)
        return 0.0;     /* no attempt to correct an invalid altitude */

    if (refraction == REFRACTION_NORMAL || refraction == REFRACTION_JPLHOR)
    {
        // http://extras.springer.com/1999/978-1-4471-0555-8/chap4/horizons/horizons.pdf
        // JPL Horizons says it uses refraction algorithm from
        // Meeus "Astronomical Algorithms", 1991, p. 101-102.
        // I found the following Go implementation:
        // https://github.com/soniakeys/meeus/blob/master/v3/refraction/refract.go
        // This is a translation from the function "Saemundsson" there.
        // I found experimentally that JPL Horizons clamps the angle to 1 degree below the horizon.
        // This is important because the 'refr' formula below goes crazy near hd = -5.11.
        hd = altitude;
        if (hd < -1.0)
            hd = -1.0;

        refr = (1.02 / tan((hd+10.3/(hd+5.11))*DEG2RAD)) / 60.0;

        if (refraction == REFRACTION_NORMAL && altitude < -1.0)
        {
            // In "normal" mode we gradually reduce refraction toward the nadir
            // so that we never get an altitude angle less than -90 degrees.
            // When horizon angle is -1 degrees, the factor is exactly 1.
            // As altitude approaches -90 (the nadir), the fraction approaches 0 linearly.
            refr *= (altitude + 90.0) / 89.0;
        }
    }
    else
    {
        /* No refraction, or the refraction option is invalid. */
        refr = 0.0;
    }

    return refr;
}


/**
 * @brief
 *      Calculates the inverse of an atmospheric refraction angle.
 *
 * Given an observed altitude angle that includes atmospheric refraction,
 * calculate the negative angular correction to obtain the unrefracted
 * altitude. This is useful for cases where observed horizontal
 * coordinates are to be converted to another orientation system,
 * but refraction first must be removed from the observed position.
 *
 * @param refraction
 *      The option selecting which refraction correction to use.
 *      See #Astronomy_Refraction.
 *
 * @param bent_altitude
 *      The apparent altitude that includes atmospheric refraction.
 *
 * @return
 *      The angular adjustment in degrees to be added to the
 *      altitude angle to correct for atmospheric lensing.
 *      This will be less than or equal to zero.
 */
double Astronomy_InverseRefraction(astro_refraction_t refraction, double bent_altitude)
{
    double altitude, diff;

    if (bent_altitude < -90.0 || bent_altitude > +90.0)
        return 0.0;     /* no attempt to correct an invalid altitude */

    /* Find the pre-adjusted altitude whose refraction correction leads to 'altitude'. */
    altitude = bent_altitude - Astronomy_Refraction(refraction, bent_altitude);
    for(;;)
    {
        /* See how close we got. */
        diff = (altitude + Astronomy_Refraction(refraction, altitude)) - bent_altitude;
        if (fabs(diff) < 1.0e-14)
            return altitude - bent_altitude;

        altitude -= diff;
    }
}

/**
 * @brief
 *      Applies a rotation to a vector, yielding a rotated vector.
 *
 * This function transforms a vector in one orientation to a vector
 * in another orientation.
 *
 * @param rotation
 *      A rotation matrix that specifies how the orientation of the vector is to be changed.
 *
 * @param vector
 *      The vector whose orientation is to be changed.
 *
 * @return
 *      A vector in the orientation specified by `rotation`.
 */
astro_vector_t Astronomy_RotateVector(astro_rotation_t rotation, astro_vector_t vector)
{
    astro_vector_t target;

    if (rotation.status != ASTRO_SUCCESS || vector.status != ASTRO_SUCCESS)
        return VecError(ASTRO_INVALID_PARAMETER, vector.t);

    target.status = ASTRO_SUCCESS;
    target.t = vector.t;
    target.x = rotation.rot[0][0]*vector.x + rotation.rot[1][0]*vector.y + rotation.rot[2][0]*vector.z;
    target.y = rotation.rot[0][1]*vector.x + rotation.rot[1][1]*vector.y + rotation.rot[2][1]*vector.z;
    target.z = rotation.rot[0][2]*vector.x + rotation.rot[1][2]*vector.y + rotation.rot[2][2]*vector.z;

    return target;
}


/**
 * @brief
 *      Calculates a rotation matrix from equatorial J2000 (EQJ) to ecliptic J2000 (ECL).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: EQJ = equatorial system, using equator at J2000 epoch.
 * Target: ECL = ecliptic system, using equator at J2000 epoch.
 *
 * @return
 *      A rotation matrix that converts EQJ to ECL.
 */
astro_rotation_t Astronomy_Rotation_EQJ_ECL(void)
{
    /* ob = mean obliquity of the J2000 ecliptic = 0.40909260059599012 radians. */
    static const double c = 0.9174821430670688;    /* cos(ob) */
    static const double s = 0.3977769691083922;    /* sin(ob) */
    astro_rotation_t r;

    r.status = ASTRO_SUCCESS;
    r.rot[0][0] = 1.0;  r.rot[1][0] = 0.0;  r.rot[2][0] = 0.0;
    r.rot[0][1] = 0.0;  r.rot[1][1] = +c;   r.rot[2][1] = +s;
    r.rot[0][2] = 0.0;  r.rot[1][2] = -s;   r.rot[2][2] = +c;
    return r;
}

/**
 * @brief
 *      Calculates a rotation matrix from ecliptic J2000 (ECL) to equatorial J2000 (EQJ).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: ECL = ecliptic system, using equator at J2000 epoch.
 * Target: EQJ = equatorial system, using equator at J2000 epoch.
 *
 * @return
 *      A rotation matrix that converts ECL to EQJ.
 */
astro_rotation_t Astronomy_Rotation_ECL_EQJ(void)
{
    /* ob = mean obliquity of the J2000 ecliptic = 0.40909260059599012 radians. */
    static const double c = 0.9174821430670688;    /* cos(ob) */
    static const double s = 0.3977769691083922;    /* sin(ob) */
    astro_rotation_t r;

    r.status = ASTRO_SUCCESS;
    r.rot[0][0] = 1.0;  r.rot[1][0] = 0.0;  r.rot[2][0] = 0.0;
    r.rot[0][1] = 0.0;  r.rot[1][1] = +c;   r.rot[2][1] = -s;
    r.rot[0][2] = 0.0;  r.rot[1][2] = +s;   r.rot[2][2] = +c;
    return r;
}

/**
 * @brief
 *      Calculates a rotation matrix from equatorial J2000 (EQJ) to equatorial of-date (EQD).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: EQJ = equatorial system, using equator at J2000 epoch.
 * Target: EQD = equatorial system, using equator of the specified date/time.
 *
 * @param time
 *      The date and time at which the Earth's equator defines the target orientation.
 *
 * @return
 *      A rotation matrix that converts EQJ to EQD at `time`.
 */
astro_rotation_t Astronomy_Rotation_EQJ_EQD(astro_time_t time)
{
    astro_rotation_t prec, nut;

    prec = precession_rot(0.0, time.tt);
    nut = nutation_rot(&time, 0);
    return Astronomy_CombineRotation(prec, nut);
}

/**
 * @brief
 *      Calculates a rotation matrix from equatorial of-date (EQD) to equatorial J2000 (EQJ).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: EQD = equatorial system, using equator of the specified date/time.
 * Target: EQJ = equatorial system, using equator at J2000 epoch.
 *
 * @param time
 *      The date and time at which the Earth's equator defines the source orientation.
 *
 * @return
 *      A rotation matrix that converts EQD at `time` to EQJ.
 */
astro_rotation_t Astronomy_Rotation_EQD_EQJ(astro_time_t time)
{
    astro_rotation_t prec, nut;

    nut = nutation_rot(&time, 1);
    prec = precession_rot(time.tt, 0.0);
    return Astronomy_CombineRotation(nut, prec);
}


/**
 * @brief
 *      Calculates a rotation matrix from equatorial of-date (EQD) to horizontal (HOR).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: EQD = equatorial system, using equator of the specified date/time.
 * Target: HOR = horizontal system.
 *
 * @param time
 *      The date and time at which the Earth's equator applies.
 *
 * @param observer
 *      A location near the Earth's mean sea level that defines the observer's horizon.
 *
 * @return
 *      A rotation matrix that converts EQD to HOR at `time` and for `observer`.
 *      The components of the horizontal vector are:
 *      x = north, y = west, z = zenith (straight up from the observer).
 *      These components are chosen so that the "right-hand rule" works for the vector
 *      and so that north represents the direction where azimuth = 0.
 */
astro_rotation_t Astronomy_Rotation_EQD_HOR(astro_time_t time, astro_observer_t observer)
{
    astro_rotation_t rot;
    double uze[3], une[3], uwe[3];
    double uz[3], un[3], uw[3];
    double spin_angle;

    double sinlat = sin(observer.latitude * DEG2RAD);
    double coslat = cos(observer.latitude * DEG2RAD);
    double sinlon = sin(observer.longitude * DEG2RAD);
    double coslon = cos(observer.longitude * DEG2RAD);

    uze[0] = coslat * coslon;
    uze[1] = coslat * sinlon;
    uze[2] = sinlat;

    une[0] = -sinlat * coslon;
    une[1] = -sinlat * sinlon;
    une[2] = coslat;

    uwe[0] = sinlon;
    uwe[1] = -coslon;
    uwe[2] = 0.0;

    spin_angle = -15.0 * sidereal_time(&time);
    spin(spin_angle, uze, uz);
    spin(spin_angle, une, un);
    spin(spin_angle, uwe, uw);

    rot.rot[0][0] = un[0]; rot.rot[1][0] = un[1]; rot.rot[2][0] = un[2];
    rot.rot[0][1] = uw[0]; rot.rot[1][1] = uw[1]; rot.rot[2][1] = uw[2];
    rot.rot[0][2] = uz[0]; rot.rot[1][2] = uz[1]; rot.rot[2][2] = uz[2];

    rot.status = ASTRO_SUCCESS;
    return rot;
}


/**
 * @brief
 *      Calculates a rotation matrix from horizontal (HOR) to equatorial of-date (EQD).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: HOR = horizontal system (x=North, y=West, z=Zenith).
 * Target: EQD = equatorial system, using equator of the specified date/time.
 *
 * @param time
 *      The date and time at which the Earth's equator applies.
 *
 * @param observer
 *      A location near the Earth's mean sea level that defines the observer's horizon.
 *
 * @return
 *      A rotation matrix that converts HOR to EQD at `time` and for `observer`.
 */
astro_rotation_t Astronomy_Rotation_HOR_EQD(astro_time_t time, astro_observer_t observer)
{
    astro_rotation_t rot = Astronomy_Rotation_EQD_HOR(time, observer);
    return Astronomy_InverseRotation(rot);
}


/**
 * @brief
 *      Calculates a rotation matrix from horizontal (HOR) to J2000 equatorial (EQJ).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: HOR = horizontal system (x=North, y=West, z=Zenith).
 * Target: EQJ = equatorial system, using equator at the J2000 epoch.
 *
 * @param time
 *      The date and time of the observation.
 *
 * @param observer
 *      A location near the Earth's mean sea level that defines the observer's horizon.
 *
 * @return
 *      A rotation matrix that converts HOR to EQD at `time` and for `observer`.
 */
astro_rotation_t Astronomy_Rotation_HOR_EQJ(astro_time_t time, astro_observer_t observer)
{
    astro_rotation_t hor_eqd, eqd_eqj;

    hor_eqd = Astronomy_Rotation_HOR_EQD(time, observer);
    eqd_eqj = Astronomy_Rotation_EQD_EQJ(time);
    return Astronomy_CombineRotation(hor_eqd, eqd_eqj);
}


/**
 * @brief
 *      Calculates a rotation matrix from equatorial J2000 (EQJ) to horizontal (HOR).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: EQJ = equatorial system, using the equator at the J2000 epoch.
 * Target: HOR = horizontal system.
 *
 * @param time
 *      The date and time of the desired horizontal orientation.
 *
 * @param observer
 *      A location near the Earth's mean sea level that defines the observer's horizon.
 *
 * @return
 *      A rotation matrix that converts EQJ to HOR at `time` and for `observer`.
 *      The components of the horizontal vector are:
 *      x = north, y = west, z = zenith (straight up from the observer).
 *      These components are chosen so that the "right-hand rule" works for the vector
 *      and so that north represents the direction where azimuth = 0.
 */
astro_rotation_t Astronomy_Rotation_EQJ_HOR(astro_time_t time, astro_observer_t observer)
{
    astro_rotation_t rot = Astronomy_Rotation_HOR_EQJ(time, observer);
    return Astronomy_InverseRotation(rot);
}


/**
 * @brief
 *      Calculates a rotation matrix from equatorial of-date (EQD) to ecliptic J2000 (ECL).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: EQD = equatorial system, using equator of date.
 * Target: ECL = ecliptic system, using equator at J2000 epoch.
 *
 * @param time
 *      The date and time of the source equator.
 *
 * @return
 *      A rotation matrix that converts EQD to ECL.
 */
astro_rotation_t Astronomy_Rotation_EQD_ECL(astro_time_t time)
{
    astro_rotation_t eqd_eqj;
    astro_rotation_t eqj_ecl;

    eqd_eqj = Astronomy_Rotation_EQD_EQJ(time);
    eqj_ecl = Astronomy_Rotation_EQJ_ECL();
    return Astronomy_CombineRotation(eqd_eqj, eqj_ecl);
}


/**
 * @brief
 *      Calculates a rotation matrix from ecliptic J2000 (ECL) to equatorial of-date (EQD).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: ECL = ecliptic system, using equator at J2000 epoch.
 * Target: EQD = equatorial system, using equator of date.
 *
 * @param time
 *      The date and time of the desired equator.
 *
 * @return
 *      A rotation matrix that converts ECL to EQD.
 */
astro_rotation_t Astronomy_Rotation_ECL_EQD(astro_time_t time)
{
    astro_rotation_t rot = Astronomy_Rotation_EQD_ECL(time);
    return Astronomy_InverseRotation(rot);
}

/**
 * @brief
 *      Calculates a rotation matrix from ecliptic J2000 (ECL) to horizontal (HOR).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: ECL = ecliptic system, using equator at J2000 epoch.
 * Target: HOR = horizontal system.
 *
 * @param time
 *      The date and time of the desired horizontal orientation.
 *
 * @param observer
 *      A location near the Earth's mean sea level that defines the observer's horizon.
 *
 * @return
 *      A rotation matrix that converts ECL to HOR at `time` and for `observer`.
 *      The components of the horizontal vector are:
 *      x = north, y = west, z = zenith (straight up from the observer).
 *      These components are chosen so that the "right-hand rule" works for the vector
 *      and so that north represents the direction where azimuth = 0.
 */
astro_rotation_t Astronomy_Rotation_ECL_HOR(astro_time_t time, astro_observer_t observer)
{
    astro_rotation_t ecl_eqd = Astronomy_Rotation_ECL_EQD(time);
    astro_rotation_t eqd_hor = Astronomy_Rotation_EQD_HOR(time, observer);
    return Astronomy_CombineRotation(ecl_eqd, eqd_hor);
}

/**
 * @brief
 *      Calculates a rotation matrix from horizontal (HOR) to ecliptic J2000 (ECL).
 *
 * This is one of the family of functions that returns a rotation matrix
 * for converting from one orientation to another.
 * Source: HOR = horizontal system.
 * Target: ECL = ecliptic system, using equator at J2000 epoch.
 *
 * @param time
 *      The date and time of the horizontal observation.
 *
 * @param observer
 *      The location of the horizontal observer.
 *
 * @return
 *      A rotation matrix that converts HOR to ECL.
 */
astro_rotation_t Astronomy_Rotation_HOR_ECL(astro_time_t time, astro_observer_t observer)
{
    astro_rotation_t rot = Astronomy_Rotation_ECL_HOR(time, observer);
    return Astronomy_InverseRotation(rot);
}


/** @cond DOXYGEN_SKIP */
typedef struct
{
    const char *symbol;
    const char *name;
}
constel_info_t;


typedef struct
{
    int    index;
    double ra_lo;
    double ra_hi;
    double dec_lo;
}
constel_boundary_t;
/** @endcond */

#define NUM_CONSTELLATIONS   88

static const constel_info_t ConstelInfo[] = {
    /*  0 */ { "And", "Andromeda"            }
,   /*  1 */ { "Ant", "Antila"               }
,   /*  2 */ { "Aps", "Apus"                 }
,   /*  3 */ { "Aql", "Aquila"               }
,   /*  4 */ { "Aqr", "Aquarius"             }
,   /*  5 */ { "Ara", "Ara"                  }
,   /*  6 */ { "Ari", "Aries"                }
,   /*  7 */ { "Aur", "Auriga"               }
,   /*  8 */ { "Boo", "Bootes"               }
,   /*  9 */ { "Cae", "Caelum"               }
,   /* 10 */ { "Cam", "Camelopardis"         }
,   /* 11 */ { "Cap", "Capricornus"          }
,   /* 12 */ { "Car", "Carina"               }
,   /* 13 */ { "Cas", "Cassiopeia"           }
,   /* 14 */ { "Cen", "Centaurus"            }
,   /* 15 */ { "Cep", "Cepheus"              }
,   /* 16 */ { "Cet", "Cetus"                }
,   /* 17 */ { "Cha", "Chamaeleon"           }
,   /* 18 */ { "Cir", "Circinus"             }
,   /* 19 */ { "CMa", "Canis Major"          }
,   /* 20 */ { "CMi", "Canis Minor"          }
,   /* 21 */ { "Cnc", "Cancer"               }
,   /* 22 */ { "Col", "Columba"              }
,   /* 23 */ { "Com", "Coma Berenices"       }
,   /* 24 */ { "CrA", "Corona Australis"     }
,   /* 25 */ { "CrB", "Corona Borealis"      }
,   /* 26 */ { "Crt", "Crater"               }
,   /* 27 */ { "Cru", "Crux"                 }
,   /* 28 */ { "Crv", "Corvus"               }
,   /* 29 */ { "CVn", "Canes Venatici"       }
,   /* 30 */ { "Cyg", "Cygnus"               }
,   /* 31 */ { "Del", "Delphinus"            }
,   /* 32 */ { "Dor", "Dorado"               }
,   /* 33 */ { "Dra", "Draco"                }
,   /* 34 */ { "Equ", "Equuleus"             }
,   /* 35 */ { "Eri", "Eridanus"             }
,   /* 36 */ { "For", "Fornax"               }
,   /* 37 */ { "Gem", "Gemini"               }
,   /* 38 */ { "Gru", "Grus"                 }
,   /* 39 */ { "Her", "Hercules"             }
,   /* 40 */ { "Hor", "Horologium"           }
,   /* 41 */ { "Hya", "Hydra"                }
,   /* 42 */ { "Hyi", "Hydrus"               }
,   /* 43 */ { "Ind", "Indus"                }
,   /* 44 */ { "Lac", "Lacerta"              }
,   /* 45 */ { "Leo", "Leo"                  }
,   /* 46 */ { "Lep", "Lepus"                }
,   /* 47 */ { "Lib", "Libra"                }
,   /* 48 */ { "LMi", "Leo Minor"            }
,   /* 49 */ { "Lup", "Lupus"                }
,   /* 50 */ { "Lyn", "Lynx"                 }
,   /* 51 */ { "Lyr", "Lyra"                 }
,   /* 52 */ { "Men", "Mensa"                }
,   /* 53 */ { "Mic", "Microscopium"         }
,   /* 54 */ { "Mon", "Monoceros"            }
,   /* 55 */ { "Mus", "Musca"                }
,   /* 56 */ { "Nor", "Norma"                }
,   /* 57 */ { "Oct", "Octans"               }
,   /* 58 */ { "Oph", "Ophiuchus"            }
,   /* 59 */ { "Ori", "Orion"                }
,   /* 60 */ { "Pav", "Pavo"                 }
,   /* 61 */ { "Peg", "Pegasus"              }
,   /* 62 */ { "Per", "Perseus"              }
,   /* 63 */ { "Phe", "Phoenix"              }
,   /* 64 */ { "Pic", "Pictor"               }
,   /* 65 */ { "PsA", "Pisces Austrinus"     }
,   /* 66 */ { "Psc", "Pisces"               }
,   /* 67 */ { "Pup", "Puppis"               }
,   /* 68 */ { "Pyx", "Pyxis"                }
,   /* 69 */ { "Ret", "Reticulum"            }
,   /* 70 */ { "Scl", "Sculptor"             }
,   /* 71 */ { "Sco", "Scorpius"             }
,   /* 72 */ { "Sct", "Scutum"               }
,   /* 73 */ { "Ser", "Serpens"              }
,   /* 74 */ { "Sex", "Sextans"              }
,   /* 75 */ { "Sge", "Sagitta"              }
,   /* 76 */ { "Sgr", "Sagittarius"          }
,   /* 77 */ { "Tau", "Taurus"               }
,   /* 78 */ { "Tel", "Telescopium"          }
,   /* 79 */ { "TrA", "Triangulum Australe"  }
,   /* 80 */ { "Tri", "Triangulum"           }
,   /* 81 */ { "Tuc", "Tucana"               }
,   /* 82 */ { "UMa", "Ursa Major"           }
,   /* 83 */ { "UMi", "Ursa Minor"           }
,   /* 84 */ { "Vel", "Vela"                 }
,   /* 85 */ { "Vir", "Virgo"                }
,   /* 86 */ { "Vol", "Volans"               }
,   /* 87 */ { "Vul", "Vulpecula"            }
};

static const constel_boundary_t ConstelBounds[] = {
    { 83,  0.00000000000000, 24.00000000000000, 88.00000000000000 }    /* UMi */
,   { 83,  8.00000000000000, 14.50000000000000, 86.50000000000000 }    /* UMi */
,   { 83, 21.00000000000000, 23.00000000000000, 86.16666666666667 }    /* UMi */
,   { 83, 18.00000000000000, 21.00000000000000, 86.00000000000000 }    /* UMi */
,   { 15,  0.00000000000000,  8.00000000000000, 85.00000000000000 }    /* Cep */
,   { 10,  9.16666666666667, 10.66666666666667, 82.00000000000000 }    /* Cam */
,   { 15,  0.00000000000000,  5.00000000000000, 80.00000000000000 }    /* Cep */
,   { 10, 10.66666666666667, 14.50000000000000, 80.00000000000000 }    /* Cam */
,   { 83, 17.50000000000000, 18.00000000000000, 80.00000000000000 }    /* UMi */
,   { 33, 20.16666666666667, 21.00000000000000, 80.00000000000000 }    /* Dra */
,   { 15,  0.00000000000000,  3.50833333333333, 77.00000000000000 }    /* Cep */
,   { 10, 11.50000000000000, 13.58333333333333, 77.00000000000000 }    /* Cam */
,   { 83, 16.53333333333333, 17.50000000000000, 75.00000000000000 }    /* UMi */
,   { 15, 20.16666666666667, 20.66666666666667, 75.00000000000000 }    /* Cep */
,   { 10,  7.96666666666667,  9.16666666666667, 73.50000000000000 }    /* Cam */
,   { 33,  9.16666666666667, 11.33333333333333, 73.50000000000000 }    /* Dra */
,   { 83, 13.00000000000000, 16.53333333333333, 70.00000000000000 }    /* UMi */
,   { 13,  3.10000000000000,  3.41666666666667, 68.00000000000000 }    /* Cas */
,   { 33, 20.41666666666667, 20.66666666666667, 67.00000000000000 }    /* Dra */
,   { 33, 11.33333333333333, 12.00000000000000, 66.50000000000000 }    /* Dra */
,   { 15,  0.00000000000000,  0.33333333333333, 66.00000000000000 }    /* Cep */
,   { 83, 14.00000000000000, 15.66666666666667, 66.00000000000000 }    /* UMi */
,   { 15, 23.58333333333333, 24.00000000000000, 66.00000000000000 }    /* Cep */
,   { 33, 12.00000000000000, 13.50000000000000, 64.00000000000000 }    /* Dra */
,   { 33, 13.50000000000000, 14.41666666666667, 63.00000000000000 }    /* Dra */
,   { 15, 23.16666666666667, 23.58333333333333, 63.00000000000000 }    /* Cep */
,   { 10,  6.10000000000000,  7.00000000000000, 62.00000000000000 }    /* Cam */
,   { 33, 20.00000000000000, 20.41666666666667, 61.50000000000000 }    /* Dra */
,   { 15, 20.53666666666667, 20.60000000000000, 60.91666666666666 }    /* Cep */
,   { 10,  7.00000000000000,  7.96666666666667, 60.00000000000000 }    /* Cam */
,   { 82,  7.96666666666667,  8.41666666666667, 60.00000000000000 }    /* UMa */
,   { 33, 19.76666666666667, 20.00000000000000, 59.50000000000000 }    /* Dra */
,   { 15, 20.00000000000000, 20.53666666666667, 59.50000000000000 }    /* Cep */
,   { 15, 22.86666666666667, 23.16666666666667, 59.08333333333334 }    /* Cep */
,   { 13,  0.00000000000000,  2.43333333333333, 58.50000000000000 }    /* Cas */
,   { 33, 19.41666666666667, 19.76666666666667, 58.00000000000000 }    /* Dra */
,   { 13,  1.70000000000000,  1.90833333333333, 57.50000000000000 }    /* Cas */
,   { 13,  2.43333333333333,  3.10000000000000, 57.00000000000000 }    /* Cas */
,   { 10,  3.10000000000000,  3.16666666666667, 57.00000000000000 }    /* Cam */
,   { 15, 22.31666666666667, 22.86666666666667, 56.25000000000000 }    /* Cep */
,   { 10,  5.00000000000000,  6.10000000000000, 56.00000000000000 }    /* Cam */
,   { 82, 14.03333333333333, 14.41666666666667, 55.50000000000000 }    /* UMa */
,   { 33, 14.41666666666667, 19.41666666666667, 55.50000000000000 }    /* Dra */
,   { 10,  3.16666666666667,  3.33333333333333, 55.00000000000000 }    /* Cam */
,   { 15, 22.13333333333333, 22.31666666666667, 55.00000000000000 }    /* Cep */
,   { 15, 20.60000000000000, 21.96666666666667, 54.83333333333334 }    /* Cep */
,   { 13,  0.00000000000000,  1.70000000000000, 54.00000000000000 }    /* Cas */
,   { 50,  6.10000000000000,  6.50000000000000, 54.00000000000000 }    /* Lyn */
,   { 82, 12.08333333333333, 13.50000000000000, 53.00000000000000 }    /* UMa */
,   { 33, 15.25000000000000, 15.75000000000000, 53.00000000000000 }    /* Dra */
,   { 15, 21.96666666666667, 22.13333333333333, 52.75000000000000 }    /* Cep */
,   { 10,  3.33333333333333,  5.00000000000000, 52.50000000000000 }    /* Cam */
,   { 13, 22.86666666666667, 23.33333333333333, 52.50000000000000 }    /* Cas */
,   { 33, 15.75000000000000, 17.00000000000000, 51.50000000000000 }    /* Dra */
,   { 62,  2.04166666666667,  2.51666666666667, 50.50000000000000 }    /* Per */
,   { 33, 17.00000000000000, 18.23333333333333, 50.50000000000000 }    /* Dra */
,   { 13,  0.00000000000000,  1.36666666666667, 50.00000000000000 }    /* Cas */
,   { 62,  1.36666666666667,  1.66666666666667, 50.00000000000000 }    /* Per */
,   { 50,  6.50000000000000,  6.80000000000000, 50.00000000000000 }    /* Lyn */
,   { 13, 23.33333333333333, 24.00000000000000, 50.00000000000000 }    /* Cas */
,   { 82, 13.50000000000000, 14.03333333333333, 48.50000000000000 }    /* UMa */
,   { 13,  0.00000000000000,  1.11666666666667, 48.00000000000000 }    /* Cas */
,   { 13, 23.58333333333333, 24.00000000000000, 48.00000000000000 }    /* Cas */
,   { 39, 18.17500000000000, 18.23333333333333, 47.50000000000000 }    /* Her */
,   { 33, 18.23333333333333, 19.08333333333333, 47.50000000000000 }    /* Dra */
,   { 30, 19.08333333333333, 19.16666666666667, 47.50000000000000 }    /* Cyg */
,   { 62,  1.66666666666667,  2.04166666666667, 47.00000000000000 }    /* Per */
,   { 82,  8.41666666666667,  9.16666666666667, 47.00000000000000 }    /* UMa */
,   { 13,  0.16666666666667,  0.86666666666667, 46.00000000000000 }    /* Cas */
,   { 82, 12.00000000000000, 12.08333333333333, 45.00000000000000 }    /* UMa */
,   { 50,  6.80000000000000,  7.36666666666667, 44.50000000000000 }    /* Lyn */
,   { 30, 21.90833333333333, 21.96666666666667, 44.00000000000000 }    /* Cyg */
,   { 30, 21.87500000000000, 21.90833333333333, 43.75000000000000 }    /* Cyg */
,   { 30, 19.16666666666667, 19.40000000000000, 43.50000000000000 }    /* Cyg */
,   { 82,  9.16666666666667, 10.16666666666667, 42.00000000000000 }    /* UMa */
,   { 82, 10.16666666666667, 10.78333333333333, 40.00000000000000 }    /* UMa */
,   {  8, 15.43333333333333, 15.75000000000000, 40.00000000000000 }    /* Boo */
,   { 39, 15.75000000000000, 16.33333333333333, 40.00000000000000 }    /* Her */
,   { 50,  9.25000000000000,  9.58333333333333, 39.75000000000000 }    /* Lyn */
,   {  0,  0.00000000000000,  2.51666666666667, 36.75000000000000 }    /* And */
,   { 62,  2.51666666666667,  2.56666666666667, 36.75000000000000 }    /* Per */
,   { 51, 19.35833333333333, 19.40000000000000, 36.50000000000000 }    /* Lyr */
,   { 62,  4.50000000000000,  4.69166666666667, 36.00000000000000 }    /* Per */
,   { 30, 21.73333333333333, 21.87500000000000, 36.00000000000000 }    /* Cyg */
,   { 44, 21.87500000000000, 22.00000000000000, 36.00000000000000 }    /* Lac */
,   {  7,  6.53333333333333,  7.36666666666667, 35.50000000000000 }    /* Aur */
,   { 50,  7.36666666666667,  7.75000000000000, 35.50000000000000 }    /* Lyn */
,   {  0,  0.00000000000000,  2.00000000000000, 35.00000000000000 }    /* And */
,   { 44, 22.00000000000000, 22.81666666666667, 35.00000000000000 }    /* Lac */
,   { 44, 22.81666666666667, 22.86666666666667, 34.50000000000000 }    /* Lac */
,   {  0, 22.86666666666667, 23.50000000000000, 34.50000000000000 }    /* And */
,   { 62,  2.56666666666667,  2.71666666666667, 34.00000000000000 }    /* Per */
,   { 82, 10.78333333333333, 11.00000000000000, 34.00000000000000 }    /* UMa */
,   { 29, 12.00000000000000, 12.33333333333333, 34.00000000000000 }    /* CVn */
,   { 50,  7.75000000000000,  9.25000000000000, 33.50000000000000 }    /* Lyn */
,   { 48,  9.25000000000000,  9.88333333333333, 33.50000000000000 }    /* LMi */
,   {  0,  0.71666666666667,  1.40833333333333, 33.00000000000000 }    /* And */
,   {  8, 15.18333333333333, 15.43333333333333, 33.00000000000000 }    /* Boo */
,   {  0, 23.50000000000000, 23.75000000000000, 32.08333333333334 }    /* And */
,   { 29, 12.33333333333333, 13.25000000000000, 32.00000000000000 }    /* CVn */
,   {  0, 23.75000000000000, 24.00000000000000, 31.33333333333333 }    /* And */
,   { 29, 13.95833333333333, 14.03333333333333, 30.75000000000000 }    /* CVn */
,   { 80,  2.41666666666667,  2.71666666666667, 30.66666666666667 }    /* Tri */
,   { 62,  2.71666666666667,  4.50000000000000, 30.66666666666667 }    /* Per */
,   {  7,  4.50000000000000,  4.75000000000000, 30.00000000000000 }    /* Aur */
,   { 51, 18.17500000000000, 19.35833333333333, 30.00000000000000 }    /* Lyr */
,   { 82, 11.00000000000000, 12.00000000000000, 29.00000000000000 }    /* UMa */
,   { 30, 19.66666666666667, 20.91666666666667, 29.00000000000000 }    /* Cyg */
,   {  7,  4.75000000000000,  5.88333333333333, 28.50000000000000 }    /* Aur */
,   { 48,  9.88333333333333, 10.50000000000000, 28.50000000000000 }    /* LMi */
,   { 29, 13.25000000000000, 13.95833333333333, 28.50000000000000 }    /* CVn */
,   {  0,  0.00000000000000,  0.06666666666667, 28.00000000000000 }    /* And */
,   { 80,  1.40833333333333,  1.66666666666667, 28.00000000000000 }    /* Tri */
,   {  7,  5.88333333333333,  6.53333333333333, 28.00000000000000 }    /* Aur */
,   { 37,  7.88333333333333,  8.00000000000000, 28.00000000000000 }    /* Gem */
,   { 30, 20.91666666666667, 21.73333333333333, 28.00000000000000 }    /* Cyg */
,   { 30, 19.25833333333333, 19.66666666666667, 27.50000000000000 }    /* Cyg */
,   { 80,  1.91666666666667,  2.41666666666667, 27.25000000000000 }    /* Tri */
,   { 25, 16.16666666666667, 16.33333333333333, 27.00000000000000 }    /* CrB */
,   {  8, 15.08333333333333, 15.18333333333333, 26.00000000000000 }    /* Boo */
,   { 25, 15.18333333333333, 16.16666666666667, 26.00000000000000 }    /* CrB */
,   { 51, 18.36666666666667, 18.86666666666667, 26.00000000000000 }    /* Lyr */
,   { 48, 10.75000000000000, 11.00000000000000, 25.50000000000000 }    /* LMi */
,   { 51, 18.86666666666667, 19.25833333333333, 25.50000000000000 }    /* Lyr */
,   { 80,  1.66666666666667,  1.91666666666667, 25.00000000000000 }    /* Tri */
,   { 66,  0.71666666666667,  0.85000000000000, 23.75000000000000 }    /* Psc */
,   { 48, 10.50000000000000, 10.75000000000000, 23.50000000000000 }    /* LMi */
,   { 87, 21.25000000000000, 21.41666666666667, 23.50000000000000 }    /* Vul */
,   { 77,  5.70000000000000,  5.88333333333333, 22.83333333333333 }    /* Tau */
,   {  0,  0.06666666666667,  0.14166666666667, 22.00000000000000 }    /* And */
,   { 73, 15.91666666666667, 16.03333333333333, 22.00000000000000 }    /* Ser */
,   { 37,  5.88333333333333,  6.21666666666667, 21.50000000000000 }    /* Gem */
,   { 87, 19.83333333333333, 20.25000000000000, 21.25000000000000 }    /* Vul */
,   { 87, 18.86666666666667, 19.25000000000000, 21.08333333333333 }    /* Vul */
,   {  0,  0.14166666666667,  0.85000000000000, 21.00000000000000 }    /* And */
,   { 87, 20.25000000000000, 20.56666666666667, 20.50000000000000 }    /* Vul */
,   { 37,  7.80833333333333,  7.88333333333333, 20.00000000000000 }    /* Gem */
,   { 87, 20.56666666666667, 21.25000000000000, 19.50000000000000 }    /* Vul */
,   { 87, 19.25000000000000, 19.83333333333333, 19.16666666666667 }    /* Vul */
,   {  6,  3.28333333333333,  3.36666666666667, 19.00000000000000 }    /* Ari */
,   { 75, 18.86666666666667, 19.00000000000000, 18.50000000000000 }    /* Sge */
,   { 59,  5.70000000000000,  5.76666666666667, 18.00000000000000 }    /* Ori */
,   { 37,  6.21666666666667,  6.30833333333333, 17.50000000000000 }    /* Gem */
,   { 75, 19.00000000000000, 19.83333333333333, 16.16666666666667 }    /* Sge */
,   { 77,  4.96666666666667,  5.33333333333333, 16.00000000000000 }    /* Tau */
,   { 39, 15.91666666666667, 16.08333333333333, 16.00000000000000 }    /* Her */
,   { 75, 19.83333333333333, 20.25000000000000, 15.75000000000000 }    /* Sge */
,   { 77,  4.61666666666667,  4.96666666666667, 15.50000000000000 }    /* Tau */
,   { 77,  5.33333333333333,  5.60000000000000, 15.50000000000000 }    /* Tau */
,   { 23, 12.83333333333333, 13.50000000000000, 15.00000000000000 }    /* Com */
,   { 39, 17.25000000000000, 18.25000000000000, 14.33333333333333 }    /* Her */
,   { 23, 11.86666666666667, 12.83333333333333, 14.00000000000000 }    /* Com */
,   { 37,  7.50000000000000,  7.80833333333333, 13.50000000000000 }    /* Gem */
,   { 39, 16.75000000000000, 17.25000000000000, 12.83333333333333 }    /* Her */
,   { 61,  0.00000000000000,  0.14166666666667, 12.50000000000000 }    /* Peg */
,   { 77,  5.60000000000000,  5.76666666666667, 12.50000000000000 }    /* Tau */
,   { 37,  7.00000000000000,  7.50000000000000, 12.50000000000000 }    /* Gem */
,   { 61, 21.11666666666667, 21.33333333333333, 12.50000000000000 }    /* Peg */
,   { 37,  6.30833333333333,  6.93333333333333, 12.00000000000000 }    /* Gem */
,   { 39, 18.25000000000000, 18.86666666666667, 12.00000000000000 }    /* Her */
,   { 31, 20.87500000000000, 21.05000000000000, 11.83333333333333 }    /* Del */
,   { 61, 21.05000000000000, 21.11666666666667, 11.83333333333333 }    /* Peg */
,   { 45, 11.51666666666667, 11.86666666666667, 11.00000000000000 }    /* Leo */
,   { 59,  6.24166666666667,  6.30833333333333, 10.00000000000000 }    /* Ori */
,   { 37,  6.93333333333333,  7.00000000000000, 10.00000000000000 }    /* Gem */
,   { 21,  7.80833333333333,  7.92500000000000, 10.00000000000000 }    /* Cnc */
,   { 61, 23.83333333333333, 24.00000000000000, 10.00000000000000 }    /* Peg */
,   {  6,  1.66666666666667,  3.28333333333333,  9.91666666666667 }    /* Ari */
,   { 31, 20.14166666666667, 20.30000000000000,  8.50000000000000 }    /* Del */
,   {  8, 13.50000000000000, 15.08333333333333,  8.00000000000000 }    /* Boo */
,   { 61, 22.75000000000000, 23.83333333333333,  7.50000000000000 }    /* Peg */
,   { 21,  7.92500000000000,  9.25000000000000,  7.00000000000000 }    /* Cnc */
,   { 45,  9.25000000000000, 10.75000000000000,  7.00000000000000 }    /* Leo */
,   { 58, 18.25000000000000, 18.66222222222222,  6.25000000000000 }    /* Oph */
,   {  3, 18.66222222222222, 18.86666666666667,  6.25000000000000 }    /* Aql */
,   { 31, 20.83333333333333, 20.87500000000000,  6.00000000000000 }    /* Del */
,   { 20,  7.00000000000000,  7.01666666666667,  5.50000000000000 }    /* CMi */
,   { 73, 18.25000000000000, 18.42500000000000,  4.50000000000000 }    /* Ser */
,   { 39, 16.08333333333333, 16.75000000000000,  4.00000000000000 }    /* Her */
,   { 58, 18.25000000000000, 18.42500000000000,  3.00000000000000 }    /* Oph */
,   { 61, 21.46666666666667, 21.66666666666667,  2.75000000000000 }    /* Peg */
,   { 66,  0.00000000000000,  2.00000000000000,  2.00000000000000 }    /* Psc */
,   { 73, 18.58333333333333, 18.86666666666667,  2.00000000000000 }    /* Ser */
,   { 31, 20.30000000000000, 20.83333333333333,  2.00000000000000 }    /* Del */
,   { 34, 20.83333333333333, 21.33333333333333,  2.00000000000000 }    /* Equ */
,   { 61, 21.33333333333333, 21.46666666666667,  2.00000000000000 }    /* Peg */
,   { 61, 22.00000000000000, 22.75000000000000,  2.00000000000000 }    /* Peg */
,   { 61, 21.66666666666667, 22.00000000000000,  1.75000000000000 }    /* Peg */
,   { 20,  7.01666666666667,  7.20000000000000,  1.50000000000000 }    /* CMi */
,   { 77,  3.58333333333333,  4.61666666666667,  0.00000000000000 }    /* Tau */
,   { 59,  4.61666666666667,  4.66666666666667,  0.00000000000000 }    /* Ori */
,   { 20,  7.20000000000000,  8.08333333333333,  0.00000000000000 }    /* CMi */
,   { 85, 14.66666666666667, 15.08333333333333,  0.00000000000000 }    /* Vir */
,   { 58, 17.83333333333333, 18.25000000000000,  0.00000000000000 }    /* Oph */
,   { 16,  2.65000000000000,  3.28333333333333, -1.75000000000000 }    /* Cet */
,   { 77,  3.28333333333333,  3.58333333333333, -1.75000000000000 }    /* Tau */
,   { 73, 15.08333333333333, 16.26666666666667, -3.25000000000000 }    /* Ser */
,   { 59,  4.66666666666667,  5.08333333333333, -4.00000000000000 }    /* Ori */
,   { 59,  5.83333333333333,  6.24166666666667, -4.00000000000000 }    /* Ori */
,   { 73, 17.83333333333333, 17.96666666666667, -4.00000000000000 }    /* Ser */
,   { 73, 18.25000000000000, 18.58333333333333, -4.00000000000000 }    /* Ser */
,   {  3, 18.58333333333333, 18.86666666666667, -4.00000000000000 }    /* Aql */
,   { 66, 22.75000000000000, 23.83333333333333, -4.00000000000000 }    /* Psc */
,   { 45, 10.75000000000000, 11.51666666666667, -6.00000000000000 }    /* Leo */
,   { 85, 11.51666666666667, 11.83333333333333, -6.00000000000000 }    /* Vir */
,   { 66,  0.00000000000000,  0.33333333333333, -7.00000000000000 }    /* Psc */
,   { 66, 23.83333333333333, 24.00000000000000, -7.00000000000000 }    /* Psc */
,   { 85, 14.25000000000000, 14.66666666666667, -8.00000000000000 }    /* Vir */
,   { 58, 15.91666666666667, 16.26666666666667, -8.00000000000000 }    /* Oph */
,   {  3, 20.00000000000000, 20.53333333333333, -9.00000000000000 }    /* Aql */
,   {  4, 21.33333333333333, 21.86666666666667, -9.00000000000000 }    /* Aqr */
,   { 58, 17.16666666666667, 17.96666666666667, -10.00000000000000 }    /* Oph */
,   { 54,  5.83333333333333,  8.08333333333333, -11.00000000000000 }    /* Mon */
,   { 35,  4.91666666666667,  5.08333333333333, -11.00000000000000 }    /* Eri */
,   { 59,  5.08333333333333,  5.83333333333333, -11.00000000000000 }    /* Ori */
,   { 41,  8.08333333333333,  8.36666666666667, -11.00000000000000 }    /* Hya */
,   { 74,  9.58333333333333, 10.75000000000000, -11.00000000000000 }    /* Sex */
,   { 85, 11.83333333333333, 12.83333333333333, -11.00000000000000 }    /* Vir */
,   { 58, 17.58333333333333, 17.66666666666667, -11.66666666666667 }    /* Oph */
,   {  3, 18.86666666666667, 20.00000000000000, -12.03333333333333 }    /* Aql */
,   { 35,  4.83333333333333,  4.91666666666667, -14.50000000000000 }    /* Eri */
,   {  4, 20.53333333333333, 21.33333333333333, -15.00000000000000 }    /* Aqr */
,   { 73, 17.16666666666667, 18.25000000000000, -16.00000000000000 }    /* Ser */
,   { 72, 18.25000000000000, 18.86666666666667, -16.00000000000000 }    /* Sct */
,   { 41,  8.36666666666667,  8.58333333333333, -17.00000000000000 }    /* Hya */
,   { 58, 16.26666666666667, 16.37500000000000, -18.25000000000000 }    /* Oph */
,   { 41,  8.58333333333333,  9.08333333333333, -19.00000000000000 }    /* Hya */
,   { 26, 10.75000000000000, 10.83333333333333, -19.00000000000000 }    /* Crt */
,   { 71, 16.26666666666667, 16.37500000000000, -19.25000000000000 }    /* Sco */
,   { 47, 15.66666666666667, 15.91666666666667, -20.00000000000000 }    /* Lib */
,   { 28, 12.58333333333333, 12.83333333333333, -22.00000000000000 }    /* Crv */
,   { 85, 12.83333333333333, 14.25000000000000, -22.00000000000000 }    /* Vir */
,   { 41,  9.08333333333333,  9.75000000000000, -24.00000000000000 }    /* Hya */
,   { 16,  1.66666666666667,  2.65000000000000, -24.38333333333333 }    /* Cet */
,   { 35,  2.65000000000000,  3.75000000000000, -24.38333333333333 }    /* Eri */
,   { 26, 10.83333333333333, 11.83333333333333, -24.50000000000000 }    /* Crt */
,   { 28, 11.83333333333333, 12.58333333333333, -24.50000000000000 }    /* Crv */
,   { 47, 14.25000000000000, 14.91666666666667, -24.50000000000000 }    /* Lib */
,   { 58, 16.26666666666667, 16.75000000000000, -24.58333333333333 }    /* Oph */
,   { 16,  0.00000000000000,  1.66666666666667, -25.50000000000000 }    /* Cet */
,   { 11, 21.33333333333333, 21.86666666666667, -25.50000000000000 }    /* Cap */
,   {  4, 21.86666666666667, 23.83333333333333, -25.50000000000000 }    /* Aqr */
,   { 16, 23.83333333333333, 24.00000000000000, -25.50000000000000 }    /* Cet */
,   { 41,  9.75000000000000, 10.25000000000000, -26.50000000000000 }    /* Hya */
,   { 35,  4.70000000000000,  4.83333333333333, -27.25000000000000 }    /* Eri */
,   { 46,  4.83333333333333,  6.11666666666667, -27.25000000000000 }    /* Lep */
,   { 11, 20.00000000000000, 21.33333333333333, -28.00000000000000 }    /* Cap */
,   { 41, 10.25000000000000, 10.58333333333333, -29.16666666666667 }    /* Hya */
,   { 41, 12.58333333333333, 14.91666666666667, -29.50000000000000 }    /* Hya */
,   { 47, 14.91666666666667, 15.66666666666667, -29.50000000000000 }    /* Lib */
,   { 71, 15.66666666666667, 16.00000000000000, -29.50000000000000 }    /* Sco */
,   { 35,  4.58333333333333,  4.70000000000000, -30.00000000000000 }    /* Eri */
,   { 58, 16.75000000000000, 17.60000000000000, -30.00000000000000 }    /* Oph */
,   { 76, 17.60000000000000, 17.83333333333333, -30.00000000000000 }    /* Sgr */
,   { 41, 10.58333333333333, 10.83333333333333, -31.16666666666667 }    /* Hya */
,   { 19,  6.11666666666667,  7.36666666666667, -33.00000000000000 }    /* CMa */
,   { 41, 12.25000000000000, 12.58333333333333, -33.00000000000000 }    /* Hya */
,   { 41, 10.83333333333333, 12.25000000000000, -35.00000000000000 }    /* Hya */
,   { 36,  3.50000000000000,  3.75000000000000, -36.00000000000000 }    /* For */
,   { 68,  8.36666666666667,  9.36666666666667, -36.75000000000000 }    /* Pyx */
,   { 35,  4.26666666666667,  4.58333333333333, -37.00000000000000 }    /* Eri */
,   { 76, 17.83333333333333, 19.16666666666667, -37.00000000000000 }    /* Sgr */
,   { 65, 21.33333333333333, 23.00000000000000, -37.00000000000000 }    /* PsA */
,   { 70, 23.00000000000000, 23.33333333333333, -37.00000000000000 }    /* Scl */
,   { 36,  3.00000000000000,  3.50000000000000, -39.58333333333334 }    /* For */
,   {  1,  9.36666666666667, 11.00000000000000, -39.75000000000000 }    /* Ant */
,   { 70,  0.00000000000000,  1.66666666666667, -40.00000000000000 }    /* Scl */
,   { 36,  1.66666666666667,  3.00000000000000, -40.00000000000000 }    /* For */
,   { 35,  3.86666666666667,  4.26666666666667, -40.00000000000000 }    /* Eri */
,   { 70, 23.33333333333333, 24.00000000000000, -40.00000000000000 }    /* Scl */
,   { 14, 14.16666666666667, 14.91666666666667, -42.00000000000000 }    /* Cen */
,   { 49, 15.66666666666667, 16.00000000000000, -42.00000000000000 }    /* Lup */
,   { 71, 16.00000000000000, 16.42083333333333, -42.00000000000000 }    /* Sco */
,   {  9,  4.83333333333333,  5.00000000000000, -43.00000000000000 }    /* Cae */
,   { 22,  5.00000000000000,  6.58333333333333, -43.00000000000000 }    /* Col */
,   { 67,  8.00000000000000,  8.36666666666667, -43.00000000000000 }    /* Pup */
,   { 35,  3.41666666666667,  3.86666666666667, -44.00000000000000 }    /* Eri */
,   { 71, 16.42083333333333, 17.83333333333333, -45.50000000000000 }    /* Sco */
,   { 24, 17.83333333333333, 19.16666666666667, -45.50000000000000 }    /* CrA */
,   { 76, 19.16666666666667, 20.33333333333333, -45.50000000000000 }    /* Sgr */
,   { 53, 20.33333333333333, 21.33333333333333, -45.50000000000000 }    /* Mic */
,   { 35,  3.00000000000000,  3.41666666666667, -46.00000000000000 }    /* Eri */
,   {  9,  4.50000000000000,  4.83333333333333, -46.50000000000000 }    /* Cae */
,   { 49, 15.33333333333333, 15.66666666666667, -48.00000000000000 }    /* Lup */
,   { 63,  0.00000000000000,  2.33333333333333, -48.16666666666666 }    /* Phe */
,   { 35,  2.66666666666667,  3.00000000000000, -49.00000000000000 }    /* Eri */
,   { 40,  4.08333333333333,  4.26666666666667, -49.00000000000000 }    /* Hor */
,   {  9,  4.26666666666667,  4.50000000000000, -49.00000000000000 }    /* Cae */
,   { 38, 21.33333333333333, 22.00000000000000, -50.00000000000000 }    /* Gru */
,   { 67,  6.00000000000000,  8.00000000000000, -50.75000000000000 }    /* Pup */
,   { 84,  8.00000000000000,  8.16666666666667, -50.75000000000000 }    /* Vel */
,   { 35,  2.41666666666667,  2.66666666666667, -51.00000000000000 }    /* Eri */
,   { 40,  3.83333333333333,  4.08333333333333, -51.00000000000000 }    /* Hor */
,   { 63,  0.00000000000000,  1.83333333333333, -51.50000000000000 }    /* Phe */
,   { 12,  6.00000000000000,  6.16666666666667, -52.50000000000000 }    /* Car */
,   { 84,  8.16666666666667,  8.45000000000000, -53.00000000000000 }    /* Vel */
,   { 40,  3.50000000000000,  3.83333333333333, -53.16666666666666 }    /* Hor */
,   { 32,  3.83333333333333,  4.00000000000000, -53.16666666666666 }    /* Dor */
,   { 63,  0.00000000000000,  1.58333333333333, -53.50000000000000 }    /* Phe */
,   { 35,  2.16666666666667,  2.41666666666667, -54.00000000000000 }    /* Eri */
,   { 64,  4.50000000000000,  5.00000000000000, -54.00000000000000 }    /* Pic */
,   { 49, 15.05000000000000, 15.33333333333333, -54.00000000000000 }    /* Lup */
,   { 84,  8.45000000000000,  8.83333333333333, -54.50000000000000 }    /* Vel */
,   { 12,  6.16666666666667,  6.50000000000000, -55.00000000000000 }    /* Car */
,   { 14, 11.83333333333333, 12.83333333333333, -55.00000000000000 }    /* Cen */
,   { 49, 14.16666666666667, 15.05000000000000, -55.00000000000000 }    /* Lup */
,   { 56, 15.05000000000000, 15.33333333333333, -55.00000000000000 }    /* Nor */
,   { 32,  4.00000000000000,  4.33333333333333, -56.50000000000000 }    /* Dor */
,   { 84,  8.83333333333333, 11.00000000000000, -56.50000000000000 }    /* Vel */
,   { 14, 11.00000000000000, 11.25000000000000, -56.50000000000000 }    /* Cen */
,   {  5, 17.50000000000000, 18.00000000000000, -57.00000000000000 }    /* Ara */
,   { 78, 18.00000000000000, 20.33333333333333, -57.00000000000000 }    /* Tel */
,   { 38, 22.00000000000000, 23.33333333333333, -57.00000000000000 }    /* Gru */
,   { 40,  3.20000000000000,  3.50000000000000, -57.50000000000000 }    /* Hor */
,   { 64,  5.00000000000000,  5.50000000000000, -57.50000000000000 }    /* Pic */
,   { 12,  6.50000000000000,  6.83333333333333, -58.00000000000000 }    /* Car */
,   { 63,  0.00000000000000,  1.33333333333333, -58.50000000000000 }    /* Phe */
,   { 35,  1.33333333333333,  2.16666666666667, -58.50000000000000 }    /* Eri */
,   { 63, 23.33333333333333, 24.00000000000000, -58.50000000000000 }    /* Phe */
,   { 32,  4.33333333333333,  4.58333333333333, -59.00000000000000 }    /* Dor */
,   { 56, 15.33333333333333, 16.42083333333333, -60.00000000000000 }    /* Nor */
,   { 43, 20.33333333333333, 21.33333333333333, -60.00000000000000 }    /* Ind */
,   { 64,  5.50000000000000,  6.00000000000000, -61.00000000000000 }    /* Pic */
,   { 18, 15.16666666666667, 15.33333333333333, -61.00000000000000 }    /* Cir */
,   {  5, 16.42083333333333, 16.58333333333333, -61.00000000000000 }    /* Ara */
,   { 18, 14.91666666666667, 15.16666666666667, -63.58333333333334 }    /* Cir */
,   {  5, 16.58333333333333, 16.75000000000000, -63.58333333333334 }    /* Ara */
,   { 64,  6.00000000000000,  6.83333333333333, -64.00000000000000 }    /* Pic */
,   { 12,  6.83333333333333,  9.03333333333333, -64.00000000000000 }    /* Car */
,   { 14, 11.25000000000000, 11.83333333333333, -64.00000000000000 }    /* Cen */
,   { 27, 11.83333333333333, 12.83333333333333, -64.00000000000000 }    /* Cru */
,   { 14, 12.83333333333333, 14.53333333333333, -64.00000000000000 }    /* Cen */
,   { 18, 13.50000000000000, 13.66666666666667, -65.00000000000000 }    /* Cir */
,   {  5, 16.75000000000000, 16.83333333333333, -65.00000000000000 }    /* Ara */
,   { 40,  2.16666666666667,  3.20000000000000, -67.50000000000000 }    /* Hor */
,   { 69,  3.20000000000000,  4.58333333333333, -67.50000000000000 }    /* Ret */
,   { 18, 14.75000000000000, 14.91666666666667, -67.50000000000000 }    /* Cir */
,   {  5, 16.83333333333333, 17.50000000000000, -67.50000000000000 }    /* Ara */
,   { 60, 17.50000000000000, 18.00000000000000, -67.50000000000000 }    /* Pav */
,   { 81, 22.00000000000000, 23.33333333333333, -67.50000000000000 }    /* Tuc */
,   { 32,  4.58333333333333,  6.58333333333333, -70.00000000000000 }    /* Dor */
,   { 18, 13.66666666666667, 14.75000000000000, -70.00000000000000 }    /* Cir */
,   { 79, 14.75000000000000, 17.00000000000000, -70.00000000000000 }    /* TrA */
,   { 81,  0.00000000000000,  1.33333333333333, -75.00000000000000 }    /* Tuc */
,   { 42,  3.50000000000000,  4.58333333333333, -75.00000000000000 }    /* Hyi */
,   { 86,  6.58333333333333,  9.03333333333333, -75.00000000000000 }    /* Vol */
,   { 12,  9.03333333333333, 11.25000000000000, -75.00000000000000 }    /* Car */
,   { 55, 11.25000000000000, 13.66666666666667, -75.00000000000000 }    /* Mus */
,   { 60, 18.00000000000000, 21.33333333333333, -75.00000000000000 }    /* Pav */
,   { 43, 21.33333333333333, 23.33333333333333, -75.00000000000000 }    /* Ind */
,   { 81, 23.33333333333333, 24.00000000000000, -75.00000000000000 }    /* Tuc */
,   { 81,  0.75000000000000,  1.33333333333333, -76.00000000000000 }    /* Tuc */
,   { 42,  0.00000000000000,  3.50000000000000, -82.50000000000000 }    /* Hyi */
,   { 17,  7.66666666666667, 13.66666666666667, -82.50000000000000 }    /* Cha */
,   {  2, 13.66666666666667, 18.00000000000000, -82.50000000000000 }    /* Aps */
,   { 52,  3.50000000000000,  7.66666666666667, -85.00000000000000 }    /* Men */
,   { 57,  0.00000000000000, 24.00000000000000, -90.00000000000000 }    /* Oct */
};

#define NUM_CONSTEL_BOUNDARIES  357



/**
 * @brief
 *      Determines the constellation that contains the given point in the sky.
 *
 * Given J2000 equatorial (EQJ) coordinates of a point in the sky, determines the
 * constellation that contains that point.
 *
 * @param ra
 *      The right ascension (RA) of a point in the sky, using the J2000 equatorial system.
 *
 * @param dec
 *      The declination (DEC) of a point in the sky, using the J2000 equatorial system.
 *
 * @return
 *      If successful, `status` holds `ASTRO_SUCCESS`,
 *      `symbol` holds a pointer to a 3-character string like "Ori", and
 *      `name` holds a pointer to the full constellation name like "Orion".
 */
astro_constellation_t Astronomy_Constellation(double ra, double dec)
{
    static astro_time_t epoch2000;
    static astro_rotation_t rot = { ASTRO_NOT_INITIALIZED };
    astro_constellation_t constel;
    astro_equatorial_t j2000, b1875;
    astro_vector_t vec2000, vec1875;
    int i, c;

    if (dec < -90.0 || dec > +90.0)
        return ConstelErr(ASTRO_INVALID_PARAMETER);

    /* Allow right ascension to "wrap around". Clamp to [0, 24) sidereal hours. */
    ra = fmod(ra, 24.0);
    if (ra < 0.0)
        ra += 24.0;

    /* Lazy-initialize the rotation matrix for converting J2000 to B1875. */
    if (rot.status != ASTRO_SUCCESS)
    {
        /*
            Need to calculate the B1875 epoch. Based on this:
            https://en.wikipedia.org/wiki/Epoch_(astronomy)#Besselian_years
            B = 1900 + (JD - 2415020.31352) / 365.242198781
            I'm interested in using TT instead of JD, giving:
            B = 1900 + ((TT+2451545) - 2415020.31352) / 365.242198781
            B = 1900 + (TT + 36524.68648) / 365.242198781
            TT = 365.242198781*(B - 1900) - 36524.68648 = -45655.741449525
            But Astronomy_TimeFromDays() wants UT, not TT.
            Near that date, I get a historical correction of ut-tt = 3.2 seconds.
            That gives UT = -45655.74141261017 for the B1875 epoch,
            or 1874-12-31T18:12:21.950Z.
        */
        astro_time_t time = Astronomy_TimeFromDays(-45655.74141261017);
        rot = Astronomy_Rotation_EQJ_EQD(time);
        if (rot.status != ASTRO_SUCCESS)
            return ConstelErr(rot.status);

        epoch2000 = Astronomy_TimeFromDays(0.0);
    }

    /* Convert coordinates from J2000 to year 1875. */
    j2000.status = ASTRO_SUCCESS;
    j2000.ra = ra;
    j2000.dec = dec;
    j2000.dist = 1.0;
    vec2000 = Astronomy_VectorFromEquator(j2000, epoch2000);
    if (vec2000.status != ASTRO_SUCCESS)
        return ConstelErr(vec2000.status);

    vec1875 = Astronomy_RotateVector(rot, vec2000);
    if (vec1875.status != ASTRO_SUCCESS)
        return ConstelErr(vec1875.status);

    b1875 = Astronomy_EquatorFromVector(vec1875);
    if (b1875.status != ASTRO_SUCCESS)
        return ConstelErr(b1875.status);

    /* Search for the constellation using the B1875 coordinates. */
    c = -1;     /* constellation not (yet) found */
    for (i=0; i < NUM_CONSTEL_BOUNDARIES; ++i)
    {
        const constel_boundary_t *b = &ConstelBounds[i];
        if ((b->dec_lo <= b1875.dec) && (b->ra_hi > b1875.ra) && (b->ra_lo <= b1875.ra))
        {
            c = b->index;
            break;
        }
    }

    if (c < 0 || c >= NUM_CONSTELLATIONS)
        return ConstelErr(ASTRO_INTERNAL_ERROR);    /* should have been able to find the constellation */

    constel.status = ASTRO_SUCCESS;
    constel.symbol = ConstelInfo[c].symbol;
    constel.name = ConstelInfo[c].name;
    constel.ra_1875 = b1875.ra;
    constel.dec_1875 = b1875.dec;
    return constel;
}


static astro_lunar_eclipse_t LunarEclipseError(astro_status_t status)
{
    astro_lunar_eclipse_t eclipse;
    eclipse.status = status;
    eclipse.kind = ECLIPSE_NONE;
    eclipse.peak = TimeError();
    eclipse.sd_penum = eclipse.sd_partial = eclipse.sd_total = NAN;
    return eclipse;
}


/** @cond DOXYGEN_SKIP */
typedef struct
{
    astro_status_t status;
    astro_time_t time;
    double  u;              /* dot product of (heliocentric earth) and (geocentric moon): defines the shadow plane where the Moon is */
    double  r;              /* km distance between center of Moon/Earth (shaded body) and the line passing through the centers of the Sun and Earth/Moon (casting body). */
    double  k;              /* umbra radius in km, at the shadow plane */
    double  p;              /* penumbra radius in km, at the shadow plane */
    astro_vector_t target;  /* coordinates of target body relative to shadow-casting body at 'time' */
    astro_vector_t dir;     /* heliocentric coordinates of shadow-casting body at 'time' */
}
shadow_t;               /* Represents alignment of the Moon/Earth with the Earth's/Moon's shadow, for finding eclipses. */

typedef struct
{
    double radius_limit;
    double direction;
}
shadow_context_t;
/** @endcond */


static shadow_t ShadowError(astro_status_t status)
{
    shadow_t shadow;
    memset(&shadow, 0, sizeof(shadow));
    shadow.status = status;
    return shadow;
}


static shadow_t CalcShadow(
    double body_radius_km,
    astro_time_t time,
    astro_vector_t target,
    astro_vector_t dir)
{
    double dx, dy, dz;
    shadow_t shadow;

    shadow.target = target;
    shadow.dir = dir;

    shadow.u = (dir.x*target.x + dir.y*target.y + dir.z*target.z) / (dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);

    dx = (shadow.u * dir.x) - target.x;
    dy = (shadow.u * dir.y) - target.y;
    dz = (shadow.u * dir.z) - target.z;
    shadow.r = KM_PER_AU * sqrt(dx*dx + dy*dy + dz*dz);

    shadow.k = +SUN_RADIUS_KM - (1.0 + shadow.u)*(SUN_RADIUS_KM - body_radius_km);
    shadow.p = -SUN_RADIUS_KM + (1.0 + shadow.u)*(SUN_RADIUS_KM + body_radius_km);
    shadow.status = ASTRO_SUCCESS;
    shadow.time = time;

    return shadow;
}


static shadow_t PlanetShadow(astro_body_t body, double planet_radius_km, astro_time_t time)
{
    astro_vector_t e, p, g;

    /* Calculate light-travel-corrected vector from Earth to planet. */
    g = Astronomy_GeoVector(body, time, NO_ABERRATION);
    if (g.status != ASTRO_SUCCESS)
        return ShadowError(g.status);

    /* Calculate light-travel-corrected vector from Earth to Sun. */
    e = Astronomy_GeoVector(BODY_SUN, time, NO_ABERRATION);
    if (e.status != ASTRO_SUCCESS)
        return ShadowError(e.status);

    /* Deduce light-travel-corrected vector from Sun to planet. */
    p.t = time;
    p.x = g.x - e.x;
    p.y = g.y - e.y;
    p.z = g.z - e.z;

    /* Calcluate Earth's position from the planet's point of view. */
    e.x = -g.x;
    e.y = -g.y;
    e.z = -g.z;

    return CalcShadow(planet_radius_km, time, e, p);
}


static shadow_t EarthShadow(astro_time_t time)
{
    /* This function helps find when the Earth's shadow falls upon the Moon. */
    astro_vector_t e, m;

    e = CalcEarth(time);            /* This function never fails; no need to check return value */
    m = Astronomy_GeoMoon(time);    /* This function never fails; no need to check return value */

    return CalcShadow(EARTH_ECLIPSE_RADIUS_KM, time, m, e);
}


static shadow_t MoonShadow(astro_time_t time)
{
    /* This function helps find when the Moon's shadow falls upon the Earth. */

    astro_vector_t h, e, m;

    /*
        This is a variation on the logic in EarthShadow().
        Instead of a heliocentric Earth and a geocentric Moon,
        we want a heliocentric Moon and a lunacentric Earth.
    */

    h = CalcEarth(time);            /* heliocentric Earth */
    m = Astronomy_GeoMoon(time);    /* geocentric Moon */

    /* Calculate lunacentric Earth. */
    e.status = m.status;
    e.x = -m.x;
    e.y = -m.y;
    e.z = -m.z;
    e.t = m.t;

    /* Convert geocentric moon to heliocentric Moon. */
    m.x += h.x;
    m.y += h.y;
    m.z += h.z;

    return CalcShadow(MOON_MEAN_RADIUS_KM, time, e, m);
}


/** @cond DOXYGEN_SKIP */
typedef shadow_t (* shadow_func_t) (astro_time_t time);
/** @endcond */


static astro_func_result_t shadow_distance_slope(void *context, astro_time_t time)
{
    const double dt = 1.0 / 86400.0;
    astro_time_t t1, t2;
    astro_func_result_t result;
    shadow_t shadow1, shadow2;
    shadow_func_t shadowfunc = context;

    t1 = Astronomy_AddDays(time, -dt);
    t2 = Astronomy_AddDays(time, +dt);

    shadow1 = shadowfunc(t1);
    if (shadow1.status != ASTRO_SUCCESS)
        return FuncError(shadow1.status);

    shadow2 = shadowfunc(t2);
    if (shadow2.status != ASTRO_SUCCESS)
        return FuncError(shadow2.status);

    result.value = (shadow2.r - shadow1.r) / dt;
    result.status = ASTRO_SUCCESS;
    return result;
}


static shadow_t PeakEarthShadow(astro_time_t search_center_time)
{
    /* Search for when the Earth's shadow axis is closest to the center of the Moon. */

    astro_time_t t1, t2;
    astro_search_result_t result;
    const double window = 0.03;        /* days before/after full moon to search for minimum shadow distance */

    t1 = Astronomy_AddDays(search_center_time, -window);
    t2 = Astronomy_AddDays(search_center_time, +window);

    result = Astronomy_Search(shadow_distance_slope, EarthShadow, t1, t2, 1.0);
    if (result.status != ASTRO_SUCCESS)
        return ShadowError(result.status);

    return EarthShadow(result.time);
}


static shadow_t PeakMoonShadow(astro_time_t search_center_time)
{
    /* Search for when the Moon's shadow axis is closest to the center of the Earth. */

    astro_time_t t1, t2;
    astro_search_result_t result;
    const double window = 0.03;     /* days before/after new moon to search for minimum shadow distance */

    t1 = Astronomy_AddDays(search_center_time, -window);
    t2 = Astronomy_AddDays(search_center_time, +window);

    result = Astronomy_Search(shadow_distance_slope, MoonShadow, t1, t2, 1.0);
    if (result.status != ASTRO_SUCCESS)
        return ShadowError(result.status);

    return MoonShadow(result.time);
}


/** @cond DOXYGEN_SKIP */
typedef struct
{
    astro_body_t    body;
    double          planet_radius_km;
    double          direction;          /* used for transit start/finish search only */
}
planet_shadow_context_t;
/** @endcond */


static astro_func_result_t planet_shadow_distance_slope(void *context, astro_time_t time)
{
    const double dt = 1.0 / 86400.0;
    astro_time_t t1, t2;
    astro_func_result_t result;
    shadow_t shadow1, shadow2;
    const planet_shadow_context_t *p = context;

    t1 = Astronomy_AddDays(time, -dt);
    t2 = Astronomy_AddDays(time, +dt);

    shadow1 = PlanetShadow(p->body, p->planet_radius_km, t1);
    if (shadow1.status != ASTRO_SUCCESS)
        return FuncError(shadow1.status);

    shadow2 = PlanetShadow(p->body, p->planet_radius_km, t2);
    if (shadow2.status != ASTRO_SUCCESS)
        return FuncError(shadow2.status);

    result.value = (shadow2.r - shadow1.r) / dt;
    result.status = ASTRO_SUCCESS;
    return result;
}


static shadow_t PeakPlanetShadow(astro_body_t body, double planet_radius_km, astro_time_t search_center_time)
{
    /* Search for when the body's shadow is closest to the center of the Earth. */

    astro_time_t t1, t2;
    astro_search_result_t result;
    planet_shadow_context_t context;
    const double window = 1.0;     /* days before/after inferior conjunction to search for minimum shadow distance */

    t1 = Astronomy_AddDays(search_center_time, -window);
    t2 = Astronomy_AddDays(search_center_time, +window);

    context.body = body;
    context.planet_radius_km = planet_radius_km;
    context.direction = 0.0;    /* not used in this search */

    result = Astronomy_Search(planet_shadow_distance_slope, &context, t1, t2, 1.0);
    if (result.status != ASTRO_SUCCESS)
        return ShadowError(result.status);

    return PlanetShadow(body, planet_radius_km, result.time);
}


static astro_func_result_t shadow_distance(void *context, astro_time_t time)
{
    astro_func_result_t result;
    const shadow_context_t *p = context;
    shadow_t shadow = EarthShadow(time);
    if (shadow.status != ASTRO_SUCCESS)
        return FuncError(shadow.status);

    result.value = p->direction * (shadow.r - p->radius_limit);
    result.status = ASTRO_SUCCESS;
    return result;
}


static double ShadowSemiDurationMinutes(astro_time_t center_time, double radius_limit, double window_minutes)
{
    /* Search backwards and forwards from the center time until shadow axis distance crosses radius limit. */
    double window = window_minutes / (24.0 * 60.0);
    shadow_context_t context;
    astro_search_result_t s1, s2;
    astro_time_t before, after;

    before = Astronomy_AddDays(center_time, -window);
    after  = Astronomy_AddDays(center_time, +window);

    context.radius_limit = radius_limit;
    context.direction = -1.0;
    s1 = Astronomy_Search(shadow_distance, &context, before, center_time, 1.0);

    context.direction = +1.0;
    s2 = Astronomy_Search(shadow_distance, &context, center_time, after, 1.0);

    if (s1.status != ASTRO_SUCCESS || s2.status != ASTRO_SUCCESS)
        return -1.0;    /* something went wrong! */

    return (s2.time.ut - s1.time.ut) * ((24.0 * 60.0) / 2.0);       /* convert days to minutes and average the semi-durations. */
}


/**
 * @brief Searches for a lunar eclipse.
 *
 * This function finds the first lunar eclipse that occurs after `startTime`.
 * A lunar eclipse may be penumbral, partial, or total.
 * See #astro_lunar_eclipse_t for more information.
 * To find a series of lunar eclipses, call this function once,
 * then keep calling #Astronomy_NextLunarEclipse as many times as desired,
 * passing in the `peak` value returned from the previous call.
 *
 * @param startTime
 *      The date and time for starting the search for a lunar eclipse.
 *
 * @return
 *      If successful, the `status` field in the returned structure will contain `ASTRO_SUCCESS`
 *      and the remaining structure fields will be valid.
 *      Any other value indicates an error.
 */
astro_lunar_eclipse_t Astronomy_SearchLunarEclipse(astro_time_t startTime)
{
    const double PruneLatitude = 1.8;   /* full Moon's ecliptic latitude above which eclipse is impossible */
    astro_time_t fmtime;
    astro_lunar_eclipse_t eclipse;
    astro_search_result_t fullmoon;
    shadow_t shadow;
    int fmcount;
    double eclip_lat, eclip_lon, distance;

    /* Iterate through consecutive full moons until we find any kind of lunar eclipse. */
    fmtime = startTime;
    for (fmcount=0; fmcount < 12; ++fmcount)
    {
        /* Search for the next full moon. Any eclipse will be near it. */
        fullmoon = Astronomy_SearchMoonPhase(180.0, fmtime, 40.0);
        if (fullmoon.status != ASTRO_SUCCESS)
            return LunarEclipseError(fullmoon.status);

        /* Pruning: if the full Moon's ecliptic latitude is too large, a lunar eclipse is not possible. */
        CalcMoon(fullmoon.time.tt / 36525.0, &eclip_lon, &eclip_lat, &distance);
        if (RAD2DEG * fabs(eclip_lat) < PruneLatitude)
        {
            /* Search near the full moon for the time when the center of the Moon */
            /* is closest to the line passing through the centers of the Sun and Earth. */
            shadow = PeakEarthShadow(fullmoon.time);
            if (shadow.status != ASTRO_SUCCESS)
                return LunarEclipseError(shadow.status);

            if (shadow.r < shadow.p + MOON_MEAN_RADIUS_KM)
            {
                /* This is at least a penumbral eclipse. We will return a result. */
                eclipse.status = ASTRO_SUCCESS;
                eclipse.kind = ECLIPSE_PENUMBRAL;
                eclipse.peak = shadow.time;
                eclipse.sd_total = 0.0;
                eclipse.sd_partial = 0.0;
                eclipse.sd_penum = ShadowSemiDurationMinutes(shadow.time, shadow.p + MOON_MEAN_RADIUS_KM, 200.0);
                if (eclipse.sd_penum <= 0.0)
                    return LunarEclipseError(ASTRO_SEARCH_FAILURE);

                if (shadow.r < shadow.k + MOON_MEAN_RADIUS_KM)
                {
                    /* This is at least a partial eclipse. */
                    eclipse.kind = ECLIPSE_PARTIAL;
                    eclipse.sd_partial = ShadowSemiDurationMinutes(shadow.time, shadow.k + MOON_MEAN_RADIUS_KM, eclipse.sd_penum);
                    if (eclipse.sd_partial <= 0.0)
                        return LunarEclipseError(ASTRO_SEARCH_FAILURE);

                    if (shadow.r + MOON_MEAN_RADIUS_KM < shadow.k)
                    {
                        /* This is a total eclipse. */
                        eclipse.kind = ECLIPSE_TOTAL;
                        eclipse.sd_total = ShadowSemiDurationMinutes(shadow.time, shadow.k - MOON_MEAN_RADIUS_KM, eclipse.sd_partial);
                        if (eclipse.sd_total <= 0.0)
                            return LunarEclipseError(ASTRO_SEARCH_FAILURE);
                    }
                }
                return eclipse;
            }
        }

        /* We didn't find an eclipse on this full moon, so search for the next one. */
        fmtime = Astronomy_AddDays(fullmoon.time, 10.0);
    }

    /* Safety valve to prevent infinite loop. */
    /* This should never happen, because at least 2 lunar eclipses happen per year. */
    return LunarEclipseError(ASTRO_INTERNAL_ERROR);
}

/**
 * @brief Searches for the next lunar eclipse in a series.
 *
 * After using #Astronomy_SearchLunarEclipse to find the first lunar eclipse
 * in a series, you can call this function to find the next consecutive lunar eclipse.
 * Pass in the `peak` value from the #astro_lunar_eclipse_t returned by the
 * previous call to `Astronomy_SearchLunarEclipse` or `Astronomy_NextLunarEclipse`
 * to find the next lunar eclipse.
 *
 * @param prevEclipseTime
 *      A date and time near a full moon. Lunar eclipse search will start at the next full moon.
 *
 * @return
 *      If successful, the `status` field in the returned structure will contain `ASTRO_SUCCESS`
 *      and the remaining structure fields will be valid.
 *      Any other value indicates an error.
 */
astro_lunar_eclipse_t Astronomy_NextLunarEclipse(astro_time_t prevEclipseTime)
{
    astro_time_t startTime = Astronomy_AddDays(prevEclipseTime, 10.0);
    return Astronomy_SearchLunarEclipse(startTime);
}


static astro_global_solar_eclipse_t GlobalSolarEclipseError(astro_status_t status)
{
    astro_global_solar_eclipse_t eclipse;

    eclipse.status = status;
    eclipse.kind = ECLIPSE_NONE;
    eclipse.peak = TimeError();
    eclipse.distance = eclipse.latitude = eclipse.longitude = NAN;

    return eclipse;
}

/* The umbra radius tells us what kind of eclipse the observer sees. */
/* If the umbra radius is positive, this is a total eclipse. Otherwise, it's annular. */
/* HACK: I added a tiny bias (14 meters) to match Espenak test data. */
#define EclipseKindFromUmbra(k)     (((k) > 0.014) ? ECLIPSE_TOTAL : ECLIPSE_ANNULAR)

static astro_global_solar_eclipse_t GeoidIntersect(shadow_t shadow)
{
    astro_global_solar_eclipse_t eclipse;
    astro_rotation_t rot, inv;
    astro_vector_t v, e, o;
    shadow_t surface;
    double A, B, C, radic, u, R;
    double px, py, pz, proj;
    double gast;

    eclipse.status = ASTRO_SUCCESS;
    eclipse.kind = ECLIPSE_PARTIAL;
    eclipse.peak = shadow.time;
    eclipse.distance = shadow.r;
    eclipse.latitude = eclipse.longitude = NAN;

    /*
        We want to calculate the intersection of the shadow axis with the Earth's geoid.
        First we must convert EQJ (equator of J2000) coordinates to EQD (equator of date)
        coordinates that are perfectly aligned with the Earth's equator at this
        moment in time.
    */
    rot = Astronomy_Rotation_EQJ_EQD(shadow.time);
    if (rot.status != ASTRO_SUCCESS)
        return GlobalSolarEclipseError(rot.status);

    v = Astronomy_RotateVector(rot, shadow.dir);        /* shadow-axis vector in equator-of-date coordinates */
    if (v.status != ASTRO_SUCCESS)
        return GlobalSolarEclipseError(v.status);

    e = Astronomy_RotateVector(rot, shadow.target);     /* lunacentric Earth in equator-of-date coordinates */
    if (e.status != ASTRO_SUCCESS)
        return GlobalSolarEclipseError(e.status);

    /*
        Convert all distances from AU to km.
        But dilate the z-coordinates so that the Earth becomes a perfect sphere.
        Then find the intersection of the vector with the sphere.
        See p 184 in Montenbruck & Pfleger's "Astronomy on the Personal Computer", second edition.
    */
    v.x *= KM_PER_AU;
    v.y *= KM_PER_AU;
    v.z *= KM_PER_AU / EARTH_FLATTENING;

    e.x *= KM_PER_AU;
    e.y *= KM_PER_AU;
    e.z *= KM_PER_AU / EARTH_FLATTENING;

    /*
        Solve the quadratic equation that finds whether and where
        the shadow axis intersects with the Earth in the dilated coordinate system.
    */
    R = EARTH_EQUATORIAL_RADIUS_KM;
    A = v.x*v.x + v.y*v.y + v.z*v.z;
    B = -2.0 * (v.x*e.x + v.y*e.y + v.z*e.z);
    C = (e.x*e.x + e.y*e.y + e.z*e.z) - R*R;
    radic = B*B - 4*A*C;

    if (radic > 0.0)
    {
        /* Calculate the closer of the two intersection points. */
        /* This will be on the day side of the Earth. */
        u = (-B - sqrt(radic)) / (2 * A);

        /* Convert lunacentric dilated coordinates to geocentric coordinates. */
        px = u*v.x - e.x;
        py = u*v.y - e.y;
        pz = (u*v.z - e.z) * EARTH_FLATTENING;

        /* Convert cartesian coordinates into geodetic latitude/longitude. */
        proj = sqrt(px*px + py*py) * (EARTH_FLATTENING * EARTH_FLATTENING);
        if (proj == 0.0)
            eclipse.latitude = (pz > 0.0) ? +90.0 : -90.0;
        else
            eclipse.latitude = RAD2DEG * atan(pz / proj);

        /* Adjust longitude for Earth's rotation at the given UT. */
        gast = sidereal_time(&eclipse.peak);
        eclipse.longitude = fmod((RAD2DEG*atan2(py, px)) - (15*gast), 360.0);
        if (eclipse.longitude <= -180.0)
            eclipse.longitude += 360.0;
        else if (eclipse.longitude > +180.0)
            eclipse.longitude -= 360.0;

        /* We want to determine whether the observer sees a total eclipse or an annular eclipse. */
        /* We need to perform a series of vector calculations... */
        /* Calculate the inverse rotation matrix, so we can convert EQD to EQJ. */
        inv = Astronomy_InverseRotation(rot);
        if (inv.status != ASTRO_SUCCESS)
            return GlobalSolarEclipseError(inv.status);

        /* Put the EQD geocentric coordinates of the observer into the vector 'o'. */
        /* Also convert back from kilometers to astronomical units. */
        o.status = ASTRO_SUCCESS;
        o.t = shadow.time;
        o.x = px / KM_PER_AU;
        o.y = py / KM_PER_AU;
        o.z = pz / KM_PER_AU;

        /* Rotate the observer's geocentric EQD back to the EQJ system. */
        o = Astronomy_RotateVector(inv, o);

        /* Convert geocentric vector to lunacentric vector. */
        o.x += shadow.target.x;
        o.y += shadow.target.y;
        o.z += shadow.target.z;

        /* Recalculate the shadow using a vector from the Moon's center toward the observer. */
        surface = CalcShadow(MOON_POLAR_RADIUS_KM, shadow.time, o, shadow.dir);

        /* If we did everything right, the shadow distance should be very close to zero. */
        /* That's because we already determined the observer 'o' is on the shadow axis! */
        if (surface.r > 1.0e-9 || surface.r < 0.0)
            return GlobalSolarEclipseError(ASTRO_INTERNAL_ERROR);

        eclipse.kind = EclipseKindFromUmbra(surface.k);
    }

    return eclipse;
}


/**
 * @brief Searches for a solar eclipse visible anywhere on the Earth's surface.
 *
 * This function finds the first solar eclipse that occurs after `startTime`.
 * A solar eclipse may be partial, annular, or total.
 * See #astro_global_solar_eclipse_t for more information.
 * To find a series of solar eclipses, call this function once,
 * then keep calling #Astronomy_NextGlobalSolarEclipse as many times as desired,
 * passing in the `peak` value returned from the previous call.
 *
 * @param startTime
 *      The date and time for starting the search for a solar eclipse.
 *
 * @return
 *      If successful, the `status` field in the returned structure will contain `ASTRO_SUCCESS`
 *      and the remaining structure fields are as described in #astro_global_solar_eclipse_t.
 *      Any other value indicates an error.
 */
astro_global_solar_eclipse_t Astronomy_SearchGlobalSolarEclipse(astro_time_t startTime)
{
    const double PruneLatitude = 1.8;   /* Moon's ecliptic latitude beyond which eclipse is impossible */
    astro_time_t nmtime;
    astro_search_result_t newmoon;
    shadow_t shadow;
    int nmcount;
    double eclip_lat, eclip_lon, distance;

    /* Iterate through consecutive new moons until we find a solar eclipse visible somewhere on Earth. */
    nmtime = startTime;
    for (nmcount=0; nmcount < 12; ++nmcount)
    {
        /* Search for the next new moon. Any eclipse will be near it. */
        newmoon = Astronomy_SearchMoonPhase(0.0, nmtime, 40.0);
        if (newmoon.status != ASTRO_SUCCESS)
            return GlobalSolarEclipseError(newmoon.status);

        /* Pruning: if the new moon's ecliptic latitude is too large, a solar eclipse is not possible. */
        CalcMoon(newmoon.time.tt / 36525.0, &eclip_lon, &eclip_lat, &distance);
        if (RAD2DEG * fabs(eclip_lat) < PruneLatitude)
        {
            /* Search near the new moon for the time when the center of the Earth */
            /* is closest to the line passing through the centers of the Sun and Moon. */
            shadow = PeakMoonShadow(newmoon.time);
            if (shadow.status != ASTRO_SUCCESS)
                return GlobalSolarEclipseError(shadow.status);

            if (shadow.r < shadow.p + EARTH_MEAN_RADIUS_KM)
            {
                /* This is at least a partial solar eclipse visible somewhere on Earth. */
                /* Try to find an intersection between the shadow axis and the Earth's oblate geoid. */
                return GeoidIntersect(shadow);
            }
        }

        /* We didn't find an eclipse on this new moon, so search for the next one. */
        nmtime = Astronomy_AddDays(newmoon.time, 10.0);
    }

    /* Safety valve to prevent infinite loop. */
    /* This should never happen, because at least 2 solar eclipses happen per year. */
    return GlobalSolarEclipseError(ASTRO_INTERNAL_ERROR);
}


/**
 * @brief Searches for the next global solar eclipse in a series.
 *
 * After using #Astronomy_SearchGlobalSolarEclipse to find the first solar eclipse
 * in a series, you can call this function to find the next consecutive solar eclipse.
 * Pass in the `peak` value from the #astro_global_solar_eclipse_t returned by the
 * previous call to `Astronomy_SearchGlobalSolarEclipse` or `Astronomy_NextGlobalSolarEclipse`
 * to find the next solar eclipse.
 *
 * @param prevEclipseTime
 *      A date and time near a new moon. Solar eclipse search will start at the next new moon.
 *
 * @return
 *      If successful, the `status` field in the returned structure will contain `ASTRO_SUCCESS`
 *      and the remaining structure fields are as described in #astro_global_solar_eclipse_t.
 *      Any other value indicates an error.
 */
astro_global_solar_eclipse_t Astronomy_NextGlobalSolarEclipse(astro_time_t prevEclipseTime)
{
    astro_time_t startTime = Astronomy_AddDays(prevEclipseTime, 10.0);
    return Astronomy_SearchGlobalSolarEclipse(startTime);
}


static astro_eclipse_event_t EclipseEventError(void)
{
    astro_eclipse_event_t evt;
    evt.time = TimeError();
    evt.altitude = NAN;
    return evt;
}


static astro_local_solar_eclipse_t LocalSolarEclipseError(astro_status_t status)
{
    astro_local_solar_eclipse_t eclipse;

    eclipse.status = status;
    eclipse.kind = ECLIPSE_NONE;

    eclipse.partial_begin = EclipseEventError();
    eclipse.total_begin   = EclipseEventError();
    eclipse.peak          = EclipseEventError();
    eclipse.total_end     = EclipseEventError();
    eclipse.partial_end   = EclipseEventError();

    return eclipse;
}


static shadow_t LocalMoonShadow(astro_time_t time, astro_observer_t observer)
{
    astro_vector_t h, o, m;
    double pos[3];

    /* Calculate observer's geocentric position. */
    /* For efficiency, do this first, to populate the earth rotation parameters in 'time'. */
    /* That way they can be recycled instead of recalculated. */
    geo_pos(&time, observer, pos);

    h = CalcEarth(time);            /* heliocentric Earth */
    m = Astronomy_GeoMoon(time);    /* geocentric Moon */

    /* Calculate lunacentric location of an observer on the Earth's surface. */
    o.status = m.status;
    o.x = pos[0] - m.x;
    o.y = pos[1] - m.y;
    o.z = pos[2] - m.z;
    o.t = m.t;

    /* Convert geocentric moon to heliocentric Moon. */
    m.x += h.x;
    m.y += h.y;
    m.z += h.z;

    return CalcShadow(MOON_MEAN_RADIUS_KM, time, o, m);
}


static astro_func_result_t local_shadow_distance_slope(void *context, astro_time_t time)
{
    const double dt = 1.0 / 86400.0;
    astro_time_t t1, t2;
    astro_func_result_t result;
    shadow_t shadow1, shadow2;
    const astro_observer_t *observer = context;

    t1 = Astronomy_AddDays(time, -dt);
    t2 = Astronomy_AddDays(time, +dt);

    shadow1 = LocalMoonShadow(t1, *observer);
    if (shadow1.status != ASTRO_SUCCESS)
        return FuncError(shadow1.status);

    shadow2 = LocalMoonShadow(t2, *observer);
    if (shadow2.status != ASTRO_SUCCESS)
        return FuncError(shadow2.status);

    result.value = (shadow2.r - shadow1.r) / dt;
    result.status = ASTRO_SUCCESS;
    return result;
}


static shadow_t PeakLocalMoonShadow(astro_time_t search_center_time, astro_observer_t observer)
{
    astro_time_t t1, t2;
    astro_search_result_t result;
    const double window = 0.2;

    /*
        Search for the time near search_center_time that the Moon's shadow comes
        closest to the given observer.
    */

    t1 = Astronomy_AddDays(search_center_time, -window);
    t2 = Astronomy_AddDays(search_center_time, +window);

    result = Astronomy_Search(local_shadow_distance_slope, &observer, t1, t2, 1.0);
    if (result.status != ASTRO_SUCCESS)
        return ShadowError(result.status);

    return LocalMoonShadow(result.time, observer);
}


static double local_partial_distance(const shadow_t *shadow)
{
    return shadow->p - shadow->r;
}

static double local_total_distance(const shadow_t *shadow)
{
    /* Must take the absolute value of the umbra radius 'k' */
    /* because it can be negative for an annular eclipse. */
    return fabs(shadow->k) - shadow->r;
}

/** @cond DOXYGEN_SKIP */
typedef double (* local_distance_func) (const shadow_t *shadow);

typedef struct
{
    local_distance_func     func;
    double                  direction;
    astro_observer_t        observer;
}
eclipse_transition_t;
/* @endcond */


static astro_func_result_t local_eclipse_func(void *context, astro_time_t time)
{
    const eclipse_transition_t *trans = context;
    shadow_t shadow;
    astro_func_result_t result;

    shadow = LocalMoonShadow(time, trans->observer);
    if (shadow.status != ASTRO_SUCCESS)
        return FuncError(shadow.status);

    result.value = trans->direction * trans->func(&shadow);
    result.status = ASTRO_SUCCESS;
    return result;
}


astro_func_result_t SunAltitude(
    astro_time_t time,
    astro_observer_t observer)
{
    astro_equatorial_t equ;
    astro_horizon_t hor;
    astro_func_result_t result;

    equ = Astronomy_Equator(BODY_SUN, &time, observer, EQUATOR_OF_DATE, ABERRATION);
    if (equ.status != ASTRO_SUCCESS)
        return FuncError(equ.status);

    hor = Astronomy_Horizon(&time, observer, equ.ra, equ.dec, REFRACTION_NORMAL);
    result.value = hor.altitude;
    result.status = ASTRO_SUCCESS;
    return result;
}


static astro_status_t CalcEvent(
    astro_observer_t observer,
    astro_time_t time,
    astro_eclipse_event_t *evt)
{
    astro_func_result_t result;

    result = SunAltitude(time, observer);
    if (result.status != ASTRO_SUCCESS)
    {
        evt->time = TimeError();
        evt->altitude = NAN;
        return result.status;
    }

    evt->time = time;
    evt->altitude = result.value;
    return ASTRO_SUCCESS;
}


static astro_status_t LocalEclipseTransition(
    astro_observer_t observer,
    double direction,
    local_distance_func func,
    astro_time_t t1,
    astro_time_t t2,
    astro_eclipse_event_t *evt)
{
    eclipse_transition_t trans;
    astro_search_result_t search;

    trans.func = func;
    trans.direction = direction;
    trans.observer = observer;

    search = Astronomy_Search(local_eclipse_func, &trans, t1, t2, 1.0);
    if (search.status != ASTRO_SUCCESS)
    {
        evt->time = TimeError();
        evt->altitude = NAN;
        return search.status;
    }

    return CalcEvent(observer, search.time, evt);
}


static astro_local_solar_eclipse_t LocalEclipse(
    shadow_t shadow,
    astro_observer_t observer)
{
    const double PARTIAL_WINDOW = 0.2;
    const double TOTAL_WINDOW = 0.01;
    astro_local_solar_eclipse_t eclipse;
    astro_time_t t1, t2;
    astro_status_t status;

    status = CalcEvent(observer, shadow.time, &eclipse.peak);
    if (status != ASTRO_SUCCESS)
        return LocalSolarEclipseError(status);

    t1 = Astronomy_AddDays(shadow.time, -PARTIAL_WINDOW);
    t2 = Astronomy_AddDays(shadow.time, +PARTIAL_WINDOW);

    status = LocalEclipseTransition(observer, +1.0, local_partial_distance, t1, shadow.time, &eclipse.partial_begin);
    if (status != ASTRO_SUCCESS)
        return LocalSolarEclipseError(status);

    status = LocalEclipseTransition(observer, -1.0, local_partial_distance, shadow.time, t2, &eclipse.partial_end);
    if (status != ASTRO_SUCCESS)
        return LocalSolarEclipseError(status);

    if (shadow.r < fabs(shadow.k))      /* take absolute value of 'k' to handle annular eclipses too. */
    {
        t1 = Astronomy_AddDays(shadow.time, -TOTAL_WINDOW);
        t2 = Astronomy_AddDays(shadow.time, +TOTAL_WINDOW);

        status = LocalEclipseTransition(observer, +1.0, local_total_distance, t1, shadow.time, &eclipse.total_begin);
        if (status != ASTRO_SUCCESS)
            return LocalSolarEclipseError(status);

        status = LocalEclipseTransition(observer, -1.0, local_total_distance, shadow.time, t2, &eclipse.total_end);
        if (status != ASTRO_SUCCESS)
            return LocalSolarEclipseError(status);

        eclipse.kind = EclipseKindFromUmbra(shadow.k);
    }
    else
    {
        eclipse.total_begin = eclipse.total_end = EclipseEventError();
        eclipse.kind = ECLIPSE_PARTIAL;
    }

    eclipse.status = ASTRO_SUCCESS;
    return eclipse;
}


/**
 * @brief Searches for a solar eclipse visible at a specific location on the Earth's surface.
 *
 * This function finds the first solar eclipse that occurs after `startTime`.
 * A solar eclipse may be partial, annular, or total.
 * See #astro_local_solar_eclipse_t for more information.
 * To find a series of solar eclipses, call this function once,
 * then keep calling #Astronomy_NextLocalSolarEclipse as many times as desired,
 * passing in the `peak` value returned from the previous call.
 *
 * IMPORTANT: An eclipse reported by this function might be partly or
 * completely invisible to the observer due to the time of day.
 *
 * @param startTime
 *      The date and time for starting the search for a solar eclipse.
 *
 * @param observer
 *      The geographic location of the observer.
 *
 * @return
 *      If successful, the `status` field in the returned structure will contain `ASTRO_SUCCESS`
 *      and the remaining structure fields are as described in #astro_local_solar_eclipse_t.
 *      Any other value indicates an error.
 */
astro_local_solar_eclipse_t Astronomy_SearchLocalSolarEclipse(
    astro_time_t startTime,
    astro_observer_t observer)
{
    const double PruneLatitude = 1.8;   /* Moon's ecliptic latitude beyond which eclipse is impossible */
    astro_time_t nmtime;
    astro_search_result_t newmoon;
    shadow_t shadow;
    double eclip_lat, eclip_lon, distance;
    astro_local_solar_eclipse_t eclipse;

    /* Iterate through consecutive new moons until we find a solar eclipse visible somewhere on Earth. */
    nmtime = startTime;
    for(;;)
    {
        /* Search for the next new moon. Any eclipse will be near it. */
        newmoon = Astronomy_SearchMoonPhase(0.0, nmtime, 40.0);
        if (newmoon.status != ASTRO_SUCCESS)
            return LocalSolarEclipseError(newmoon.status);

        /* Pruning: if the new moon's ecliptic latitude is too large, a solar eclipse is not possible. */
        CalcMoon(newmoon.time.tt / 36525.0, &eclip_lon, &eclip_lat, &distance);
        if (RAD2DEG * fabs(eclip_lat) < PruneLatitude)
        {
            /* Search near the new moon for the time when the observer */
            /* is closest to the line passing through the centers of the Sun and Moon. */
            shadow = PeakLocalMoonShadow(newmoon.time, observer);
            if (shadow.status != ASTRO_SUCCESS)
                return LocalSolarEclipseError(shadow.status);

            if (shadow.r < shadow.p)
            {
                /* This is at least a partial solar eclipse for the observer. */
                eclipse = LocalEclipse(shadow, observer);

                /* If any error occurs, something is really wrong and we should bail out. */
                if (eclipse.status != ASTRO_SUCCESS)
                    return eclipse;

                /* Ignore any eclipse that happens completely at night. */
                /* More precisely, the center of the Sun must be above the horizon */
                /* at the beginning or the end of the eclipse, or we skip the event. */
                if (eclipse.partial_begin.altitude > 0.0 || eclipse.partial_end.altitude > 0.0)
                    return eclipse;
            }
        }

        /* We didn't find an eclipse on this new moon, so search for the next one. */
        nmtime = Astronomy_AddDays(newmoon.time, 10.0);
    }
}


/**
 * @brief Searches for the next local solar eclipse in a series.
 *
 * After using #Astronomy_SearchLocalSolarEclipse to find the first solar eclipse
 * in a series, you can call this function to find the next consecutive solar eclipse.
 * Pass in the `peak` value from the #astro_local_solar_eclipse_t returned by the
 * previous call to `Astronomy_SearchLocalSolarEclipse` or `Astronomy_NextLocalSolarEclipse`
 * to find the next solar eclipse.
 *
 * @param prevEclipseTime
 *      A date and time near a new moon. Solar eclipse search will start at the next new moon.
 *
 * @param observer
 *      The geographic location of the observer.
 *
 * @return
 *      If successful, the `status` field in the returned structure will contain `ASTRO_SUCCESS`
 *      and the remaining structure fields are as described in #astro_local_solar_eclipse_t.
 *      Any other value indicates an error.
 */
astro_local_solar_eclipse_t Astronomy_NextLocalSolarEclipse(
    astro_time_t prevEclipseTime,
    astro_observer_t observer)
{
    astro_time_t startTime = Astronomy_AddDays(prevEclipseTime, 10.0);
    return Astronomy_SearchLocalSolarEclipse(startTime, observer);
}


static astro_func_result_t planet_transit_bound(void *context, astro_time_t time)
{
    shadow_t shadow;
    astro_func_result_t result;
    const planet_shadow_context_t *p = context;

    shadow = PlanetShadow(p->body, p->planet_radius_km, time);
    if (shadow.status != ASTRO_SUCCESS)
        return FuncError(shadow.status);

    result.status = ASTRO_SUCCESS;
    result.value = p->direction * (shadow.r - shadow.p);
    return result;
}


static astro_search_result_t PlanetTransitBoundary(
    astro_body_t body,
    double planet_radius_km,
    astro_time_t t1,
    astro_time_t t2,
    double direction)
{
    /* Search for the time the planet's penumbra begins/ends making contact with the center of the Earth. */
    planet_shadow_context_t context;

    context.body = body;
    context.planet_radius_km = planet_radius_km;
    context.direction = direction;

    return Astronomy_Search(planet_transit_bound, &context, t1, t2, 1.0);
}


/**
 * @brief Searches for the first transit of Mercury or Venus after a given date.
 *
 * Finds the first transit of Mercury or Venus after a specified date.
 * A transit is when an inferior planet passes between the Sun and the Earth
 * so that the silhouette of the planet is visible against the Sun in the background.
 * To continue the search, pass the `finish` time in the returned structure to
 * #Astronomy_NextTransit.
 *
 * @param body
 *      The planet whose transit is to be found. Must be `BODY_MERCURY` or `BODY_VENUS`.
 *
 * @param startTime
 *      The date and time for starting the search for a transit.
 *
 * @return
 *      If successful, the `status` field in the returned structure hold `ASTRO_SUCCESS`
 *      and the other fields are as documented in #astro_transit_t.
 *      Otherwise, `status` holds an error code and the other structure members are undefined.
 */
astro_transit_t Astronomy_SearchTransit(astro_body_t body, astro_time_t startTime)
{
    astro_time_t search_time;
    astro_transit_t transit;
    astro_search_result_t conj, search;
    astro_angle_result_t conj_separation, min_separation;
    shadow_t shadow;
    double planet_radius_km;
    astro_time_t tx;
    const double threshold_angle = 0.4;     /* maximum angular separation to attempt transit calculation */
    const double dt_days = 1.0;

    /* Validate the planet and find its mean radius. */
    switch (body)
    {
    case BODY_MERCURY:  planet_radius_km = 2439.7;  break;
    case BODY_VENUS:    planet_radius_km = 6051.8;  break;
    default:
        return TransitErr(ASTRO_INVALID_BODY);
    }

    search_time = startTime;
    for(;;)
    {
        /*
            Search for the next inferior conjunction of the given planet.
            This is the next time the Earth and the other planet have the same
            ecliptic longitude as seen from the Sun.
        */
        conj = Astronomy_SearchRelativeLongitude(body, 0.0, search_time);
        if (conj.status != ASTRO_SUCCESS)
            return TransitErr(conj.status);

        /* Calculate the angular separation between the body and the Sun at this time. */
        conj_separation = Astronomy_AngleFromSun(body, conj.time);
        if (conj_separation.status != ASTRO_SUCCESS)
            return TransitErr(conj_separation.status);

        if (conj_separation.angle < threshold_angle)
        {
            /*
                The planet's angular separation from the Sun is small enough
                to consider it a transit candidate.
                Search for the moment when the line passing through the Sun
                and planet are closest to the Earth's center.
            */
            shadow = PeakPlanetShadow(body, planet_radius_km, conj.time);
            if (shadow.status != ASTRO_SUCCESS)
                return TransitErr(shadow.status);

            if (shadow.r < shadow.p)        /* does the planet's penumbra touch the Earth's center? */
            {
                /* Find the beginning and end of the penumbral contact. */
                tx = Astronomy_AddDays(shadow.time, -dt_days);
                search = PlanetTransitBoundary(body, planet_radius_km, tx, shadow.time, -1.0);
                if (search.status != ASTRO_SUCCESS)
                    return TransitErr(search.status);
                transit.start = search.time;

                tx = Astronomy_AddDays(shadow.time, +dt_days);
                search = PlanetTransitBoundary(body, planet_radius_km, shadow.time, tx, +1.0);
                if (search.status != ASTRO_SUCCESS)
                    return TransitErr(search.status);
                transit.finish = search.time;
                transit.status = ASTRO_SUCCESS;
                transit.peak = shadow.time;

                min_separation = Astronomy_AngleFromSun(body, shadow.time);
                if (min_separation.status != ASTRO_SUCCESS)
                    return TransitErr(min_separation.status);

                transit.separation = 60.0 * min_separation.angle;   /* convert degrees to arcminutes */
                return transit;
            }
        }

        /* This inferior conjunction was not a transit. Try the next inferior conjunction. */
        search_time = Astronomy_AddDays(conj.time, 10.0);
    }
}


/**
 * @brief Searches for another transit of Mercury or Venus.
 *
 * After calling #Astronomy_SearchTransit to find a transit of Mercury or Venus,
 * this function finds the next transit after that.
 * Keep calling this function as many times as you want to keep finding more transits.
 *
 * @param body
 *      The planet whose transit is to be found. Must be `BODY_MERCURY` or `BODY_VENUS`.
 *
 * @param prevTransitTime
 *      A date and time near the previous transit.
 *
 * @return
 *      If successful, the `status` field in the returned structure hold `ASTRO_SUCCESS`
 *      and the other fields are as documented in #astro_transit_t.
 *      Otherwise, `status` holds an error code and the other structure members are undefined.
 */
astro_transit_t Astronomy_NextTransit(astro_body_t body, astro_time_t prevTransitTime)
{
    astro_time_t startTime;

    startTime = Astronomy_AddDays(prevTransitTime, 100.0);
    return Astronomy_SearchTransit(body, startTime);
}


/**
 * @brief Frees up all dynamic memory allocated by Astronomy Engine.
 *
 * Astronomy Engine uses dynamic memory allocation in only one place:
 * it makes calculation of Pluto's orbit more efficient by caching 11 KB
 * segments recycling them. To force purging this cache and
 * freeing all the dynamic memory, you can call this function at any time.
 * It is always safe to call, although it will slow down the very next
 * calculation of Pluto's position for a nearby time value.
 * Calling this function before your program exits is optional, but
 * it will be helpful for leak-checkers like valgrind.
 */
void Astronomy_Reset(void)
{
    int i;
    for (i=0; i < PLUTO_NUM_STATES-1; ++i)
    {
        free(pluto_cache[i]);
        pluto_cache[i] = NULL;
    }
}


#ifdef __cplusplus
}
#endif
