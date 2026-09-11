package de.esolutions.fw.comm.dsi.carplay.impl;

import de.esolutions.fw.comm.core.CallContext;
import de.esolutions.fw.comm.core.Proxy;
import de.esolutions.fw.comm.core.ServiceInstanceID;
import de.esolutions.fw.comm.core.method.MethodException;
import de.esolutions.fw.comm.dsi.carplay.DSICarplay;
import de.esolutions.fw.comm.dsi.carplay.DSICarplayC;
import de.esolutions.fw.comm.dsi.carplay.DSICarplayReply;
import de.esolutions.fw.comm.dsi.carplay.impl.DSICarplayProxy.1;
import de.esolutions.fw.comm.dsi.carplay.impl.DSICarplayProxy.2;
import de.esolutions.fw.comm.dsi.carplay.impl.DSICarplayProxy.3;
import de.esolutions.fw.comm.dsi.carplay.impl.DSICarplayProxy.4;
import de.esolutions.fw.util.serializer.adapter.GenericSerializable;
import de.esolutions.fw.util.serializer.exception.SerializerException;
import org.dsi.ifc.carplay.AppState;
import org.dsi.ifc.carplay.AppStateRequest;
import org.dsi.ifc.carplay.Resource;
import org.dsi.ifc.carplay.ResourceRequest;
import org.dsi.ifc.carplay.ServiceConfiguration;
import org.dsi.ifc.carplay.TouchEvent;

public class DSICarplayProxy implements DSICarplay, DSICarplayC {
   private static CallContext context = CallContext.getContext("PROXY.dsi.carplay.DSICarplay");
   private Proxy proxy;

   public DSICarplayProxy(int var1, DSICarplayReply var2) {
      ServiceInstanceID var3 = new ServiceInstanceID(
         "b60ff3e0-c276-5843-8023-5aba5e2ac150", var1, "44e3fa4c-73e2-5fd1-a140-725298101c75", "dsi.carplay.DSICarplay"
      );
      DSICarplayReplyService var4 = new DSICarplayReplyService(var2);
      this.proxy = new Proxy(var3, var4, context);
   }

   public Proxy getProxy() {
      return this.proxy;
   }

   public void startService(ServiceConfiguration var1) {
      1 var2 = new 1(this, var1);
      this.proxy.remoteCallMethod((short)46, var2);
   }

   public void postButtonEvent(int var1, int var2) {
      GenericSerializable var3 = new GenericSerializable();

      try {
         var3.putInt32(var1);
         var3.putInt32(var2);
      } catch (SerializerException var5) {
         throw new MethodException(var5.getMessage());
      }

      this.proxy.remoteCallMethod((short)4, var3);
   }

   public void postTouchEvent(int var1, int var2, TouchEvent[] var3) {
      2 var4 = new 2(this, var1, var2, var3);
      this.proxy.remoteCallMethod((short)7, var4);
   }

   public void postRotaryEvent(int var1) {
      GenericSerializable var2 = new GenericSerializable();

      try {
         var2.putInt32(var1);
      } catch (SerializerException var4) {
         throw new MethodException(var4.getMessage());
      }

      this.proxy.remoteCallMethod((short)6, var2);
   }

   public void postCharacterEvent(int var1, String[] var2) {
      GenericSerializable var3 = new GenericSerializable();

      try {
         var3.putInt32(var1);
         var3.putOptionalStringVarArray(var2);
      } catch (SerializerException var5) {
         throw new MethodException(var5.getMessage());
      }

      this.proxy.remoteCallMethod((short)5, var3);
   }

   public void requestModeChange(ResourceRequest[] var1, AppStateRequest[] var2, String var3) {
      boolean var4 = false;
      if (var2 != null) {
         for (int var5 = 0; var5 < var2.length; var5++) {
            AppStateRequest var6 = var2[var5];
            String var7 = "";
            if (var6.appStateID == 2 && var6.state && var6.speechMode == 0) {
               var7 = " (ignored)";
               var4 = true;
            }

            System.out.println("DSICarplayProxy.requestModeChange(" + var3 + "): " + var6.toString() + var7);
         }
      } else {
         System.out.println("DSICarplayProxy.requestModeChange(" + var3 + "): " + var1);
      }

      new Exception("Stack trace").printStackTrace();
      if (!var4) {
         3 var8 = new 3(this, var1, var2, var3);
         this.proxy.remoteCallMethod((short)9, var8);
      }
   }

   public void responseUpdateMode(Resource[] var1, AppState[] var2) {
      4 var3 = new 4(this, var1, var2);
      this.proxy.remoteCallMethod((short)38, var3);
   }

   public void responseBTDeactivation() {
      this.proxy.remoteCallMethod((short)12, null);
   }

   public void requestUI(int var1) {
      GenericSerializable var2 = new GenericSerializable();

      try {
         var2.putInt32(var1);
      } catch (SerializerException var4) {
         throw new MethodException(var4.getMessage());
      }

      this.proxy.remoteCallMethod((short)11, var2);
   }

   public void requestNightMode(boolean var1) {
      GenericSerializable var2 = new GenericSerializable();

      try {
         var2.putBool(var1);
      } catch (SerializerException var4) {
         throw new MethodException(var4.getMessage());
      }

      this.proxy.remoteCallMethod((short)10, var2);
   }

   public void requestSIRIAction(int var1) {
      GenericSerializable var2 = new GenericSerializable();

      try {
         var2.putInt32(var1);
      } catch (SerializerException var4) {
         throw new MethodException(var4.getMessage());
      }

      this.proxy.remoteCallMethod((short)34, var2);
   }

   public void responseUpdateMainAudioType(int var1) {
      GenericSerializable var2 = new GenericSerializable();

      try {
         var2.putInt32(var1);
      } catch (SerializerException var4) {
         throw new MethodException(var4.getMessage());
      }

      this.proxy.remoteCallMethod((short)42, var2);
   }

   public void requestUI2(String var1) {
      GenericSerializable var2 = new GenericSerializable();

      try {
         var2.putOptionalString(var1);
      } catch (SerializerException var4) {
         throw new MethodException(var4.getMessage());
      }

      this.proxy.remoteCallMethod((short)47, var2);
   }

   public void setNotification(int[] var1) {
      GenericSerializable var2 = new GenericSerializable();

      try {
         var2.putOptionalInt32VarArray(var1);
      } catch (SerializerException var4) {
         throw new MethodException(var4.getMessage());
      }

      this.proxy.remoteCallMethod((short)15, var2);
   }

   public void setNotification(int var1) {
      GenericSerializable var2 = new GenericSerializable();

      try {
         var2.putInt32(var1);
      } catch (SerializerException var4) {
         throw new MethodException(var4.getMessage());
      }

      this.proxy.remoteCallMethod((short)16, var2);
   }

   public void setNotification() {
      this.proxy.remoteCallMethod((short)14, null);
   }

   public void clearNotification(int[] var1) {
      GenericSerializable var2 = new GenericSerializable();

      try {
         var2.putOptionalInt32VarArray(var1);
      } catch (SerializerException var4) {
         throw new MethodException(var4.getMessage());
      }

      this.proxy.remoteCallMethod((short)2, var2);
   }

   public void clearNotification(int var1) {
      GenericSerializable var2 = new GenericSerializable();

      try {
         var2.putInt32(var1);
      } catch (SerializerException var4) {
         throw new MethodException(var4.getMessage());
      }

      this.proxy.remoteCallMethod((short)3, var2);
   }

   public void clearNotification() {
      this.proxy.remoteCallMethod((short)1, null);
   }

   public void yySet(String var1, String var2) {
      GenericSerializable var3 = new GenericSerializable();

      try {
         var3.putOptionalString(var1);
         var3.putOptionalString(var2);
      } catch (SerializerException var5) {
         throw new MethodException(var5.getMessage());
      }

      this.proxy.remoteCallMethod((short)31, var3);
   }
}
