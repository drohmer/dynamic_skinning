#include "noise.hpp"

#include "third_party/simplexnoise/simplexnoise1234.hpp"
#include "cgp/01_base/base.hpp"

namespace cgp
{

    float noise_perlin(float x, int octave, float persistency, float frequency_gain)
    {
        float value = 0.0f;
        float a = 1.0f;
        float f = 1.0f;
        float a_sum = 0.0f;
        for(int k=0; k<octave; k++)
        {
            value += a*(0.5f + 0.5f*static_cast<float>(snoise1(x*f)));
            a_sum += a;
            f *= frequency_gain;
            a *= persistency;
        }
        return value / a_sum;
    }

    float noise_perlin(vec2 const& p, int octave, float persistency, float frequency_gain)
    {
        float value = 0.0f;
        float a = 1.0f;
        float f = 1.0f;
        float a_sum = 0.0f;
        for(int k=0; k<octave; k++)
        {
            value += a*(0.5f + 0.5f*static_cast<float>(snoise2(p.x*f, p.y*f)));
            a_sum += a;
            f *= frequency_gain;
            a *= persistency;
        }
        return value / a_sum;
    }

    float noise_perlin(vec3 const& p, int octave, float persistency, float frequency_gain)
    {
        float value = 0.0f;
        float a = 1.0f;
        float f = 1.0f;
        float a_sum = 0.0f;
        for(int k=0; k<octave; k++)
        {
            value += a*(0.5f + 0.5f*static_cast<float>(snoise3(p.x*f, p.y*f, p.z*f)));
            a_sum += a;
            f *= frequency_gain;
            a *= persistency;
        }
        return value / a_sum;
    }

}