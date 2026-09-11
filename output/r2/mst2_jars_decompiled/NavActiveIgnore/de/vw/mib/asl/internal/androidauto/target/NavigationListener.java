/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  de.vw.mib.asl.api.navigation.ASLNavigationServices
 *  de.vw.mib.asl.api.navigation.AbstractASLNavigationServicesListener
 *  de.vw.mib.asl.internal.androidauto.target.AndroidAutoGlobalProperties
 *  de.vw.mib.asl.internal.androidauto.target.AndroidAutoTarget
 *  de.vw.mib.asl.internal.androidauto.target.StartupHandler
 */
package de.vw.mib.asl.internal.androidauto.target;

import de.vw.mib.asl.api.navigation.ASLNavigationServices;
import de.vw.mib.asl.api.navigation.AbstractASLNavigationServicesListener;
import de.vw.mib.asl.internal.androidauto.target.AndroidAutoGlobalProperties;
import de.vw.mib.asl.internal.androidauto.target.AndroidAutoTarget;
import de.vw.mib.asl.internal.androidauto.target.StartupHandler;

public class NavigationListener
extends AbstractASLNavigationServicesListener {
    private AndroidAutoTarget target;
    private StartupHandler startupHandler;
    private AndroidAutoGlobalProperties properties;
    private ASLNavigationServices navServices;
    private boolean stopGuidancePending = false;

    public NavigationListener(AndroidAutoTarget androidAutoTarget, AndroidAutoGlobalProperties androidAutoGlobalProperties, StartupHandler startupHandler) {
        this.target = androidAutoTarget;
        this.properties = androidAutoGlobalProperties;
        this.startupHandler = startupHandler;
    }

    public void initNavServices(ASLNavigationServices aSLNavigationServices) {
        this.navServices = aSLNavigationServices;
    }

    public void updateGuidanceActive(boolean bl) {
        if (this.target.isTraceEnabled()) {
            this.target.trace("NavigationHandler::updateGuidanceActive (ignored) - flag = " + bl);
        }
    }

    public void updateServiceAvailable(boolean bl) {
        if (bl && this.stopGuidancePending) {
            this.stopGuidancePending = false;
            this.stopGuidance();
        }
    }

    public void stopGuidance() {
        if (this.target.isTraceEnabled()) {
            this.target.trace("NavigationHandler::stopGuidance (ignored)");
        }
    }

    public boolean isGuidanceActive() {
        return false;
    }
}

