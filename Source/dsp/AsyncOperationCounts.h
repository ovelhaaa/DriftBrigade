#pragma once
#include <cstdint>
namespace drift {
struct AsyncOperationCounts {
 std::uint64_t advances=0, builds=0, periodApplications=0, arbitraryBuilds=0, realExp=0, sinCosPairs=0, characterExp=0, characterPow=0;
};
#ifdef DRIFT_BBD_INSTRUMENT
inline AsyncOperationCounts asyncOperations;
#define DRIFT_ASYNC_COUNT(field, amount) (::drift::asyncOperations.field += (amount))
#else
#define DRIFT_ASYNC_COUNT(field, amount) ((void)0)
#endif
}
