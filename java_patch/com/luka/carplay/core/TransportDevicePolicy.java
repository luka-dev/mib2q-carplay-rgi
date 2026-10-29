package com.luka.carplay.core;

import java.util.Hashtable;
import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.device.TMDevice;
import de.audi.atip.utils.generics.GIterator;
import de.audi.atip.utils.generics.GList;
import org.dsi.ifc.smartphoneintegration.Device;

/** Presentation and initial checkbox defaults only. SI owns identities,
 * availability, connection methods and every connect/disconnect operation. */
public final class TransportDevicePolicy {
    private static final int CARPLAY = 2;
    private static final int USB = 1;
    private static final int WLAN = 2;
    private static final int REMEMBER_CHOICE = 3200034;
    private static final Hashtable transports = new Hashtable();

    private TransportDevicePolicy() { }

    /** Never mutate the DSI object shared with other native/HMI listeners. */
    public static synchronized Device presentation(Device source) {
        if (source == null) return null;
        boolean carplay = (source.connectionMethod & CARPLAY) != 0;
        if (source.deviceAddress != null && source.deviceAddress.length() > 0) {
            if (carplay && (source.connectionType == USB || source.connectionType == WLAN)) {
                if (transports.size() < 100 || transports.containsKey(source.deviceAddress)) {
                    transports.put(source.deviceAddress, new Integer(source.connectionType));
                }
            } else {
                transports.remove(source.deviceAddress);
            }
        }
        String name = source.deviceName;
        // Stock TMDeviceControl recognizes this exact USB enumeration placeholder
        // and keeps the previously learned real name. Preserve that comparison.
        if (carplay && name != null && name.length() > 0 && !"Apple Inc.-iPhone".equals(name)) {
            String prefix = source.connectionType == USB ? "Wired - "
                : source.connectionType == WLAN ? "Wireless - " : null;
            if (prefix != null && !name.startsWith(prefix)) name = prefix + name;
        }
        return new Device(source.deviceID, name, source.connectionType,
                          source.deviceAddress, source.connectionMethod);
    }

    private static synchronized boolean wireless(TMDevice device) {
        if (device == null || !device.isCarplayDevice()) return false;
        Integer transport = (Integer)transports.get(device.getUniqueID().address);
        return transport != null && transport.intValue() == WLAN;
    }

    /** Called before the initial popup becomes visible, never on Yes/No.
     * Stock handlers still read the checkbox and persist the user's choice. */
    public static void prepareDisclaimer(IContext context, TMDevice device) {
        if (wireless(device)) context.getChoiceModel(REMEMBER_CHOICE).setValue(1);
    }

    /** A mixed-device popup keeps the stock default. */
    public static void preparePopup(IContext context, GList devices) {
        if (devices == null || devices.isEmpty()) return;
        GIterator iterator = devices.iterator();
        while (iterator.hasNext()) if (!wireless((TMDevice)iterator.next())) return;
        context.getChoiceModel(REMEMBER_CHOICE).setValue(1);
    }
}
