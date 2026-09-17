package com.luka.carplay.pdc;

import com.luka.carplay.framework.Log;
import de.audi.atip.hmi.model.IntegerListCell;
import de.audi.atip.hmi.modelaccess.ListModelApp;
import de.audi.atip.interapp.audio.drawer.AudioDrawerContext;

/** Masks only the visual APS request in audio-drawer model 538. The parking
 * controller and its latest requested state remain authoritative. No audio
 * resource, parking component, or other drawer column is changed. */
public final class OpsAudioDrawerPolicy {
    private static volatile boolean suppressed;
    private static OpsAudioDrawerPolicy current;
    private final ListModelApp model;
    private IntegerListCell requested = AudioDrawerContext.LIST_CELL_INACTIVE;
    private boolean publishing;

    public OpsAudioDrawerPolicy(ListModelApp model) {
        this.model = model;
        synchronized (OpsAudioDrawerPolicy.class) {
            current = this;
        }
    }

    public static void setSuppressed(boolean value) {
        OpsAudioDrawerPolicy target;
        synchronized (OpsAudioDrawerPolicy.class) {
            if (suppressed == value) return;
            suppressed = value;
            target = current;
        }
        if (target != null) {
            try {
                target.publish();
            } catch (RuntimeException e) {
                Log.w("PDC", "APS drawer model update failed: " + e);
            }
        }
    }

    public void setRequested(IntegerListCell cell) {
        synchronized (this) {
            requested = cell;
        }
        publish();
    }

    private void publish() {
        synchronized (this) {
            if (publishing) return;
            publishing = true;
        }
        boolean released = false;
        try {
            for (;;) {
                IntegerListCell snapshot;
                boolean hidden;
                synchronized (this) {
                    snapshot = requested;
                    hidden = suppressed;
                }
                IntegerListCell visible = hidden ? AudioDrawerContext.LIST_CELL_INACTIVE : snapshot;
                // Model callbacks can re-enter setContext or change parking state.
                // Serialize writes without holding a policy lock across callbacks.
                if (model.getCell(0, AudioDrawerContext.PRIO_IDX_APS) != visible) {
                    model.setCell(0, AudioDrawerContext.PRIO_IDX_APS, visible);
                }
                synchronized (this) {
                    if (requested == snapshot && suppressed == hidden) {
                        publishing = false;
                        released = true;
                        return;
                    }
                }
            }
        } finally {
            if (!released) {
                synchronized (this) {
                    publishing = false;
                }
            }
        }
    }
}
