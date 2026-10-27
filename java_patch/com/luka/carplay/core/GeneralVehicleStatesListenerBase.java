/*
 * GENERATED from org.dsi.ifc.generalvehiclestates.DSIGeneralVehicleStatesListener (+ org.dsi.ifc.base.DSIListener) of the stock MU1329 lsd.jar:
 * every method empty, so a listener only overrides what it reads.
 *
 * Why not the stock adapters: on the car (2026-10-10) a subclass of
 * de.audi.remotehmi.util.dsi.DSICarKombiListenerAdapter failed with NoClassDefFoundError,
 * CarPlayApp's static init failed with it and took AppTerminalMode and AppNavi down
 * ("Initialising navigation" forever).  The same rule applies to the TerminalMode default listener.  Only org.dsi interfaces, which every
 * firmware loads, are referenced here.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
package com.luka.carplay.core;

public class GeneralVehicleStatesListenerBase implements org.dsi.ifc.generalvehiclestates.DSIGeneralVehicleStatesListener {
    public void updateAirbagData(org.dsi.ifc.generalvehiclestates.AirbagData a0, int a1) { }
    public void updateTankInfo(org.dsi.ifc.generalvehiclestates.TankInfo a0, int a1) { }
    public void updateDimmedHeadlight(boolean a0, int a1) { }
    public void updateAcousticParkingSystem(boolean a0, int a1) { }
    public void updateReverseGear(boolean a0, int a1) { }
    public void updateVehicleStandstill(boolean a0, int a1) { }
    public void updateCarVelocityThreshold(boolean a0, int a1) { }
    public void updateTVVelocityThreshold(boolean a0, int a1) { }
    public void updateHDDVelocityThreshold(boolean a0, int a1) { }
    public void updateBrowserSlideShowVelocityThreshold(boolean a0, int a1) { }
    public void updateBrowserBordBookVelocityThreshold(boolean a0, int a1) { }
    public void updateBrowserTravelAgentVelocityThreshold(boolean a0, int a1) { }
    public void updateBrowserWebVelocityThreshold(boolean a0, int a1) { }
    public void updateBWSVelocityThreshold(boolean a0, int a1) { }
    public void updateRadiotextVelocityThreshold(boolean a0, int a1) { }
    public void updateDisplayDayNightDesign(boolean a0, int a1) { }
    public void updateBTBondingVelocityThreshold(boolean a0, int a1) { }
    public void updateMessagingVelocityThreshold(boolean a0, int a1) { }
    public void updateDestinationInputVelocityThreshold(boolean a0, int a1) { }
    public void updateDSSSViewOption(org.dsi.ifc.global.CarViewOption a0, int a1) { }
    public void updateServiceKeyData(byte[] a0, int a1) { }
    public void updateServiceKeyViewOption(org.dsi.ifc.global.CarViewOption a0, int a1) { }
    public void updatePersonalizationStatus(boolean a0, int a1, int a2) { }
    public void updateTLOViewOptions(org.dsi.ifc.generalvehiclestates.TLOViewOptions a0, int a1) { }
    public void updateEmergencyAssistVolLowering(int a0, int a1) { }
    public void updateParkingBrake(boolean a0, int a1) { }
    public void updateAppConnectTrigger(int a0, int a1) { }
    public void updateSTPState(int a0, int a1) { }
    public void updateAutomaticGearShiftTransMode(int a0, int a1) { }
    public void asyncException(int a0, java.lang.String a1, int a2) { }
}
