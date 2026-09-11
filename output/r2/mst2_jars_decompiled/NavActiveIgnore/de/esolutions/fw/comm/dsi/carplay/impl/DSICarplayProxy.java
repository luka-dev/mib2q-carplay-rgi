/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  de.esolutions.fw.comm.core.CallContext
 *  de.esolutions.fw.comm.core.IReplyService
 *  de.esolutions.fw.comm.core.Proxy
 *  de.esolutions.fw.comm.core.ServiceInstanceID
 *  de.esolutions.fw.comm.core.method.MethodException
 *  de.esolutions.fw.comm.dsi.carplay.DSICarplay
 *  de.esolutions.fw.comm.dsi.carplay.DSICarplayC
 *  de.esolutions.fw.comm.dsi.carplay.DSICarplayReply
 *  de.esolutions.fw.comm.dsi.carplay.impl.DSICarplayProxy$1
 *  de.esolutions.fw.comm.dsi.carplay.impl.DSICarplayProxy$2
 *  de.esolutions.fw.comm.dsi.carplay.impl.DSICarplayProxy$3
 *  de.esolutions.fw.comm.dsi.carplay.impl.DSICarplayProxy$4
 *  de.esolutions.fw.comm.dsi.carplay.impl.DSICarplayReplyService
 *  de.esolutions.fw.util.serializer.ISerializable
 *  de.esolutions.fw.util.serializer.adapter.GenericSerializable
 *  de.esolutions.fw.util.serializer.exception.SerializerException
 *  org.dsi.ifc.carplay.AppState
 *  org.dsi.ifc.carplay.AppStateRequest
 *  org.dsi.ifc.carplay.Resource
 *  org.dsi.ifc.carplay.ResourceRequest
 *  org.dsi.ifc.carplay.ServiceConfiguration
 *  org.dsi.ifc.carplay.TouchEvent
 */
package de.esolutions.fw.comm.dsi.carplay.impl;

import de.esolutions.fw.comm.core.CallContext;
import de.esolutions.fw.comm.core.IReplyService;
import de.esolutions.fw.comm.core.Proxy;
import de.esolutions.fw.comm.core.ServiceInstanceID;
import de.esolutions.fw.comm.core.method.MethodException;
import de.esolutions.fw.comm.dsi.carplay.DSICarplay;
import de.esolutions.fw.comm.dsi.carplay.DSICarplayC;
import de.esolutions.fw.comm.dsi.carplay.DSICarplayReply;
import de.esolutions.fw.comm.dsi.carplay.impl.DSICarplayProxy;
import de.esolutions.fw.comm.dsi.carplay.impl.DSICarplayReplyService;
import de.esolutions.fw.util.serializer.ISerializable;
import de.esolutions.fw.util.serializer.adapter.GenericSerializable;
import de.esolutions.fw.util.serializer.exception.SerializerException;
import org.dsi.ifc.carplay.AppState;
import org.dsi.ifc.carplay.AppStateRequest;
import org.dsi.ifc.carplay.Resource;
import org.dsi.ifc.carplay.ResourceRequest;
import org.dsi.ifc.carplay.ServiceConfiguration;
import org.dsi.ifc.carplay.TouchEvent;

public class DSICarplayProxy
implements DSICarplay,
DSICarplayC {
    private static CallContext context = CallContext.getContext((String)"PROXY.dsi.carplay.DSICarplay");
    private Proxy proxy;

    public DSICarplayProxy(int n, DSICarplayReply dSICarplayReply) {
        ServiceInstanceID serviceInstanceID = new ServiceInstanceID("b60ff3e0-c276-5843-8023-5aba5e2ac150", n, "44e3fa4c-73e2-5fd1-a140-725298101c75", "dsi.carplay.DSICarplay");
        DSICarplayReplyService dSICarplayReplyService = new DSICarplayReplyService(dSICarplayReply);
        this.proxy = new Proxy(serviceInstanceID, (IReplyService)dSICarplayReplyService, context);
    }

    public Proxy getProxy() {
        return this.proxy;
    }

    public void startService(ServiceConfiguration serviceConfiguration) {
        1 var2_2 = new 1(this, serviceConfiguration);
        this.proxy.remoteCallMethod((short)46, (ISerializable)var2_2);
    }

    public void postButtonEvent(int n, int n2) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putInt32(n);
            genericSerializable.putInt32(n2);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)4, (ISerializable)genericSerializable);
    }

    public void postTouchEvent(int n, int n2, TouchEvent[] touchEventArray) {
        2 var4_4 = new 2(this, n, n2, touchEventArray);
        this.proxy.remoteCallMethod((short)7, (ISerializable)var4_4);
    }

    public void postRotaryEvent(int n) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putInt32(n);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)6, (ISerializable)genericSerializable);
    }

    public void postCharacterEvent(int n, String[] stringArray) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putInt32(n);
            genericSerializable.putOptionalStringVarArray(stringArray);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)5, (ISerializable)genericSerializable);
    }

    public void requestModeChange(ResourceRequest[] resourceRequestArray, AppStateRequest[] appStateRequestArray, String string) {
        boolean bl = false;
        if (appStateRequestArray != null) {
            for (int i = 0; i < appStateRequestArray.length; ++i) {
                AppStateRequest appStateRequest = appStateRequestArray[i];
                String string2 = "";
                if (appStateRequest.appStateID == 2 && appStateRequest.state && appStateRequest.speechMode == 0) {
                    string2 = " (ignored)";
                    bl = true;
                }
                System.out.println("DSICarplayProxy.requestModeChange(" + string + "): " + appStateRequest.toString() + string2);
            }
        } else {
            System.out.println("DSICarplayProxy.requestModeChange(" + string + "): " + resourceRequestArray);
        }
        new Exception("Stack trace").printStackTrace();
        if (bl) {
            return;
        }
        3 var5_6 = new 3(this, resourceRequestArray, appStateRequestArray, string);
        this.proxy.remoteCallMethod((short)9, (ISerializable)var5_6);
    }

    public void responseUpdateMode(Resource[] resourceArray, AppState[] appStateArray) {
        4 var3_3 = new 4(this, resourceArray, appStateArray);
        this.proxy.remoteCallMethod((short)38, (ISerializable)var3_3);
    }

    public void responseBTDeactivation() {
        this.proxy.remoteCallMethod((short)12, null);
    }

    public void requestUI(int n) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putInt32(n);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)11, (ISerializable)genericSerializable);
    }

    public void requestNightMode(boolean bl) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putBool(bl);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)10, (ISerializable)genericSerializable);
    }

    public void requestSIRIAction(int n) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putInt32(n);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)34, (ISerializable)genericSerializable);
    }

    public void responseUpdateMainAudioType(int n) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putInt32(n);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)42, (ISerializable)genericSerializable);
    }

    public void requestUI2(String string) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putOptionalString(string);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)47, (ISerializable)genericSerializable);
    }

    public void setNotification(int[] nArray) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putOptionalInt32VarArray(nArray);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)15, (ISerializable)genericSerializable);
    }

    public void setNotification(int n) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putInt32(n);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)16, (ISerializable)genericSerializable);
    }

    public void setNotification() {
        this.proxy.remoteCallMethod((short)14, null);
    }

    public void clearNotification(int[] nArray) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putOptionalInt32VarArray(nArray);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)2, (ISerializable)genericSerializable);
    }

    public void clearNotification(int n) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putInt32(n);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)3, (ISerializable)genericSerializable);
    }

    public void clearNotification() {
        this.proxy.remoteCallMethod((short)1, null);
    }

    public void yySet(String string, String string2) {
        GenericSerializable genericSerializable = new GenericSerializable();
        try {
            genericSerializable.putOptionalString(string);
            genericSerializable.putOptionalString(string2);
        }
        catch (SerializerException serializerException) {
            throw new MethodException(serializerException.getMessage());
        }
        this.proxy.remoteCallMethod((short)31, (ISerializable)genericSerializable);
    }
}

