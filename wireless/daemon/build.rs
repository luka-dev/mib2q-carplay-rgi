fn main() {
    if std::env::var_os("CARGO_FEATURE_SOFTWARE_MFI_TEST").is_some()
        && (std::env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("nto")
            || std::env::var("CARGO_CFG_TARGET_FAMILY").as_deref() != Ok("unix"))
    {
        panic!("software-mfi-test is an offline host-only synthetic identity provider");
    }
    if std::env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("nto") {
        let output = std::path::PathBuf::from(std::env::var_os("OUT_DIR").expect("OUT_DIR"));
        let mut sources = vec!["src/qnx_i2c.c", "src/qnx_process.c"];
        if std::env::var_os("CARGO_FEATURE_QEMU_PROBE").is_some() {
            sources.push("probes/software_mfi_verify_qemu.c");
        }
        let archive = output.join("libmhi2q_qnx_native.a");
        let compiler =
            std::env::var("CC").unwrap_or_else(|_| "arm-unknown-nto-qnx6.5.0eabi-gcc".into());
        let archiver =
            std::env::var("AR").unwrap_or_else(|_| "arm-unknown-nto-qnx6.5.0eabi-ar".into());
        let mut objects = Vec::new();
        for source in sources {
            let object = output.join(format!("{}.o", source.replace(['/', '.'], "_")));
            let mut command = std::process::Command::new(&compiler);
            command
                .args(["-std=c11", "-D_DEFAULT_SOURCE", "-fPIC", "-c", source, "-o"])
                .arg(&object);
            if source == "probes/software_mfi_verify_qemu.c" {
                command.args(["-Wall", "-Wextra", "-Werror"]);
            }
            let compile = command.status().expect("run QNX C compiler");
            assert!(compile.success(), "QNX C shim compilation failed: {source}");
            objects.push(object);
            println!("cargo:rerun-if-changed={source}");
        }
        let archive_status = std::process::Command::new(archiver)
            .arg("crs")
            .arg(&archive)
            .args(&objects)
            .status()
            .expect("run QNX archiver");
        assert!(archive_status.success(), "QNX native archive failed");
        println!("cargo:rustc-link-search=native={}", output.display());
        println!("cargo:rustc-link-lib=static=mhi2q_qnx_native");
    }
}
