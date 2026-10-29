import com.luka.carplay.core.ApStatus;

public final class ApStatusTest {
    private static void check(int status, String suffix) {
        if (!suffix.equals(ApStatus.suffix(status))) throw new AssertionError("status " + status);
    }
    public static void main(String[] args) {
        check(-6714, " (kNotHandledErr)");
        check(-6728, " (kNoMemoryErr)");
        check(-6735, " (kUnsupportedErr)");
        check(-6740, " (kBadReferenceErr)");
        check(-6700, " (kUnknownErr)");
        check(-6775, " (kWriteWouldBlockErr)");
        check(-6776, "");
        check(0, "");
        check(Integer.MIN_VALUE, "");
        check(Integer.MAX_VALUE, "");
        System.out.println("ApStatus: known errors, boundaries and unknown statuses PASS");
    }
}
