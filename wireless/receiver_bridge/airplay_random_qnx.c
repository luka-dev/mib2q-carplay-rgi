/* QNX 6.5 libc exports rand(), but not random(). The 280 SDK's POSIX
 * Random32() macro calls random() in the AirPlay timing thread. Supply the
 * expected 31-bit interface from the SDK's /dev/urandom-backed RandomBytes.
 */
#include <stddef.h>
#include <stdint.h>

extern int RandomBytes(void *buffer, size_t length);

long random(void)
{
    uint32_t value = 0;
    if (RandomBytes(&value, sizeof(value)) != 0)
        return 0;
    return (long)(value & UINT32_C(0x7fffffff));
}
