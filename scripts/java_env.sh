# Java toolchain for the build and the Java tests; sourced, never run.
#   JAVA8_HOME  a JDK 8 (the patch compiles with -source/-target 1.4, which JDK 9+ dropped).
#               Default: the Zulu 8 bundled in a jxe2jar checkout, then macOS java_home -v 1.8.
#   JXE2JAR     a https://github.com/luka-dev/jxe2jar checkout with its out/ jars; only the
#               stock-class audits and the tests that run real stock HMI code need it.
_JE_REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
JXE2JAR=${JXE2JAR:-$_JE_REPO/../../Tools/jxe2jar}
if [ -z "${JAVA8_HOME:-}" ]; then
    for _je in "$JXE2JAR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home" \
               "$JXE2JAR/jvms/zulu8.78.0.19-ca-jdk8.0.412-linux_x64" \
               "$(/usr/libexec/java_home -v 1.8 2>/dev/null || true)"; do
        # java_home -v 1.8 answers with the newest JDK when no 8 is installed: check the version
        [ -n "$_je" ] && [ -x "$_je/bin/javac" ] && "$_je/bin/javac" -version 2>&1 | grep -q ' 1\.8\.' \
            && { JAVA8_HOME=$_je; break; }
    done
fi
[ -n "${JAVA8_HOME:-}" ] || { echo "ERROR: no JDK 8 found; set JAVA8_HOME" >&2; exit 1; }
OSGI_CP="$_JE_REPO/toolchain/java/org.osgi.framework-1.10.0.jar:$_JE_REPO/toolchain/java/org.osgi.util.tracker-1.5.4.jar"
ASM_CP="$_JE_REPO/toolchain/java/asm-9.7.jar:$_JE_REPO/toolchain/java/asm-tree-9.7.jar:$_JE_REPO/toolchain/java/asm-analysis-9.7.jar"
# A test that runs real stock HMI code skips cleanly without a jxe2jar checkout.
java_need_jxe2jar() {
    [ -d "$JXE2JAR/out" ] || { echo "SKIP ($1: needs a jxe2jar checkout with out/ jars; set JXE2JAR)"; exit 0; }
}
