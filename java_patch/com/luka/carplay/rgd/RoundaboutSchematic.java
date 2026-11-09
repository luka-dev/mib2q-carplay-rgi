package com.luka.carplay.rgd;

/** Deliberately synthetic presentation of an ordinal, never measured geometry.
 * Both outputs share the HUD's 22.5-degree grid, expressed in half-degrees.
 * Reserve one direction for the entry and one exit AFTER the selected exit. */
final class RoundaboutSchematic {
    final int exitAngle;
    final int[] junctionAngles;
    final int direction;
    final byte[] sideStreets;

    private RoundaboutSchematic(int ordinal, int side) {
        junctionAngles = new int[ordinal + 1];
        sideStreets = new byte[ordinal];
        int next = 0;
        for (int i = 1; i <= ordinal + 1; i++) {
            // Nearest distinct grid point; entry is bin 0/16. With ordinal<=14
            // every exit occupies a different bin, with no wrap onto the entry.
            int bin = (16 * i + (ordinal + 2) / 2) / (ordinal + 2);
            int angle = 360 - bin * 45;
            if (side == ManeuverMapper.DRIVING_SIDE_LEFT) angle = -angle;
            junctionAngles[i - 1] = angle;
            if (i != ordinal) sideStreets[next++] = (byte) direction(angle);
        }
        exitAngle = junctionAngles[ordinal - 1];
        direction = direction(exitAngle);
    }

    static RoundaboutSchematic create(ManeuverSemantics value, int[] receivedRoads) {
        if (value.exitNumber < 1 || value.exitNumber > 14 || value.hasAngle()
                || value.junctionType != ManeuverMapper.JUNCTION_ROUNDABOUT
                || (value.drivingSide != ManeuverMapper.DRIVING_SIDE_LEFT
                    && value.drivingSide != ManeuverMapper.DRIVING_SIDE_RIGHT)) return null;
        // Do not mix fictional positions with even partial real geometry.
        if (receivedRoads != null) {
            for (int i = 0; i < receivedRoads.length; i++)
                if (receivedRoads[i] >= -180 && receivedRoads[i] <= 180) return null;
        }
        return new RoundaboutSchematic(value.exitNumber, value.drivingSide);
    }

    private static int direction(int halfDegrees) {
        return (-(halfDegrees / 45) * 16) & 255;
    }
}
