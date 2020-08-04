#include <cmath>

#include "CAS.h"

template<typename pixel_t>
void CAS::filter_c(PVideoFrame& src, PVideoFrame& dst, const CAS* const __restrict, IScriptEnvironment* env) noexcept
{
    using var_t = std::conditional_t<std::is_integral_v<pixel_t>, int, float>;

    const var_t limit = std::any_cast<var_t>(CAS::limit);

    auto filtering = [&](const var_t a, const var_t b, const var_t c, const var_t d, const var_t e, const var_t f, const var_t g, const var_t h, const var_t i, const float chromaOffset) noexcept
    {
            // Soft min and max.
            //  a b c             b
            //  d e f * 0.5  +  d e f * 0.5
            //  g h i             h
            // These are 2.0x bigger (factored out the extra multiply).
            var_t mn = std::min({ d, e, f, b, h });
            const var_t mn2 = std::min({ mn, a, c, g, i });
            mn += mn2;

            var_t mx = std::max({ d, e, f, b, h });
            const var_t mx2 = std::max({ mx, a, c, g, i });
            mx += mx2;

            if constexpr (std::is_floating_point_v<pixel_t>)
            {
                mn += chromaOffset;
                mx += chromaOffset;
            }

            // Smooth minimum distance to signal limit divided by smooth max.
            float amp = std::clamp(std::min(mn, limit - mx) / static_cast<float>(mx), 0.0f, 1.0f);

            // Shaping amount of sharpening.
            amp = std::sqrt(amp);

            // Filter shape.
            //  0 w 0
            //  w 1 w
            //  0 w 0
            const float weight = amp * sharpness_;
            return ((b + d + f + h) * weight + e) / (1.0f + 4.0f * weight);
    };

    int planes_y[4] = { PLANAR_Y, PLANAR_U, PLANAR_V, PLANAR_A };
    int planes_r[4] = { PLANAR_G, PLANAR_B, PLANAR_R, PLANAR_A };
    const int* current_planes = (vi.IsYUV() || vi.IsYUVA()) ? planes_y : planes_r;
    const int planecount = std::min(vi.NumComponents(), 3);
    for (int i = 0; i < planecount; i++)
    {
        const int plane = current_planes[i];

        if (process[i] == 3)
        {
            const int stride = src->GetPitch(plane) / sizeof(pixel_t);
            const int width = src->GetRowSize(plane) / sizeof(pixel_t);
            const int height = src->GetHeight(plane);
            const pixel_t* srcp = reinterpret_cast<const pixel_t*>(src->GetReadPtr(plane));
            pixel_t* __restrict dstp = reinterpret_cast<pixel_t*>(dst->GetWritePtr(plane));

            const float chromaOffset = i ? 1.0f : 0.0f;

            for (int y = 0; y < height; y++)
            {
                const pixel_t* above = srcp + (y == 0 ? stride : -stride);
                const pixel_t* below = srcp + (y == height - 1 ? -stride : stride);

                {
                    const float result = filtering(above[1], above[0], above[1],
                        srcp[1], srcp[0], srcp[1],
                        below[1], below[0], below[1],
                        chromaOffset);

                    if constexpr (std::is_integral_v<pixel_t>)
                        dstp[0] = std::clamp(static_cast<int>(result + 0.5f), 0, peak);
                    else
                        dstp[0] = result;
                }

                for (int x = 1; x < width - 1; x++)
                {
                    const float result = filtering(above[x - 1], above[x], above[x + 1],
                        srcp[x - 1], srcp[x], srcp[x + 1],
                        below[x - 1], below[x], below[x + 1],
                        chromaOffset);

                    if constexpr (std::is_integral_v<pixel_t>)
                        dstp[x] = std::clamp(static_cast<int>(result + 0.5f), 0, peak);
                    else
                        dstp[x] = result;
                }

                {
                    const float result = filtering(above[width - 2], above[width - 1], above[width - 2],
                        srcp[width - 2], srcp[width - 1], srcp[width - 2],
                        below[width - 2], below[width - 1], below[width - 2],
                        chromaOffset);

                    if constexpr (std::is_integral_v<pixel_t>)
                        dstp[width - 1] = std::clamp(static_cast<int>(result + 0.5f), 0, peak);
                    else
                        dstp[width - 1] = result;
                }

                srcp += stride;
                dstp += dst->GetPitch(plane) / sizeof(pixel_t);
            }
        }
        else if (process[i] == 2)
            env->BitBlt(dst->GetWritePtr(plane), dst->GetPitch(plane), src->GetReadPtr(plane), src->GetPitch(plane), src->GetRowSize(plane), src->GetHeight(plane));
    }
}

CAS::CAS(PClip _child, float sharpness, int y, int u, int v, int opt, IScriptEnvironment* env)
    : GenericVideoFilter(_child), sharpness_(sharpness), y_(y), u_(u), v_(v), opt_(opt)
{
    if (sharpness_ < 0.0f || sharpness_ > 1.0f)
        env->ThrowError("CAS: sharpness must be between 0.0 and 1.0 (inclusive)");
    if (opt_ < -1 || opt_ > 3)
        env->ThrowError("CAS: opt must be between -1..3");
    if (!(env->GetCPUFlags() & CPUF_AVX512F) && opt_ == 3)
        env->ThrowError("CAS: opt=3 requires AVX512F.");
    if (!(env->GetCPUFlags() & CPUF_AVX2) && opt_ == 2)
        env->ThrowError("CAS: opt=2 requires AVX2.");
    if (!(env->GetCPUFlags() & CPUF_SSE2) && opt_ == 1)
        env->ThrowError("CAS: opt=1 requires SSE2.");

    const int planecount = std::min(vi.NumComponents(), 3);
    for (int i = 0; i < planecount; i++)
    {
        if (vi.IsRGB())
            process[i] = 3;
        else
        {
            switch (i)
            {
                case 0:
                    switch (y_)
                    {
                        case 3: process[i] = 3; break;
                        case 2: process[i] = 2; break;
                        default: process[i] = 1; break;
                    }
                    break;
                case 1:
                    switch (u_)
                    {
                        case 3: process[i] = 3; break;
                        case 2: process[i] = 2; break;
                        default: process[i] = 1; break;
                    }
                    break;
                default:
                    switch (v_)
                    {
                        case 3: process[i] = 3; break;
                        case 2: process[i] = 2; break;
                        default: process[i] = 1; break;
                    }
                    break;
            }
        }

        if (vi.width >> (i ? vi.GetPlaneWidthSubsampling(PLANAR_U) : 0) < 3)
            env->ThrowError("CAS: plane's width must be greater than or equal to 3");

        if (vi.height >> (i ? vi.GetPlaneHeightSubsampling(PLANAR_U) : 0) < 3)
            env->ThrowError("CAS: plane's height must be greater than or equal to 3");
    }

    auto lerp = [](const float a, const float b, const float t) noexcept { return a + (b - a) * t; };
    sharpness_ = -1.0f / lerp(16.0f, 5.0f, sharpness_);

    const int bits = vi.BitsPerComponent();
    if (vi.ComponentSize() != 4)
    {
        limit = (1 << (bits + 1)) - 1;
        peak = (1 << bits) - 1;
    }
    else
        limit = 2.0f;

    has_at_least_v8 = true;
    try { env->CheckVersion(8); }
    catch (const AvisynthError&) { has_at_least_v8 = false; }
}

PVideoFrame __stdcall CAS::GetFrame(int n, IScriptEnvironment* env)
{
    PVideoFrame src = child->GetFrame(n, env);
    PVideoFrame dst = has_at_least_v8 ? env->NewVideoFrameP(vi, &src) : env->NewVideoFrame(vi);

    if ((!!(env->GetCPUFlags() & CPUF_AVX512F) && opt_ < 0) || opt_ == 3)
    {
        switch (vi.ComponentSize())
        {
            case 1: filter_avx512<uint8_t>(src, dst, 0, env); break;
            case 2: filter_avx512<uint16_t>(src, dst, 0, env); break;
            default: filter_avx512<float>(src, dst, 0, env); break;
        }
    }
    else if ((!!(env->GetCPUFlags() & CPUF_AVX2) && opt_ < 0) || opt_ == 2)
    {
        switch (vi.ComponentSize())
        {
            case 1: filter_avx2<uint8_t>(src, dst, 0, env); break;
            case 2: filter_avx2<uint16_t>(src, dst, 0, env); break;
            default: filter_avx2<float>(src, dst, 0, env); break;
        }
    }
    else if ((!!(env->GetCPUFlags() & CPUF_SSE2) && opt_ < 0) || opt_ == 1)
    {
        switch (vi.ComponentSize())
        {
            case 1: filter_sse2<uint8_t>(src, dst, 0, env); break;
            case 2: filter_sse2<uint16_t>(src, dst, 0, env); break;
            default: filter_sse2<float>(src, dst, 0, env); break;
        }
    }
    else
    {
        switch (vi.ComponentSize())
        {
            case 1: filter_c<uint8_t>(src, dst, 0, env); break;
            case 2: filter_c<uint16_t>(src, dst, 0, env); break;
            default: filter_c<float>(src, dst, 0, env); break;
        }
    }

    return dst;
}

AVSValue __cdecl Create_CAS(AVSValue args, void* user_data, IScriptEnvironment* env)
{
    return new CAS(
        args[0].AsClip(),
        args[1].AsFloatf(0.5f),
        args[2].AsInt(3),
        args[3].AsInt(2),
        args[4].AsInt(2),
        args[5].AsInt(-1),
        env);
}

const AVS_Linkage* AVS_linkage;

extern "C" __declspec(dllexport)
const char* __stdcall AvisynthPluginInit3(IScriptEnvironment * env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("CAS", "c[sharpness]f[y]i[u]i[v]i[opt]i", Create_CAS, 0);

    return "CAS";
}