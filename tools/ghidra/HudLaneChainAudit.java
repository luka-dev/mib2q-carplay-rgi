// Execute the original HUD V850 decoded lane-cache -> layout -> display packet.
// Args: inputs.csv outputs.csv [FD51 override]. All writes are emulator-only.
import ghidra.app.script.GhidraScript;
import ghidra.app.emulator.EmulatorHelper;
import java.nio.file.*;
import java.util.*;
import java.io.*;
import java.security.MessageDigest;
public class HudLaneChainAudit extends GhidraScript {
 private EmulatorHelper emu;
 private final long scratch=0x1002000L,stop=0x1004000L,stack=0x1020000L;
 private long steps;
 private void require(boolean v,String m)throws Exception{if(!v)throw new Exception(m);}
 private void put(long a,long v,int n)throws Exception{byte[] b=new byte[n];for(int i=0;i<n;i++)b[i]=(byte)(v>>>(i*8));emu.writeMemory(toAddr(a),b);}
 private long get(long a,int n)throws Exception{byte[] b=emu.readMemory(toAddr(a),n);long v=0;for(int i=0;i<n;i++)v|=(long)(b[i]&255)<<(i*8);return v;}
 private String hex(byte[] b){StringBuilder s=new StringBuilder();for(byte x:b)s.append(String.format("%02x",x&255));return s.toString();}
 private byte[] unhex(String s){byte[] b=new byte[s.length()/2];for(int i=0;i<b.length;i++)b[i]=(byte)Integer.parseInt(s.substring(i*2,i*2+2),16);return b;}
 private void call(long addr,long...args)throws Exception {
  for(int i=0;i<args.length;i++)emu.writeRegister("r"+(i+6),args[i]);
  emu.writeRegister("sp",stack);emu.writeRegister("lp",stop);emu.writeRegister("pc",addr);
  int n=0;
  while((emu.readRegister("pc").longValue()&0xffffffffL)!=stop && n++<100000){
   require(emu.step(monitor),"Execution failed at "+emu.getExecutionAddress()+": "+emu.getLastError());
  }
  require(n<100000,"Instruction cap at "+Long.toHexString(addr));steps+=n;
 }
 public void run()throws Exception {
  String[] args=getScriptArgs();require(args.length==2||args.length==3,"inputs outputs [override]");
  require(currentProgram.getMinAddress().getOffset()==0x20000L,"ROM block must start at 0x20000");
  byte[] rom=new byte[638976];currentProgram.getMemory().getBytes(toAddr(0x20000L),rom);
  if(args.length==3)rom=Files.readAllBytes(Paths.get(args[2]));
  String sha=hex(MessageDigest.getInstance("SHA-256").digest(rom));
  require(sha.equals("138351c2f12cd8e45d9f50fd4833775371ec46401b6838f04c24a9a4baaabae2")||sha.equals("6e59b62c982873a778bf72b749bf6dc113fe3fe14ee4bb3ae3855d0c624f6fe5"),"Unknown firmware");
  emu=new EmulatorHelper(currentProgram);emu.writeMemory(toAddr(0x20000L),rom);
  List<String> lines=Files.readAllLines(Paths.get(args[0]));int cases=0;
  try(PrintWriter out=new PrintWriter(args[1],"UTF-8")) {
   out.println("case,name,cache_hex,model_hex,packet_hex,group_mask");
   for(String line:lines.subList(1,lines.size())) {
    monitor.checkCancelled();String[] f=line.split(",",-1);byte[] cache=unhex(f[2]);require(cache.length==226,"Decoded lane-cache size");
    emu.writeMemory(toAddr(0xfedf0000L),new byte[65536]);emu.writeMemory(toAddr(scratch),new byte[0x1000]);emu.writeMemory(toAddr(stack-0x4000L),new byte[0x4000]);
    for(int reg=1;reg<32;reg++)if(currentProgram.getRegister("r"+reg)!=null)emu.writeRegister("r"+reg,0);
    emu.writeRegister("PSW",0);emu.writeRegister("ep",0);
    emu.writeRegister("gp",0xfedff8b4L);emu.writeRegister("tp",0x28018L);emu.writeRegister("CTBP",0xb59c0L);
    emu.writeMemory(toAddr(0xfedfaa02L),cache);
    put(scratch+3,128,1);
    call(0x8ad74L); // reset lane geometry and restore five-cell capacity
    call(0x8b21cL,scratch); // snapshot import, trim, symbols, dividers
    require(get(0xfedfc42cL,1)==5,"Five-cell configuration changed");
    call(0x9767aL,39); // lane model transfer, original code
    put(0xfedfc180L,39,4); // isolate the active lane pane
    call(0x8731cL,scratch+0x100,scratch+0x200,1<<10);
    long mask=get(scratch+0x200,4);require(mask==(1<<10),"Lane group mask changed");
    out.println(line+","+hex(emu.readMemory(toAddr(0xfedfc708L),42))+","+hex(emu.readMemory(toAddr(scratch+0x100),35))+","+mask);cases++;
   }
  }finally{emu.dispose();}
  println("HUD_LANE_CHAIN_PASS cases="+cases+" original_steps="+steps+" substitutions=0 sha256="+sha);
 }
}
