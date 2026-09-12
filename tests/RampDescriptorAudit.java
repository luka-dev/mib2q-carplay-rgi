import com.luka.carplay.framework.Log;
import java.io.*;

/** Focused evidence for ramp/slight equivalence, using the actual bridge. */
public final class RampDescriptorAudit {
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        ManeuverChainAudit audit = new ManeuverChainAudit();
        PrintWriter out = new PrintWriter(new OutputStreamWriter(new FileOutputStream(args[0]), "UTF-8"));
        out.println("case,mt,type,angle,junction,traffic,z_input,junction_angles,bap_main,bap_direction,bap_z,bap_sidestreets,stock_sidestreets,stock_fct23_hex,renderer_icon,renderer_direction,renderer_angle,renderer_hex");
        int count = 0;
        for (int side = 0; side < 2; side++) for (int sign : new int[]{-1, 1}) {
            int[] types = {8, 9, sign < 0 ? 13 : 14, sign < 0 ? 22 : 23,
                sign < 0 ? 49 : 50, sign < 0 ? 52 : 53};
            for (int mt : types) for (int angle : new int[]{45, 90, 135})
                for (int[] streets : new int[][]{null, {0}, {0, sign * angle}}) {
                    audit.row(out, "ramp_focus", mt, sign * angle, 0, side, 0, streets);
                    count++;
                }
        }
        out.close();
        System.err.println("RampDescriptorAudit: " + count + " actual bridge cases");
    }
}
