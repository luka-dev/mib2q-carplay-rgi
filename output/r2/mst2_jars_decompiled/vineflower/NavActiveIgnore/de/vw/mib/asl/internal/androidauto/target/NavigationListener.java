package de.vw.mib.asl.internal.androidauto.target;

import de.vw.mib.asl.api.navigation.ASLNavigationServices;
import de.vw.mib.asl.api.navigation.AbstractASLNavigationServicesListener;

public class NavigationListener extends AbstractASLNavigationServicesListener {
   private AndroidAutoTarget target;
   private StartupHandler startupHandler;
   private AndroidAutoGlobalProperties properties;
   private ASLNavigationServices navServices;
   private boolean stopGuidancePending = false;

   public NavigationListener(AndroidAutoTarget var1, AndroidAutoGlobalProperties var2, StartupHandler var3) {
      this.target = var1;
      this.properties = var2;
      this.startupHandler = var3;
   }

   public void initNavServices(ASLNavigationServices var1) {
      this.navServices = var1;
   }

   public void updateGuidanceActive(boolean var1) {
      if (this.target.isTraceEnabled()) {
         this.target.trace("NavigationHandler::updateGuidanceActive (ignored) - flag = " + var1);
      }
   }

   public void updateServiceAvailable(boolean var1) {
      if (var1 && this.stopGuidancePending) {
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
