package com.luka.carplay.rgd;

import com.luka.carplay.framework.Log;
import de.audi.atip.interapp.combi.bap.navi.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.navi.data.*;
import java.lang.reflect.*;
import java.util.*;

/** Actual gate against a failing BAP endpoint; no timing or stock framework mocks. */
public final class NativeBapReplayTest {
    static int checks;
    static void check(boolean ok, String why) { checks++; if (!ok) throw new AssertionError(why); }
    static final String[] SYNC = {"updateRGStatus", "updateActiveRGType", "updateManeuverDescriptor",
        "updateDistanceToNextManeuver", "updateExitView"};
    static final String[] OWNED = {"updateRGStatus", "updateActiveRGType", "updateManeuverDescriptor",
        "updateDistanceToNextManeuver", "updateExitView", "updateLaneGuidance", "updateManeuverState",
        "updateCurrentPositionInfo", "updateTurnToInfo", "updateDistanceToDestination",
        "updateTimeToDestination", "updateDestinationInfo", "updateMapScale", "updateAltitude"};
    static class Sink implements InvocationHandler {
        Map<String,Object[]> values = new HashMap<String,Object[]>();
        List<String> calls = new ArrayList<String>();
        String failing; int failures;
        public Object invoke(Object p, Method m, Object[] a) {
            calls.add(m.getName());
            if (m.getName().equals(failing) && failures-- > 0) throw new IllegalStateException("injected");
            values.put(m.getName(), a); return null;
        }
        CombiBAPServiceNavi endpoint() {
            return (CombiBAPServiceNavi) Proxy.newProxyInstance(getClass().getClassLoader(),
                new Class[]{CombiBAPServiceNavi.class}, this);
        }
    }
    static CombiBAPDestinationInfo destination() {
        return new CombiBAPDestinationInfo(3, 1, new CombiBAPNaviDestination("Street", "7", "Town", "", "State", "Zip", "Country"));
    }
    static void seed(GatedCombiService g) {
        g.updateRGStatus(0); g.updateActiveRGType(4);
        g.updateManeuverDescriptor(new CombiBAPNaviManeuverDescriptor[0]);
        g.updateDistanceToNextManeuver(-1,255,false,0); g.updateExitView(0,0);
        g.updateLaneGuidance(false,new CombiBAPNaviLaneGuidanceData[0]); g.updateManeuverState(0);
        g.updateCurrentPositionInfo("native"); g.updateTurnToInfo("", "");
        g.updateDistanceToDestination(-1,255,false); g.updateTimeToDestination(15,0,-1);
        g.updateDestinationInfo(destination()); g.updateMapScale(1,false,250,0,true); g.updateAltitude(120,0);
    }
    static void splitOwnership() {
        Sink s = new Sink(); GatedCombiService g = new GatedCombiService(s.endpoint()); seed(g);
        s.calls.clear(); g.setRouteGuidanceBlocked(true); g.setCurrentPositionInfoBlocked(true);
        check(s.calls.isEmpty(),"acquisition did endpoint I/O");
        g.replayPending(); check(s.calls.equals(Arrays.asList("updateMapScale","updateAltitude")),"hide policy");
        s.calls.clear(); seed(g); // MapScale must ACK each SetGet even while gated.
        check(s.calls.equals(Arrays.asList("updateMapScale")),"blocked data escaped / SetGet ACK missing");
        g.updateCurrentPositionInfo("latest"); g.updateTurnToInfo("cleared next", "");
        g.updateTurnToInfo("", ""); s.calls.clear();
        g.setCurrentPositionInfoBlocked(false); check(s.calls.isEmpty(),"release did endpoint I/O");
        g.replayPending();
        check(s.calls.size()==5 && !s.calls.contains("updateRGStatus"),"route end released session fields");
        check("latest".equals(s.values.get("updateCurrentPositionInfo")[0]),"latest street missing");
        check("".equals(s.values.get("updateTurnToInfo")[0]),"native clear lost");
        check(!g.hasPendingReplay(),"blocked route data kept recovery spinning");
        s.calls.clear(); g.setRouteGuidanceBlocked(false); g.replayPending();
        check(s.calls.subList(0,5).equals(Arrays.asList(SYNC)),"sync ordering");
        check(s.calls.size()==9,"disconnect restored wrong group");
        check(((Integer)s.values.get("updateMapScale")[2])==250,"scale not restored");
        check(((Integer)s.values.get("updateAltitude")[0])==120,"altitude not restored");
        s.calls.clear(); g.replayPending(); check(s.calls.isEmpty(),"successful replay duplicated");
        g.requestReplay(false); g.replayPending(); check(s.calls.size()==5,"repeated cleanup not repaired");
    }
    static void failureAndSupersession() {
        for (String name : OWNED) {
            Sink s = new Sink(); GatedCombiService g = new GatedCombiService(s.endpoint()); seed(g);
            g.setRouteGuidanceBlocked(true); g.setCurrentPositionInfoBlocked(true); g.replayPending();
            g.setRouteGuidanceBlocked(false); g.setCurrentPositionInfoBlocked(false);
            s.failing=name; s.failures=1; s.calls.clear(); g.replayPending();
            check(g.hasPendingReplay(),"failed restore forgotten: "+name);
            s.calls.clear(); g.replayPending(); check(!g.hasPendingReplay(),"retry did not converge: "+name);
            if (Arrays.asList(SYNC).contains(name)) check(s.calls.equals(Arrays.asList(SYNC)),"partial sync retry: "+name);
            else check(s.calls.equals(Arrays.asList(name)),"replayed unrelated fields: "+name);
        }
        Sink s = new Sink(); GatedCombiService g = new GatedCombiService(s.endpoint());
        g.setCurrentPositionInfoBlocked(true); g.updateCurrentPositionInfo("old");
        g.setCurrentPositionInfoBlocked(false); s.failing="updateCurrentPositionInfo"; s.failures=1;
        g.replayPending(); g.updateCurrentPositionInfo("new"); s.calls.clear(); g.replayPending();
        check(s.calls.isEmpty() && "new".equals(s.values.get("updateCurrentPositionInfo")[0]),"stale failed replay overwrote new native data");
        g.requestReplay(false); g.setCurrentPositionInfoBlocked(true); s.calls.clear(); g.replayPending();
        check(s.calls.isEmpty() && !g.hasPendingReplay(),"reacquired ownership leaked replay");
        g.retire(); g.setCurrentPositionInfoBlocked(false); seed(g); g.replayPending();
        check(s.calls.isEmpty() && !g.hasPendingReplay(),"retired endpoint received owned writes");
    }
    static void mutableInputs() {
        Sink s = new Sink(); GatedCombiService g = new GatedCombiService(s.endpoint());
        g.setRouteGuidanceBlocked(true); g.setCurrentPositionInfoBlocked(true);
        CombiBAPNaviManeuverDescriptor m = new CombiBAPNaviManeuverDescriptor(1,2,3,new byte[]{4});
        CombiBAPNaviLaneGuidanceData l = new CombiBAPNaviLaneGuidanceData((short)1,(short)2,new byte[]{3},(short)4,(byte)5,(byte)6,(byte)7,(byte)8);
        CombiBAPDestinationInfo d = destination();
        g.updateManeuverDescriptor(new CombiBAPNaviManeuverDescriptor[]{m,null});
        g.updateLaneGuidance(true,new CombiBAPNaviLaneGuidanceData[]{l,null}); g.updateDestinationInfo(d);
        m.direction=90; m.sideStreets[0]=99; l.laneDirection=90; l.laneSideStreets[0]=99;
        d.setStopoverInformation(9,9); d.getDestination().setAddress("changed","","","","","","",255);
        g.setRouteGuidanceBlocked(false); g.setCurrentPositionInfoBlocked(false); g.replayPending();
        CombiBAPNaviManeuverDescriptor[] ms=(CombiBAPNaviManeuverDescriptor[])s.values.get("updateManeuverDescriptor")[0];
        CombiBAPNaviLaneGuidanceData[] ls=(CombiBAPNaviLaneGuidanceData[])s.values.get("updateLaneGuidance")[1];
        CombiBAPDestinationInfo ds=(CombiBAPDestinationInfo)s.values.get("updateDestinationInfo")[0];
        check(ms[0].direction==2 && ms[0].sideStreets[0]==4 && ms[1]==null,"mutable maneuver snapshot");
        check(ls[0].laneDirection==2 && ls[0].laneSideStreets[0]==3 && ls[1]==null,"mutable lane snapshot");
        check(ds.getNoOfStopovers()==3 && ds.getDestination().getStreet().equals("Street"),"mutable destination snapshot");
        ms[0].direction=80; ls[0].laneSideStreets[0]=80; ds.setStopoverInformation(8,8);
        g.requestReplay(true); g.requestReplay(false); g.replayPending();
        check(((CombiBAPNaviManeuverDescriptor[])s.values.get("updateManeuverDescriptor")[0])[0].direction==2,"endpoint mutated saved maneuver");
        check(((CombiBAPNaviLaneGuidanceData[])s.values.get("updateLaneGuidance")[1])[0].laneSideStreets[0]==3,"endpoint mutated saved lane");
        check(((CombiBAPDestinationInfo)s.values.get("updateDestinationInfo")[0]).getNoOfStopovers()==3,"endpoint mutated saved destination");
    }
    public static void main(String[] args) {
        Log.setLevel(-1); splitOwnership(); failureAndSupersession(); mutableInputs();
        System.out.println("NativeBapReplayTest: PASS ("+checks+" checks)");
    }
}
