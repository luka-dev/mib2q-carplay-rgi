package com.luka.carplay.rgd;

/** Stateless, ordered transformation of a fresh semantic value. */
interface ManeuverCompensation {
    void apply(ManeuverSemantics value);
}
