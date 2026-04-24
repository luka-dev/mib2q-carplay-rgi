/*
 * Reflection-based injector for the DSIKombiSync proxy filter.
 *
 * Looks up the live stock DSIKombiSync provider via OSGi, finds its
 * private DSIKombiSyncProxy field, and replaces it with a
 * FilteringKombiProxy that wraps the original.  After install(), every
 * setMMIDisplayStatus() call from any HMI bundle goes through our
 * filter (Phone→Map rewrite + optional flap filter) before reaching
 * the wire.
 *
 * This avoids the OSGi bundle isolation problem that made the earlier
 * class-replacement approach silently no-op (logged "no provider
 * instance" and never executed our patched class).
 */
package com.luka.carplay.kombi;

import com.luka.carplay.framework.Log;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.osgi.IServiceManager;

import de.esolutions.fw.comm.dsi.kombisync2.DSIKombiSyncReply;
import de.esolutions.fw.comm.dsi.kombisync2.impl.DSIKombiSyncProxy;

import java.lang.reflect.Field;

import org.dsi.ifc.kombisync2.DSIKombiSync;
import org.osgi.framework.ServiceReference;

public class KombiInjector {

    private static final String TAG = "KombiInjector";

    /* Saved state for uninstall() */
    private static volatile boolean installed = false;
    private static volatile Object savedProvider = null;
    private static volatile DSIKombiSyncProxy savedStockProxy = null;
    private static volatile Field savedProxyField = null;

    /**
     * Install the filter.  Idempotent (returns true if already installed).
     * Call from CarPlayHook.tryInit() after the IContext is known.
     */
    public static synchronized boolean install(Object context) {
        if (installed) {
            return true;
        }

        try {
            if (!(context instanceof IContext)) {
                Log.w(TAG, "install: context is not IContext");
                return false;
            }
            IServiceManager sm = ((IContext) context).getServiceManager();
            if (sm == null) {
                Log.w(TAG, "install: getServiceManager returned null");
                return false;
            }

            ServiceReference[] refs = sm.getServiceReferences(DSIKombiSync.class);
            if (refs == null || refs.length == 0) {
                Log.w(TAG, "install: DSIKombiSync service not registered yet");
                return false;
            }

            Object provider = sm.getService(refs[0]);
            if (provider == null) {
                Log.w(TAG, "install: getService returned null");
                return false;
            }

            String providerClass = provider.getClass().getName();
            Log.i(TAG, "install: stock provider = " + providerClass);

            /* Find the DSIKombiSyncProxy-typed field on the provider
             * (or one of its superclasses).  Stock name is "proxy" but
             * search by type to be resilient to obfuscation. */
            Field proxyField = findProxyField(provider.getClass());
            if (proxyField == null) {
                Log.e(TAG, "install: no DSIKombiSyncProxy field on provider");
                return false;
            }
            proxyField.setAccessible(true);
            DSIKombiSyncProxy stockProxy = (DSIKombiSyncProxy) proxyField.get(provider);
            if (stockProxy == null) {
                Log.e(TAG, "install: stock proxy field is null");
                return false;
            }

            /* Pull AbstractProvider.instance + .dispatcher to feed our
             * super(...) call.  Without these we'd have to pass null and
             * trip an NPE inside DSIKombiSyncReplyService. */
            Field instanceField = findFieldUpClass(provider.getClass(), "instance");
            Field dispatcherField = findFieldUpClass(provider.getClass(), "dispatcher");
            if (instanceField == null || dispatcherField == null) {
                Log.e(TAG, "install: AbstractProvider instance/dispatcher fields missing");
                return false;
            }
            instanceField.setAccessible(true);
            dispatcherField.setAccessible(true);
            int instanceId = instanceField.getInt(provider);
            Object dispatcher = dispatcherField.get(provider);

            FilteringKombiProxy wrapper = new FilteringKombiProxy(
                    instanceId,
                    (DSIKombiSyncReply) dispatcher,
                    stockProxy);

            proxyField.set(provider, wrapper);

            savedProvider = provider;
            savedStockProxy = stockProxy;
            savedProxyField = proxyField;
            installed = true;

            Log.i(TAG, "install: reflection swap COMPLETE — wrapper is now active");
            return true;

        } catch (Throwable t) {
            Log.e(TAG, "install failed", t);
            return false;
        }
    }

    /**
     * Restore stock proxy on the provider.  Safe to call multiple times.
     */
    public static synchronized void uninstall() {
        if (!installed) {
            return;
        }
        try {
            savedProxyField.set(savedProvider, savedStockProxy);
            Log.i(TAG, "uninstall: reverted to stock proxy");
        } catch (Throwable t) {
            Log.e(TAG, "uninstall failed", t);
        } finally {
            installed = false;
            savedProvider = null;
            savedStockProxy = null;
            savedProxyField = null;
        }
    }

    public static boolean isInstalled() {
        return installed;
    }

    /* ============================================================
     * Reflection helpers
     * ============================================================ */

    private static Field findProxyField(Class providerCls) {
        Class cls = providerCls;
        while (cls != null) {
            Field[] fs = cls.getDeclaredFields();
            for (int i = 0; i < fs.length; i++) {
                Class type = fs[i].getType();
                if (DSIKombiSyncProxy.class.isAssignableFrom(type)) {
                    return fs[i];
                }
            }
            cls = cls.getSuperclass();
        }
        return null;
    }

    private static Field findFieldUpClass(Class providerCls, String name) {
        Class cls = providerCls;
        while (cls != null) {
            try {
                return cls.getDeclaredField(name);
            } catch (NoSuchFieldException nsfe) {
                cls = cls.getSuperclass();
            }
        }
        return null;
    }
}
