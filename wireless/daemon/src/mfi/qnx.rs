use super::RegisterBus;
use std::ffi::CString;

unsafe extern "C" {
    fn mhi2q_i2c_open(path: *const std::ffi::c_char) -> i32;
    fn mhi2q_i2c_close(fd: i32) -> i32;
    fn mhi2q_i2c_lock(fd: i32) -> i32;
    fn mhi2q_i2c_unlock(fd: i32) -> i32;
    fn mhi2q_i2c_read(fd: i32, address: u32, register: u8, data: *mut u8, length: u32) -> i32;
    fn mhi2q_i2c_write(fd: i32, address: u32, register: u8, data: *const u8, length: u32) -> i32;
}

pub struct QnxBus {
    path: CString,
    address: u32,
    fd: Option<i32>,
}

impl Clone for QnxBus {
    fn clone(&self) -> Self {
        Self {
            path: self.path.clone(),
            address: self.address,
            fd: None,
        }
    }
}

impl QnxBus {
    pub fn from_env() -> Result<Self, String> {
        let path = std::env::var("MHI2Q_CP_MFI_PATH").unwrap_or_else(|_| "/dev/i2c8".into());
        let address = std::env::var("MHI2Q_CP_MFI_ADDR")
            .unwrap_or_else(|_| "17".into())
            .parse::<u32>()
            .map_err(|_| "invalid MHI2Q_CP_MFI_ADDR")?;
        if address > 0x7f {
            return Err("MFI I2C address must be 7-bit".into());
        }
        Ok(Self {
            path: CString::new(path).map_err(|_| "MFI path contains NUL")?,
            address,
            fd: None,
        })
    }

    fn fd(&mut self) -> Result<i32, String> {
        if let Some(fd) = self.fd {
            return Ok(fd);
        }
        // SAFETY: path is a valid NUL-terminated C string.
        let fd = unsafe { mhi2q_i2c_open(self.path.as_ptr()) };
        if fd < 0 {
            return Err(format!("open MFi I2C failed: errno {}", -fd));
        }
        self.fd = Some(fd);
        Ok(fd)
    }
}

impl Drop for QnxBus {
    fn drop(&mut self) {
        if let Some(fd) = self.fd.take() {
            // SAFETY: this descriptor was returned by mhi2q_i2c_open.
            unsafe { mhi2q_i2c_close(fd) };
        }
    }
}

impl RegisterBus for QnxBus {
    fn lock(&mut self) -> Result<(), String> {
        let fd = self.fd()?;
        // SAFETY: fd is open and owned by this bus.
        let result = unsafe { mhi2q_i2c_lock(fd) };
        if result != 0 {
            return Err(format!("MFi I2C lock failed: errno {}", -result));
        }
        Ok(())
    }

    fn unlock(&mut self) -> Result<(), String> {
        let fd = self.fd()?;
        // SAFETY: fd is open and owned by this bus.
        let result = unsafe { mhi2q_i2c_unlock(fd) };
        if result != 0 {
            return Err(format!("MFi I2C unlock failed: errno {}", -result));
        }
        Ok(())
    }

    fn read(&mut self, register: u8, output: &mut [u8]) -> Result<(), String> {
        let fd = self.fd()?;
        // SAFETY: output is valid for `len` bytes for the duration of this call.
        let result = unsafe {
            mhi2q_i2c_read(
                fd,
                self.address,
                register,
                output.as_mut_ptr(),
                output.len() as u32,
            )
        };
        if result != 0 {
            return Err(format!("MFi I2C read 0x{register:02x}: errno {}", -result));
        }
        Ok(())
    }

    fn write(&mut self, register: u8, data: &[u8]) -> Result<(), String> {
        let fd = self.fd()?;
        // SAFETY: data is valid for `len` bytes for the duration of this call.
        let result = unsafe {
            mhi2q_i2c_write(fd, self.address, register, data.as_ptr(), data.len() as u32)
        };
        if result != 0 {
            return Err(format!("MFi I2C write 0x{register:02x}: errno {}", -result));
        }
        Ok(())
    }
}
