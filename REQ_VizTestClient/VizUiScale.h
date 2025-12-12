#pragma once

#include <algorithm>
#include <cstdint>

// ============================================================================
// VizUiScale - Unified UI scaling for consistent text sizing
// ============================================================================

namespace VizUi {
    /**
     * GetUiFontPx
     * 
     * Calculates UI font size based on window height for consistent scaling.
     * Uses a divisor-based formula to ensure readability at all resolutions.
     * 
     * Formula: fontSize = clamp(windowHeight / divisor, minPx, maxPx)
     * 
     * @param windowHeight Current window height in pixels
     * @param minPx Minimum font size (fallback for small windows)
     * @param maxPx Maximum font size (cap for large windows)
     * @param divisor Scaling divisor (default 32.0f for balanced scaling)
     * @return Clamped font size in pixels
     * 
     * Examples at divisor=32:
     *   720p  (720px):  720/32 = 22.5  ? clamp to 24px (min)
     *   1080p (1080px): 1080/32 = 33.75 ? 34px
     *   1440p (1440px): 1440/32 = 45    ? 45px
     *   4K    (2160px): 2160/32 = 67.5  ? clamp to 48px (max)
     */
    inline unsigned int GetUiFontPx(
        float windowHeight,
        unsigned int minPx = 24u,
        unsigned int maxPx = 48u,
        float divisor = 32.0f)
    {
        float scaledSize = windowHeight / divisor;
        unsigned int fontSize = static_cast<unsigned int>(scaledSize);
        return std::clamp(fontSize, minPx, maxPx);
    }
}
