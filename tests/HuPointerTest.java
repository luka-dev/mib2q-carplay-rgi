package com.luka.carplay.cursor;

import java.io.File;
import java.io.FileOutputStream;

public final class HuPointerTest {
    private static int checks;
    private static void check(boolean value, String label) {
        checks++;
        if (!value) throw new AssertionError(label);
    }
    private static PointerState state(int width, int height, int pad) {
        PointerState s = new PointerState(); s.configure(width, height, pad); return s;
    }
    private static final class Sink implements PointerClick.Sink {
        String events = "";
        public void touch(int n, int x, int y) { events += n + "@" + x + "," + y + ";"; }
    }

    public static void main(String[] args) throws Exception {
        PointerState p = state(1024, 480, 1024);
        check(p.input() == CursorWire.DISMISS, "initial input has no activity");
        p.touch(900,700);
        check(p.x()==512 && p.y()==240, "first contact anchors without teleporting");
        int activity=p.activity(); p.touch(900,700);
        check(p.activity()==activity, "stationary input is not repeated activity");
        p.touch(920,710);
        check(p.x()==532 && p.y()==250, "relative movement");
        activity=p.activity(); p.end();
        check(p.input()==CursorWire.RELEASED && p.activity()==activity, "release preserves activity for coalesced short gestures");
        p.touch(10,10); p.forceHide(); activity=p.activity(); p.touch(120,200);
        check(p.input()==CursorWire.FORCE_HIDE && p.activity()==activity && p.x()==532, "rotary suppresses held contact");
        p.hide(); check(p.input()==CursorWire.FORCE_HIDE,"soft input cannot downgrade an unconsumed rotary hide");
        p.end(); p.touch(120,200);
        check(p.input()==CursorWire.TOUCH && p.activity()!=activity, "fresh contact after rotary");
        p.touch(-20000,-20000); check(p.x()==0 && p.y()==0,"top/left clamp");
        p.touch(20000,20000); check(p.x()==1023 && p.y()==479,"bottom/right clamp");
        p.reset(); check(p.input()==CursorWire.DISMISS,"session reset dismisses");
        PointerState smallPad=state(1024,480,256);
        smallPad.touch(0,0); smallPad.touch(25,25);
        check(smallPad.x()==612 && smallPad.y()==340,"equal gain on both axes");

        Sink a = new Sink(), b = new Sink();
        PointerClick click = new PointerClick();
        check(click.press(true, 45, 67, a), "visible pointer selects synthetic route");
        check(click.ownsPress(), "LONGPRESSED stays on the synthetic route");
        check(click.press(false, 999, 999, b), "duplicate press remains consumed");
        check(click.release(), "release is consumed after cursor has hidden");
        check(a.events.equals("1@45,67;0@45,67;") && b.events.length() == 0,
            "release uses original coordinates and sink");
        check(!click.release(), "duplicate release produces no synthetic event");
        check(!click.press(false, 0, 0, a), "hidden pointer selects stock route");
        check(!click.ownsPress(), "stock LONGPRESSED is preserved");
        check(!click.press(true, 3, 4, a) && !click.release(), "stock press stays stock even if cursor appears");
        a.events = "";
        click.press(true, 10, 20, a); click.cancel(); click.cancel();
        check(click.ownsPress(), "cancelled touch also consumes late LONGPRESSED");
        check(a.events.equals("1@10,20;0@10,20;"), "rotary or real touchscreen cancels touch exactly once");
        check(click.release() && a.events.equals("1@10,20;0@10,20;"), "cancelled physical release stays consumed");
        a.events = "";
        click.press(true, 30, 40, a); click.abandon();
        check(click.release() && a.events.equals("1@30,40;"), "disconnect never sends to a rebound DSI session");

        byte[] wire = CursorWire.encode(CursorWire.SHOW, 0x12345678, 0x76543210,
            1024, 480, 100, 20, 800, 440, 399, 219, CursorWire.TOUCH, 7, 9);
        check(wire.length == 40 && wire[0] == 2 && wire[1] == 2, "fixed protocol version/shape");
        check((wire[3] & 255) == 103, "owned displayable ID");
        check(CursorWire.get32(wire, 4) == 0x12345678 && CursorWire.get32(wire, 8) == 0x76543210,
            "sequence and lease token are big endian");
        check((wire[12] & 255) == 4 && wire[13] == 0 && (wire[28] & 255) == CursorWire.TOUCH && CursorWire.get32(wire,32)==7 && CursorWire.get32(wire,36)==9,
            "dimensions and input ordering serialization");
        if (args.length > 0) {
            File dir = new File(args[0]); dir.mkdirs();
            FileOutputStream out = new FileOutputStream(new File(dir, "cursor-wire.bin"));
            out.write(wire); out.close();
        }
        System.out.println("HuPointerTest: " + checks + " checks passed");
    }
}
