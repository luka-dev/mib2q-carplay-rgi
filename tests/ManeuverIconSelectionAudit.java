import com.luka.carplay.framework.Log;
import java.io.*;

/** Controlled, readable icon-review examples through the real bridge. */
public final class ManeuverIconSelectionAudit {
    public static void main(String[] args) throws Exception {
        Log.setLevel(-1);
        ManeuverChainAudit audit = new ManeuverChainAudit();
        PrintWriter out = new PrintWriter(new OutputStreamWriter(new FileOutputStream(args[0]), "UTF-8"));
        out.println("case,mt,type,angle,junction,traffic,z_input,junction_angles,bap_main,bap_direction,bap_z,bap_sidestreets,stock_sidestreets,stock_fct23_hex,renderer_icon,renderer_direction,renderer_angle,renderer_hex");
        for (int mt = 0; mt < 54; mt++) {
            int angle = ManeuverChainAudit.exampleAngle(mt);
            if (mt == 11 || mt == 16 || mt == 51) angle = 45;
            if (mt == 20) angle = -135;
            if (mt == 21) angle = 135;
            int junction = (mt == 6 || mt == 7 || mt == 19 || mt >= 28 && mt <= 46) ? 1 : 0;
            for (int side = 0; side < 2; side++) {
                audit.row(out, "selection", mt, angle, junction, side, 0, new int[]{-90, 0, 90});
            }
        }
        out.close();
        System.err.println("ManeuverIconSelectionAudit: 108 bridge examples");
    }
}
