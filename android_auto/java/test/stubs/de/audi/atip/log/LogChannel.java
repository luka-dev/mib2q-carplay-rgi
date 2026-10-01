package de.audi.atip.log;

/** Host-test stub (see test/stubs/README.txt). */
public abstract class LogChannel {
    public String name;
    protected int loglevel;
    public LogChannel() { }
    public abstract void log(int level, String pattern, Object a, Object b, Object c, Object d,
                             long l1, long l2, long l3, int flags, Throwable t);
    public abstract void log(int level, int messageId, Object a, Object b, Object c, Object d,
                             long l1, long l2, long l3, int flags, Throwable t);
    public boolean log(int level, String s) { return true; }
    public boolean log(int level, String s, long v) { return true; }
    public boolean log(int level, String s, Object o) { return true; }
}
