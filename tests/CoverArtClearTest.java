import com.luka.carplay.coverart.CoverArt;
import com.luka.carplay.bus.CarplayBus;
import java.lang.reflect.Field;

public class CoverArtClearTest {
    private static int calls;
    private static void require(boolean okay) { if (!okay) throw new AssertionError(); }
    private static void event(CoverArt art,String text) throws Exception {
        byte[] bytes=text.getBytes("UTF-8");art.onFrame(CarplayBus.EVT_COVERART,0,bytes,bytes.length);
    }
    public static void main(String[] args) throws Exception {
        CoverArt art=CoverArt.getInstance();
        Field running=CoverArt.class.getDeclaredField("running");running.setAccessible(true);running.setBoolean(art,true);
        Field callback=CoverArt.class.getDeclaredField("callback");callback.setAccessible(true);
        callback.set(art,new CoverArt.Callback() {
            public void onNewCoverArt(String path,int id) {
                calls++;require(id==art.getArtId()&&path.equals(art.getPath()));
            }
        });
        event(art,"crc:n:123\npath:s:"+CoverArt.COVERART_PATH+"\n");
        require(calls==1&&art.hasCoverArt());
        event(art,"crc:n:0\npath:s:\n");require(calls==1&&art.hasCoverArt());
        event(art,"crc:n:0\nclear:n:1\npath:s:\n");
        require(calls==2&&!art.hasCoverArt()&&art.getArtId()==0&&art.getPath().length()==0);
        event(art,"crc:n:0\nclear:n:1\npath:s:\n");require(calls==2);
        event(art,"crc:n:123\npath:s:"+CoverArt.COVERART_PATH+"\n");require(calls==3&&art.hasCoverArt());
        System.out.println("Cover art explicit empty update, CRC reset, duplicate and coherent callback PASS");
    }
}
