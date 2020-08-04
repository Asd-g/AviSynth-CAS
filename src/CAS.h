#pragma once

#include <algorithm>
#include <any>

#include "avisynth.h"
#include "VCL2/vectorclass.h"

class CAS : public GenericVideoFilter
{
    float sharpness_;
    int y_, u_, v_, opt_;
    int process[3];
    std::any limit;
    int peak;
    bool has_at_least_v8;

    template<typename pixel_t>
    void filter_c(PVideoFrame& src, PVideoFrame& dst, const CAS* const __restrict, IScriptEnvironment* env) noexcept;
    template<typename pixel_t>
    void filter_sse2(PVideoFrame& src, PVideoFrame& dst, const CAS* const __restrict, IScriptEnvironment* env) noexcept;
    template<typename pixel_t>
    void filter_avx2(PVideoFrame& src, PVideoFrame& dst, const CAS* const __restrict, IScriptEnvironment* env) noexcept;
    template<typename pixel_t>
    void filter_avx512(PVideoFrame& src, PVideoFrame& dst, const CAS* const __restrict, IScriptEnvironment* env) noexcept;

public:
    CAS(PClip _child, float sharpness, int y, int u, int v, int opt, IScriptEnvironment* env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);
    int __stdcall SetCacheHints(int cachehints, int frame_range)
    {
        return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
    }
};