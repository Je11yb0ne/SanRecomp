#include <os/media.h>

// WinRT GSMTC query replaced with stub to avoid pulling in
// api-ms-win-core-winrt-error-l1-1-1.dll dependency.
// See REXRUNTIME_FIX_AND_PROJECT_PLAN.md for details.

bool os::media::IsExternalMediaPlaying()
{
    return false;
}
