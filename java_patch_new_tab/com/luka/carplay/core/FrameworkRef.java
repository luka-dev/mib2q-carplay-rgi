/*
 * FrameworkRef — one place that resolves the stock HMI handles from the CarPlay
 * activation context, so modules don't each re-derive them.
 *
 * Seam (verified against the MU1316 decompile + the proven old hook):
 *   context : de.audi.app.terminalmode.IContext
 *     .getFramework()      -> IFrameworkAccess  (HMI service app, storage, DSI svc…)
 *     .getServiceManager() -> IServiceManager   (OSGi service lookup)
 *     .getDeviceManager()  -> IDeviceManager
 *   IFrameworkAccess.getHmiServiceApp() -> IHMIServiceApp
 *     (the object TMVirtualButtonListener uses: getVirtualButtonModel(...) — our roller).
 */
package com.luka.carplay.core;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.device.IDeviceManager;
import de.audi.app.terminalmode.osgi.IServiceManager;
import de.audi.atip.base.IFrameworkAccess;
import de.audi.atip.hmi.IHMIServiceApp;

import org.osgi.framework.ServiceReference;

public final class FrameworkRef {
    private final IContext ctx;
    private final IFrameworkAccess fw;

    public FrameworkRef(IContext ctx) {
        this.ctx = ctx;
        this.fw = (ctx != null) ? ctx.getFramework() : null;
    }

    public boolean isReady() { return ctx != null && fw != null; }

    public IContext context()               { return ctx; }
    public IFrameworkAccess framework()      { return fw; }
    public IServiceManager serviceManager()  { return ctx != null ? ctx.getServiceManager() : null; }
    public IDeviceManager deviceManager()    { return ctx != null ? ctx.getDeviceManager() : null; }
    public IHMIServiceApp hmiServiceApp()    { return fw != null ? fw.getHmiServiceApp() : null; }

    /** OSGi service lookup by interface; null if none registered yet (caller retries). */
    public Object getService(Class iface) {
        IServiceManager sm = serviceManager();
        if (sm == null || iface == null) return null;
        ServiceReference[] refs = sm.getServiceReferences(iface);
        if (refs == null || refs.length == 0) return null;
        return sm.getService(refs[0]);
    }
}
