/* One bounded, guest-native locationd trajectory for the owned research VM. */
typedef void *Object;
typedef void *Selector;
typedef struct { double latitude, longitude; } Coordinate;
struct timespec { long tv_sec, tv_nsec; };
extern Object objc_getClass(const char *);
extern Selector sel_registerName(const char *);
extern void objc_msgSend(void);
extern void *dlopen(const char *, int);
extern unsigned sleep(unsigned);
extern unsigned alarm(unsigned);
extern unsigned getuid(void);
extern int printf(const char *, ...);
extern int fflush(void *);
extern int clock_gettime(unsigned, struct timespec *);

static Object send0(Object object, const char *selector)
{
    return ((Object (*)(Object, Selector))objc_msgSend)(object, sel_registerName(selector));
}

static void send1(Object object, const char *selector, Object argument)
{
    ((void (*)(Object, Selector, Object))objc_msgSend)(object, sel_registerName(selector), argument);
}

static Object location(double latitude, double longitude, double speed, Object date)
{
    Object allocated = send0(objc_getClass("CLLocation"), "alloc");
    return ((Object (*)(Object, Selector, Coordinate, double, double, double, double, double, Object))objc_msgSend)(
        allocated, sel_registerName("initWithCoordinate:altitude:horizontalAccuracy:verticalAccuracy:course:speed:timestamp:"),
        (Coordinate){latitude, longitude}, 0, 5, 5, 242.7, speed, date);
}

static unsigned long long wall_ns(void)
{
    struct timespec stamp;
    if (clock_gettime(0, &stamp)) return 0;
    return (unsigned long long)stamp.tv_sec * 1000000000ULL + stamp.tv_nsec;
}

int main(void)
{
    const unsigned seconds = 40;
    unsigned i;
    Object manager, date, start;
    const char *selectors[] = {"stopLocationSimulation", "clearSimulatedLocations",
        "appendSimulatedLocation:", "flush", "startLocationSimulation"};
    if (getuid() != 0) return 2;
    alarm(70);
    if (!dlopen("/System/Library/Frameworks/Foundation.framework/Foundation", 2) ||
        !dlopen("/System/Library/Frameworks/CoreLocation.framework/CoreLocation", 2)) return 3;
    manager = send0(objc_getClass("CLSimulationManager"), "new");
    if (!manager) return 4;
    for (i = 0; i < sizeof(selectors)/sizeof(selectors[0]); i++) {
        if (!((signed char (*)(Object, Selector, Selector))objc_msgSend)(manager,
            sel_registerName("respondsToSelector:"), sel_registerName(selectors[i]))) return 5;
    }
    send0(manager, "stopLocationSimulation");
    send0(manager, "clearSimulatedLocations");
    start = ((Object (*)(Object, Selector, double))objc_msgSend)(objc_getClass("NSDate"),
        sel_registerName("dateWithTimeIntervalSinceNow:"), 2.0);
    for (i = 0; i <= seconds; i++) {
        double fraction = (double)i / seconds;
        Object point;
        date = ((Object (*)(Object, Selector, double))objc_msgSend)(start,
            sel_registerName("dateByAddingTimeInterval:"), (double)i);
        point = location(37.3349 + (37.3320-37.3349)*fraction,
                         -122.0090 + (-122.0160+122.0090)*fraction, 17.5, date);
        if (!point) return 6;
        send1(manager, "appendSimulatedLocation:", point);
        send0(point, "release");
    }
    send0(manager, "flush");
    printf("route prepared points=%u duration=%u start_wall_ns=%llu\n", seconds+1, seconds, wall_ns());
    fflush((void *)0);
    send0(manager, "startLocationSimulation");
    sleep(seconds+4);
    send0(manager, "stopLocationSimulation");
    send0(manager, "clearSimulatedLocations");
    date = send0(objc_getClass("NSDate"), "date");
    {
        Object point = location(37.3320, -122.0160, 0, date);
        if (!point) return 7;
        send1(manager, "appendSimulatedLocation:", point);
        send0(point, "release");
    }
    send0(manager, "flush");
    send0(manager, "startLocationSimulation");
    printf("route complete stationary=1 end_wall_ns=%llu\n", wall_ns());
    fflush((void *)0);
    send0(manager, "release");
    return 0;
}
