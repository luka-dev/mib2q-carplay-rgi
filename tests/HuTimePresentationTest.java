import com.luka.carplay.core.CarPlayApp;
import com.luka.carplay.core.FrameworkRef;
import com.luka.carplay.framework.HuTime;
import com.luka.carplay.rgd.BAPBridge;
import com.luka.carplay.telephony.GatedPhoneService;
import de.audi.app.terminalmode.IContext;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPCallStackEntry;
import de.audi.atip.metrics.DateMetric;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.time.Instant;
import java.util.TimeZone;

/** Exercise the shipping helper and both display consumers with a fake HU clock. */
public final class HuTimePresentationTest {
    private static final long UTC_MS = Instant.parse("2026-12-31T23:30:42Z").toEpochMilli();
    private static long offsetMs;
    private static boolean failConversion;
    private static int calls, checks;
    private static Method recent, arrival;

    private static void equal(String label, Object expected, Object actual) {
        checks++;
        if (!expected.equals(actual))
            throw new AssertionError(label + ": expected " + expected + ", got " + actual);
    }

    private static CombiBAPCallStackEntry recent(long timestamp) throws Exception {
        return (CombiBAPCallStackEntry) recent.invoke(null, 0, "Caller", "123", 2, timestamp);
    }

    private static String date(CombiBAPCallStackEntry entry) {
        return entry.getYear() + "/" + entry.getMonth() + "/" + entry.getDay() + " "
            + entry.getHour() + ":" + entry.getMinute() + ":" + entry.getSecond();
    }

    private static void presentation(String expectedDate, String expectedTime) throws Exception {
        equal("recent in seconds", expectedDate, date(recent(UTC_MS / 1000)));
        equal("recent in milliseconds", expectedDate, date(recent(UTC_MS)));
        equal("ETA text", "\u25cc " + expectedTime, arrival.invoke(null, UTC_MS / 1000));
    }

    public static void main(String[] args) throws Exception {
        TimeZone originalZone = TimeZone.getDefault();
        int originalFormat = DateMetric.timeFormat;
        Field framework = CarPlayApp.class.getDeclaredField("fwRef");
        framework.setAccessible(true);
        Object originalFramework = framework.get(null);
        recent = GatedPhoneService.class.getDeclaredMethod("recent",
            int.class, String.class, String.class, int.class, long.class);
        recent.setAccessible(true);
        arrival = BAPBridge.class.getDeclaredMethod("formatArrivalForText", long.class);
        arrival.setAccessible(true);
        try {
            // The HU's JVM uses UTC; its selected local offset comes from the framework.
            TimeZone.setDefault(TimeZone.getTimeZone("UTC"));
            DateMetric.timeFormat = 10;
            IFrameworkAccess clock = (IFrameworkAccess) Proxy.newProxyInstance(
                HuTimePresentationTest.class.getClassLoader(), new Class[] {IFrameworkAccess.class},
                (proxy, method, values) -> {
                    if (!method.getName().equals("convertUTCTimeToLocalTime"))
                        throw new AssertionError("Unexpected framework call: " + method);
                    calls++;
                    if (failConversion) throw new IllegalStateException("Clock unavailable");
                    return ((Long) values[0]).longValue() + offsetMs;
                });
            IContext context = (IContext) Proxy.newProxyInstance(
                HuTimePresentationTest.class.getClassLoader(), new Class[] {IContext.class},
                (proxy, method, values) -> {
                    if (method.getName().equals("getFramework")) return clock;
                    throw new AssertionError("Unexpected context call: " + method);
                });
            framework.set(null, new FrameworkRef(context));

            offsetMs = 2 * 3600000L;
            equal("helper uses milliseconds", UTC_MS + offsetMs, HuTime.convertUtcToLocalMs(UTC_MS));
            presentation("2027/1/1 1:30:42", "01:30");
            equal("each consumer uses framework once", 4, calls);

            // Reuse the same framework: changing the HU offset must not leave a cached value.
            offsetMs = -5 * 3600000L;
            presentation("2026/12/31 18:30:42", "18:30");
            offsetMs = 19800000L; // UTC+05:30
            presentation("2027/1/1 5:0:42", "05:00");
            offsetMs = 0;
            presentation("2026/12/31 23:30:42", "23:30");

            offsetMs = 2 * 3600000L;
            failConversion = true;
            presentation("2026/12/31 23:30:42", "23:30");
            failConversion = false;
            framework.set(null, null);
            presentation("2026/12/31 23:30:42", "23:30");

            framework.set(null, new FrameworkRef(context));
            int before = calls;
            for (long timestamp : new long[] {0, -1}) {
                CombiBAPCallStackEntry entry = recent(timestamp);
                equal("missing timestamp has no date", "0/0/0 0:0:0", date(entry));
                equal("missing timestamp retains caller", "Caller", entry.getPbName());
            }
            equal("missing timestamp skips conversion", before, calls);
            System.out.println("HuTimePresentationTest: " + checks + " checks passed");
        } finally {
            framework.set(null, originalFramework);
            DateMetric.timeFormat = originalFormat;
            TimeZone.setDefault(originalZone);
        }
    }
}
