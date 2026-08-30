/*
 * A CarPlay feature module (route guidance, cover art, cluster, ...).
 * Lifecycle owned by CarPlayApp: start() on CarPlay-connect, stop() on disconnect.
 */
package com.luka.carplay.core;

public interface Module {
    /** short name for logs. */
    String name();

    /**
     * Start the module.  Return false if a required service/model is not up yet —
     * CarPlayApp will retry (mirrors the proven old-hook retry-until-ready loop).
     */
    boolean start(FrameworkRef fw);

    /** Stop + release; must be idempotent (may be called without a prior start). */
    void stop();
}
