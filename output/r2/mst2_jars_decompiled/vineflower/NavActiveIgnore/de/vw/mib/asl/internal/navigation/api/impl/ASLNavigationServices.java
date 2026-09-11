package de.vw.mib.asl.internal.navigation.api.impl;

import de.vw.mib.asl.api.navigation.ASLNavigationServicesListener;
import de.vw.mib.asl.api.navigation.GuidanceInfo;
import de.vw.mib.asl.api.navigation.ILocationWrapper;
import de.vw.mib.asl.api.navigation.NavAddress;
import de.vw.mib.asl.api.navigation.map.ASLNavigationMapFactory;
import de.vw.mib.asl.api.navigation.map.ASLNavigationServiceMainMap;
import de.vw.mib.asl.api.navigation.memory.ASLNavigationMemoryFactory;
import de.vw.mib.asl.api.navigation.poi.ASLNavigationPoiFactory;
import de.vw.mib.asl.api.navigation.util.ASLNavigationUtilFactory;
import de.vw.mib.asl.internal.navigation.gateway.INavGateway;
import de.vw.mib.genericevents.AbstractTarget;
import org.dsi.ifc.global.NavLocation;
import org.dsi.ifc.navigation.NavRouteListData;
import org.dsi.ifc.navigation.RgInfoForNextDestination;
import org.dsi.ifc.navigation.Route;
import org.dsi.ifc.navigation.RouteDestination;
import org.dsi.ifc.organizer.AdbEntry;

public class ASLNavigationServices implements de.vw.mib.asl.api.navigation.ASLNavigationServices {
   private ASLNavigationServicesListener listener;
   private TargetASLNavigationServices target;

   static NavAddress createNavAddress(ILocationWrapper var0) {
      NavAddress var1 = new NavAddress();
      var1.setCity(var0.getCity());
      var1.setCountry(var0.getCountry());
      var1.setCountryCode(var0.getCountryCode());
      var1.setCrossing(var0.getCrossing());
      var1.setHousenumber(var0.getHousenumber());
      var1.setLatitude(ASLNavigationUtilFactory.getNavigationUtilApi().getNaviHelper().dsiToDecimal(var0.getLatitude()));
      var1.setLongitude(ASLNavigationUtilFactory.getNavigationUtilApi().getNaviHelper().dsiToDecimal(var0.getLongitude()));
      var1.setName(var0.getPoiName());
      var1.setPhoneNumber(var0.getPhonenumber());
      var1.setState(var0.getState());
      var1.setStateAbbreviation(var0.getStateAbbreviation());
      var1.setStreet(var0.getStreet());
      var1.setUrl("");
      var1.setZip(var0.getPostalCode());
      return var1;
   }

   public ASLNavigationServices(ASLNavigationServicesListener var1, TargetASLNavigationServices var2) {
      this.target = var2;
      this.listener = var1;
   }

   public ASLNavigationServiceMainMap createMainMapService(AbstractTarget var1, int[] var2) {
      return ASLNavigationMapFactory.getNavigationMapApi().createMainMapService(var1, var2);
   }

   public int getContactCapacity() {
      return this.isServiceAvailable() ? this.target.getContactCapacity() : 0;
   }

   public GuidanceInfo[] getGuidanceInfo() {
      if (this.isServiceAvailable()) {
         GuidanceInfo[] var1 = new GuidanceInfo[2];
         INavGateway var2 = ASLNavigationUtilFactory.getNavigationUtilApi().getNavGateway();
         Route var3 = var2.getRgCurrentRoute();
         if (var3 != null && var3.getRoutelist() != null && var3.getRoutelist().length > 0) {
            RgInfoForNextDestination var4 = var2.getRgInfoForNextDestination();
            NavRouteListData[] var5 = var2.getRgDestinationInfo();
            RouteDestination[] var6 = var3.getRoutelist();
            NavLocation var7 = var6[var6.length - 1].getRouteLocation();
            ILocationWrapper var8 = var7 != null
               ? ASLNavigationUtilFactory.getNavigationUtilApi().getLocationWrapper(var7)
               : ASLNavigationUtilFactory.getNavigationUtilApi().getLocationWrapper();
            NavAddress var9 = createNavAddress(var8);
            int[] var10 = this.calculateEtaForFinalDest(var3, var4, var5);
            int var11 = var10[2];
            int var12 = var10[0];
            int var13 = var10[1];
            long var14 = ASLNavigationUtilFactory.getNavigationUtilApi().getNaviHelper().calculateDistanceFromCCPToFinalDest(var4, var3, var5);
            var1[1] = new GuidanceInfo(var9, var11, var12, var13, var14);
            if (var3.indexOfCurrentDestination < var6.length) {
               var7 = var6[(int)var3.getIndexOfCurrentDestination()].getRouteLocation();
               ILocationWrapper var16 = var7 != null
                  ? ASLNavigationUtilFactory.getNavigationUtilApi().getLocationWrapper(var7)
                  : ASLNavigationUtilFactory.getNavigationUtilApi().getLocationWrapper();
               var9 = createNavAddress(var16);
               long var17 = var4.getTimeToNextDest();
               var10 = ASLNavigationUtilFactory.getNavigationUtilApi().getNaviHelper().calculateEta(var17);
               var11 = var10[2];
               var12 = var10[0];
               var13 = var10[1];
               var14 = var4.getDistanceToNextDest();
               var1[0] = new GuidanceInfo(var9, var11, var12, var13, var14);
            }
         }

         return var1;
      } else {
         return new GuidanceInfo[0];
      }
   }

   public NavAddress getCurrentCarPosition() {
      if (this.isServiceAvailable()) {
         INavGateway var1 = ASLNavigationUtilFactory.getNavigationUtilApi().getNavGateway();
         NavAddress var2 = createNavAddress(ASLNavigationUtilFactory.getNavigationUtilApi().getLocationWrapper(var1.getSoPosPositionDescription()));
         var2.setLatitude(ASLNavigationUtilFactory.getNavigationUtilApi().getNaviHelper().dsiToDecimal(var1.getSoPosPosition().getLatitude()));
         var2.setLongitude(ASLNavigationUtilFactory.getNavigationUtilApi().getNaviHelper().dsiToDecimal(var1.getSoPosPosition().getLongitude()));
         return var2;
      } else {
         return null;
      }
   }

   public NavAddress getLocationInputAddress() {
      NavLocation var1 = ASLNavigationUtilFactory.getNavigationUtilApi().getNavigationDp().getExternalLocationInputLocation();
      if (var1 == null) {
         this.target.warn("No location input address stored!");
      }

      return createNavAddress(ASLNavigationUtilFactory.getNavigationUtilApi().getLocationWrapper(var1));
   }

   public void importContact(AdbEntry var1, int var2) {
      this.importContact(var1, var2, true);
   }

   public void importContact(AdbEntry var1, int var2, boolean var3) {
      if (this.isServiceAvailable()) {
         this.target.importContact(var1, this.listener, var2, var3);
      }
   }

   public void importContactSummary(int var1, int var2, int var3, int var4) {
      this.target.notifyDestinationImportActive(false);
      if (this.isServiceAvailable()) {
         ASLNavigationMemoryFactory.getNavigationMemoryApi().importContactSummary(var1, var2, var3, var4);
      }
   }

   public boolean isGuidanceActive() {
      return this.isServiceAvailable() ? this.target.calculatedRgActive : false;
   }

   public boolean isServiceAvailable() {
      return this.target.isServiceAvailable();
   }

   public int startGuidance(NavAddress[] var1, boolean var2, int var3) {
      if (this.isServiceAvailable()) {
         this.target.trace("ASLNavigationServices.startGuidance() received.");
         if (this.target.isCurrentlyResolvingLocations()) {
            this.target.warn("ASLNavigationServices.startGuidance() blocked because route calculation or location resolving is in progress.");
            return 2;
         } else {
            this.target.startGuidance(var1, var2, var3);
            return 1;
         }
      } else {
         this.target.trace("ASLNavigationServices.startGuidance() received but navigation context is not available.");
         return 2;
      }
   }

   private int[] calculateEtaForFinalDest(Route var1, RgInfoForNextDestination var2, NavRouteListData[] var3) {
      long var4 = var2.getTimeToNextDest() * 0L;
      if (var3 != null && var3.length > 1 && var1 != null) {
         int var6 = (int)(var1.getIndexOfCurrentDestination() + 1L);
         if (var1.getRoutelist() != null && var6 < var3.length) {
            var4 += var3[var6].getRemainingTravelTime();
         }
      }

      return ASLNavigationUtilFactory.getNavigationUtilApi().getNaviHelper().calculateEta(var4);
   }

   public boolean isValidOnlinePoiCategory(int var1) {
      return ASLNavigationPoiFactory.getNavigationPoiApi().isValidOnlinePoiCategory(var1);
   }

   public int[] parseGeoPositionVCard3_0(String var1) {
      return this.target.parseGeoPosition(var1);
   }

   public int stopGuidance(int var1) {
      this.target.trace("ASLNavigationServices.stopGuidance(" + var1 + ") received.");
      new Exception("Stack trace").printStackTrace();
      if (var1 != 6) {
         this.target.stopGuidance(var1);
      }

      return 1;
   }
}
