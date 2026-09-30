#!/usr/bin/env python3
"""Generate minimal Java stubs to allow compiling CarPlay hook classes without MU1316-final.jar."""
import os
from pathlib import Path

STUBS = {
    'de/audi/app/terminalmode/IContext.java': 'package de.audi.app.terminalmode;\npublic interface IContext {}\n',
    'de/audi/atip/base/IFrameworkAccess.java': 'package de.audi.atip.base;\nimport de.audi.atip.hmi.HMIService;\npublic interface IFrameworkAccess {\n    long getUTCTime();\n    long convertUTCTimeToLocalTime(long utcMs);\n    int getKombiType();\n    HMIService getHMIService();\n}\n',
    'de/audi/atip/hmi/HMIService.java': 'package de.audi.atip.hmi;\nimport de.audi.atip.hmi.view.IDisplayManager;\npublic interface HMIService {\n    IDisplayManager getDisplayManager();\n}\n',
    'de/audi/atip/hmi/intercommunication/NaviMoKoKDKConstants.java': 'package de.audi.atip.hmi.intercommunication;\npublic interface NaviMoKoKDKConstants {}\n',
    'de/audi/atip/hmi/view/IDisplayManager.java': 'package de.audi.atip.hmi.view;\npublic interface IDisplayManager {\n    int getCurrentContextID(int terminal);\n    void switchContext(int ctx, int terminal, Object opt);\n    void setUpdateRate(int terminal, int rate);\n}\n',
    'de/audi/atip/interapp/combi/bap/navi/CombiBAPServiceNavi.java': 'package de.audi.atip.interapp.combi.bap.navi;\nimport de.audi.atip.interapp.combi.bap.navi.data.*;\npublic interface CombiBAPServiceNavi {\n    void updateActiveRGType(int t);\n    void updateCurrentPositionInfo(String s);\n    void updateTurnToInfo(String a, String b);\n    void updateDestinationInfo(CombiBAPDestinationInfo d);\n    void updateDistanceToDestination(int val, int unit, boolean stop);\n    void updateDistanceToNextManeuver(int val, int unit, boolean on, int bar);\n    void updateExitView(int v, int num);\n    void updateLaneGuidance(boolean on, CombiBAPNaviLaneGuidanceData[] data);\n    void updateManeuverDescriptor(CombiBAPNaviManeuverDescriptor[] desc);\n    void updateManeuverState(int s);\n    void updateRGStatus(int s);\n    void updateTimeToDestination(int mode, int fmt, long val);\n}\n',
    'de/audi/atip/interapp/combi/bap/navi/data/CombiBAPDestinationInfo.java': 'package de.audi.atip.interapp.combi.bap.navi.data;\npublic class CombiBAPDestinationInfo {\n    public CombiBAPDestinationInfo(CombiBAPNaviDestination d) {}\n}\n',
    'de/audi/atip/interapp/combi/bap/navi/data/CombiBAPNaviDestination.java': 'package de.audi.atip.interapp.combi.bap.navi.data;\npublic class CombiBAPNaviDestination {\n    public CombiBAPNaviDestination(String a, String b, String c, String d, String e, String f, String g) {}\n}\n',
    'de/audi/atip/interapp/combi/bap/navi/data/CombiBAPNaviLaneGuidanceData.java': 'package de.audi.atip.interapp.combi.bap.navi.data;\npublic class CombiBAPNaviLaneGuidanceData {\n    public CombiBAPNaviLaneGuidanceData(int pos, int dir, int status, int angle) {}\n    public CombiBAPNaviLaneGuidanceData(short pos, short dir, byte[] desc, short status, byte a, byte b, byte c, byte d) {}\n}\n',
    'de/audi/atip/interapp/combi/bap/navi/data/CombiBAPNaviManeuverDescriptor.java': 'package de.audi.atip.interapp.combi.bap.navi.data;\npublic class CombiBAPNaviManeuverDescriptor {\n    public CombiBAPNaviManeuverDescriptor(int main, int dir, int z, byte[] side) {}\n}\n',
    'de/audi/atip/log/LogChannel.java': 'package de.audi.atip.log;\npublic class LogChannel {\n    public void log(int level, String msg) {}\n}\n',
    'de/audi/atip/metrics/DateMetric.java': 'package de.audi.atip.metrics;\npublic class DateMetric {\n    public static int timeFormat = 0;\n}\n',
    'de/audi/atip/metrics/Distance.java': 'package de.audi.atip.metrics;\npublic class Distance {\n    public static final int KM = 1, METERS = 0, NONE = 2, MILES = 3;\n    private static int unit = KM;\n    public static int getSystemUnit() { return unit; }\n    public static void setSystemUnit(int u) { unit = u; }\n}\n',
    'de/audi/atip/power/PowerEventListener.java': 'package de.audi.atip.power;\npublic interface PowerEventListener {}\n',
    'de/audi/tghu/fwhmi/IDisplayManagerKombiControl.java': 'package de.audi.tghu.fwhmi;\npublic interface IDisplayManagerKombiControl {\n    void setOpacity(int a, int b, int c);\n    void setPosition(int a, int b, int c, int d);\n    void setCropping(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j);\n}\n',
    'de/audi/tghu/navi/app/Navigation.java': 'package de.audi.tghu.navi.app;\nimport de.audi.tghu.navi.app.cluster.ClusterService;\npublic class Navigation {\n    public static Navigation getInstance() { return new Navigation(); }\n    public ClusterService getClusterService() { return null; }\n    public de.audi.tghu.navi.app.routeguidance.IRouteManager getRouteManager() { return null; }\n}\n',
    'de/audi/tghu/navi/app/cluster/BAPDistanceFormatter.java': 'package de.audi.tghu.navi.app.cluster;\nimport de.audi.atip.log.LogChannel;\npublic class BAPDistanceFormatter {\n    public static class BAPDistance {\n        public int getValue() { return 0; }\n        public int getUnit() { return 0; }\n    }\n    public BAPDistanceFormatter(LogChannel ch) {}\n    public BAPDistance formatDistanceToTurn(int m, boolean metric) { return new BAPDistance(); }\n    public BAPDistance formatDistanceToDestination(int m, boolean metric) { return new BAPDistance(); }\n}\n',
    'de/audi/tghu/navi/app/cluster/ClusterService.java': 'package de.audi.tghu.navi.app.cluster;\nimport de.audi.tghu.navi.app.command.DSIResponseContainer;\npublic class ClusterService implements de.audi.atip.hmi.intercommunication.NaviMoKoKDKConstants, de.audi.atip.power.PowerEventListener {\n    public KOMOService getKomoService() { return null; }\n    public ClusterViewMode getClusterViewMode() { return null; }\n    public DSIResponseContainer getDSIResponseContainer() { return null; }\n    public void setRouteGuidanceAborted() {}\n    public void updateRGIString(short[] s) {}\n    public void triggerRefreshRGIValid() {}\n    public void setKOMODataRate(int r) {}\n}\n',
    'de/audi/tghu/navi/app/cluster/ClusterViewMode.java': 'package de.audi.tghu.navi.app.cluster;\npublic class ClusterViewMode {\n    public void setDataRate(int r) {}\n    public void setGFXAvailable(boolean b) {}\n}\n',
    'de/audi/tghu/navi/app/cluster/KOMOService.java': 'package de.audi.tghu.navi.app.cluster;\npublic interface KOMOService {\n    void setClusterViewMode(ClusterViewMode m);\n    void updateDataRate(int r, int x);\n    void updateGfxState(int s, int x);\n}\n',
    'de/audi/tghu/navi/app/command/DSIResponseContainer.java': 'package de.audi.tghu.navi.app.command;\npublic class DSIResponseContainer {\n    public Object getDSICarplayListener() { return null; }\n    public boolean isRgActive() { return false; }\n    public void setRgActive(boolean b) {}\n}\n',
    'de/audi/tghu/navi/app/routeguidance/IRouteManager.java': 'package de.audi.tghu.navi.app.routeguidance;\npublic interface IRouteManager {\n    org.dsi.ifc.navigation.Route getRoute();\n    void stopRouteGuidance();\n}\n',
    'de/audi/tghu/navi/app/util/Util.java': 'package de.audi.tghu.navi.app.util;\nimport de.audi.atip.base.IFrameworkAccess;\npublic class Util {\n    public static boolean isClusterMapMOST(IFrameworkAccess fw) { return true; }\n}\n',
    'de/esolutions/hmi/widgets/audi/base/Layout.java': 'package de.esolutions.hmi.widgets.audi.base;\npublic class Layout {\n    public static class Rect {\n        public int x, y, width, height;\n    }\n    public Rect getLayer(int l) { return null; }\n    public int getIntegerConstant(int id) { return 0; }\n}\n',
    'org/dsi/ifc/base/DSIListener.java': 'package org.dsi.ifc.base;\npublic interface DSIListener {}\n',
    'org/dsi/ifc/keypanel/DSIKeyPanel.java': 'package org.dsi.ifc.keypanel;\npublic interface DSIKeyPanel {}\n',
    'org/dsi/ifc/keypanel/DSIKeyPanelListener.java': 'package org.dsi.ifc.keypanel;\nimport org.dsi.ifc.base.DSIListener;\npublic interface DSIKeyPanelListener extends DSIListener {}\n',
    'org/dsi/ifc/navigation/Route.java': 'package org.dsi.ifc.navigation;\npublic class Route {}\n',
}

def main():
    base = Path("build_stubs")
    for rel_path, code in STUBS.items():
        target = base / rel_path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(code)
    print(f"Generated {len(STUBS)} stubs in {base}/")

if __name__ == "__main__":
    main()
