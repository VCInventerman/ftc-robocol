#if !defined(LIBROBOCOL_BUFCACHE_H)
#define LIBROBOCOL_BUFCACHE_H

#include <vector>
#include <assert.h>
#include <algorithm>

#include "sync.h"
#include "FixedBuf.h"

namespace librobocol
{
    // Singleton storage for caching read/write buffers
    class BufCache
    {
    public:
        static std::vector<FixedBuf> freeBufs;

        // Avoid memory leaks by storing a list of buffers that may or may not still exist
        // static std::vector<std::reference_wrapper<FixedBuf>> allBufs;

        STATIC_SYNCHRONIZED_CLASS

        // Get the smallest power of 2 equal to or greater than x
        static size_t nearestPower(size_t size)
        {
            size_t val = 1 << (32 - __builtin_clz(size - 1));

            printf("rounded %u to %u\n", size, val);

            return val;
        }

        static FixedBuf getBuf(size_t size)
        {
            assert(size < 55001);

            auto l = lock();

            // nearestPower ROUNDS UP
            //size_t closest = nearestPower(size);
            size_t closest = size;

            auto itr = std::find_if(freeBufs.begin(), freeBufs.end(),
                                    [closest](FixedBuf &val)
                                    { return val.len == closest; });

            if (itr == freeBufs.end())
            {
                FixedBuf buf(closest);
                //buf.len = size;
                return buf;
            }
            else
            {
                //todo: prevent bufs gradually losing size?
                size_t dist = std::distance(freeBufs.begin(), itr);
                printf("Moving from len %u with a dist of %u\n", freeBufs.size(), dist);
                FixedBuf buf = std::move(freeBufs[dist]);
                // freeBufs.erase(itr);
                buf.len = size;
                return buf;
            }
        }

        static void recycle(FixedBuf &&buf)
        {
            auto l = lock();

            printf("stage 6\n");

            freeBufs.emplace_back(FixedBuf());

            printf("stage 7\n");

            freeBufs.emplace_back(std::move(buf));

            printf("stage 8\n");
        }
    };
    Mutex BufCache::accessM = {};
    std::vector<FixedBuf> BufCache::freeBufs = {};
}

#endif // if !defined(LIBROBOCOL_BUFCACHE_H)