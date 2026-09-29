//! QEMU-only QNX client for the real stock dio_manager receiver bridge.
use mhi2q_wireless_daemon::receiver_control::{LoopbackReceiverControl, ReceiverControl};
use std::thread;
use std::time::{Duration, Instant};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut receiver = LoopbackReceiverControl::new(5000)?;
    let deadline = Instant::now() + Duration::from_secs(55);
    let mut attempts = 0;
    loop {
        attempts += 1;
        match receiver.start(1) {
            Ok(()) => break,
            Err(error) if Instant::now() < deadline => {
                eprintln!("[receiver-control-qemu] START retry {attempts}: {error}");
                thread::sleep(Duration::from_millis(500));
            }
            Err(error) => return Err(format!("START 1 failed: {error}").into()),
        }
    }
    println!("[receiver-control-qemu] START 1=0 attempts={attempts}");
    receiver.stop(1)?;
    println!("[receiver-control-qemu] STOP 1=0");
    receiver.start(2)?;
    println!("[receiver-control-qemu] START 2=0");
    receiver.stop(2)?;
    println!("[receiver-control-qemu] STOP 2=0");
    Ok(())
}
