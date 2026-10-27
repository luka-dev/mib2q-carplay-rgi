/* CMD_VEHICLE bytes match hook/vehicle/vehicle_hook.c veh_parse_cmd (LE, 14 bytes), unit
 * conversions to cm/s, km and 0.1 degC, and the BAP battery buckets of GatedPhoneService. */
package com.luka.carplay.core;

import com.luka.carplay.telephony.GatedPhoneService;
import de.audi.atip.interapp.combi.bap.phone.data.CombiBAPhoneMobileBatteryLevel;

public final class VehicleDataEncodingTest {
    private static void check(boolean ok, String what) {
        if (!ok) throw new AssertionError(what);
    }

    private static int level(int pct) {
        return GatedPhoneService.batteryLevel(pct).getMobileChargeLevel1().getChargeLevelPercent();
    }

    public static void main(String[] args) {
        byte[] p = VehicleDataModule.encode(0x1F, 1389, VehicleDataModule.GEAR_REVERSE, 412, -6, 1, 0x01020304L);
        byte[] want = { 1, 0x1F, 0x6D, 0x05, 2, (byte)0x9C, 0x01, (byte)0xFA, (byte)0xFF, 1, 4, 3, 2, 1 };
        check(p.length == want.length, "length");
        for (int i = 0; i < want.length; i++) check(p[i] == want[i], "byte " + i);

        check(VehicleDataModule.speedToCms(50f, 0) == 1389, "50 km/h");
        check(VehicleDataModule.speedToCms(31.0686f, 1) == 1389, "31.07 mph");
        check(VehicleDataModule.speedToCms(3000f, 0) == 65535, "clamp");
        check(VehicleDataModule.distanceToKm(100.0, 1) == 161, "100 mi");
        check(VehicleDataModule.tempToDc(-0.6f, 0) == -6, "-0.6 C");
        check(VehicleDataModule.tempToDc(68f, 1) == 200, "68 F");

        check(level(-1) == CombiBAPhoneMobileBatteryLevel.BATTERY_LEVEL_UNKNOWN_LEVEL_NOT_RECEIVED, "unknown");
        check(level(0) == 0 && level(19) == 19 && level(20) == 39 && level(59) == 59, "buckets low");
        check(level(60) == 79 && level(80) == 100 && level(100) == 100, "buckets high");
        check(GatedPhoneService.batteryLevel(15).getMobileChargeLevel1().isCritical(), "critical");
        check(!GatedPhoneService.batteryLevel(20).getMobileChargeLevel1().isCritical(), "not critical");
        System.out.println("VehicleDataEncodingTest: CMD_VEHICLE layout, unit conversions, battery buckets PASS");
    }
}
