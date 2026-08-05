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

    /** Paired lookup handle: callers with a session lifecycle must release it. */
    public ServiceHandle getServiceHandle(Class iface) {
        IServiceManager sm = serviceManager();
        if (sm == null || iface == null) return null;
        ServiceReference[] refs = sm.getServiceReferences(iface);
        if (refs == null || refs.length == 0) return null;
        /* A remove/add race can leave a stale reference first in the array.  Walk all published
         * references and pair the exact successful one with its later release. */
        for (int i = 0; i < refs.length; i++) {
            Object service = sm.getService(refs[i]);
            if (service != null) return new ServiceHandle(sm, refs[i], service);
        }
        return null;
    }

    public static final class ServiceHandle {
        private IServiceManager manager;
        private ServiceReference reference;
        private Object service;

        private ServiceHandle(IServiceManager manager, ServiceReference reference, Object service) {
            this.manager = manager;
            this.reference = reference;
            this.service = service;
        }

        public synchronized Object service() { return service; }

        public synchronized void release() {
            if (manager != null && reference != null) {
                try { manager.releaseService(reference); } catch (Throwable t) { }
            }
            manager = null;
            reference = null;
            service = null;
        }
    }
}
