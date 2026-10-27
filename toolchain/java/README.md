# Java build libraries

_OSGi API jars the Java patch compiles against._

---

`org.osgi.framework-1.10.0.jar` and `org.osgi.util.tracker-1.5.4.jar` are the OSGi Alliance API
jars from Maven Central (Apache License 2.0), unchanged. `scripts/java_env.sh` puts them on the
compile classpath next to the stock HMI jar from `java_stock/`.

`asm-9.7.jar`, `asm-tree-9.7.jar` and `asm-analysis-9.7.jar` are the OW2 ASM bytecode library
from Maven Central (BSD 3-Clause), unchanged; Java tests that inspect stock bytecode use them
(`ASM_CP` in `scripts/java_env.sh`).
