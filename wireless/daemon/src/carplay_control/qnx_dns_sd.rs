//! DNS-SD discovery using the `libdns_sd.so.1` already in the MHI2Q image.
//! Symbols are loaded at runtime so the daemon can still start in a QEMU
//! image without mdnsd. No DNS-SD library is copied into the mod.

use super::Controller;
use std::cell::RefCell;
use std::ffi::{CStr, CString, c_char, c_void};
use std::net::{Ipv4Addr, SocketAddr, SocketAddrV4};
use std::time::{Duration, Instant};

type Ref = *mut c_void;
type BrowseCallback = unsafe extern "C" fn(
    Ref,
    u32,
    u32,
    i32,
    *const c_char,
    *const c_char,
    *const c_char,
    *mut c_void,
);
type ResolveCallback = unsafe extern "C" fn(
    Ref,
    u32,
    u32,
    i32,
    *const c_char,
    *const c_char,
    u16,
    u16,
    *const u8,
    *mut c_void,
);
type AddressCallback =
    unsafe extern "C" fn(Ref, u32, u32, i32, *const c_char, *const c_void, u32, *mut c_void);
type BrowseFn = unsafe extern "C" fn(
    *mut Ref,
    u32,
    u32,
    *const c_char,
    *const c_char,
    BrowseCallback,
    *mut c_void,
) -> i32;
type ResolveFn = unsafe extern "C" fn(
    *mut Ref,
    u32,
    u32,
    *const c_char,
    *const c_char,
    *const c_char,
    ResolveCallback,
    *mut c_void,
) -> i32;
type GetAddrFn = unsafe extern "C" fn(
    *mut Ref,
    u32,
    u32,
    u32,
    *const c_char,
    AddressCallback,
    *mut c_void,
) -> i32;
type SockFdFn = unsafe extern "C" fn(Ref) -> i32;
type ProcessFn = unsafe extern "C" fn(Ref) -> i32;
type DeallocateFn = unsafe extern "C" fn(Ref);

unsafe extern "C" {
    fn dlopen(path: *const c_char, flags: i32) -> *mut c_void;
    fn dlsym(handle: *mut c_void, symbol: *const c_char) -> *mut c_void;
    fn dlclose(handle: *mut c_void) -> i32;
    fn if_nametoindex(name: *const c_char) -> u32;
    fn poll(fds: *mut PollFd, count: u32, timeout_ms: i32) -> i32;
}

#[repr(C)]
struct PollFd {
    fd: i32,
    events: i16,
    revents: i16,
}

struct DnsSd {
    handle: *mut c_void,
    browse: BrowseFn,
    resolve: ResolveFn,
    get_addr: GetAddrFn,
    sock_fd: SockFdFn,
    process: ProcessFn,
    deallocate: DeallocateFn,
}

impl DnsSd {
    fn open() -> Result<Self, String> {
        // SAFETY: the literal is NUL terminated, and the handle is closed in Drop.
        let handle = unsafe { dlopen(c"libdns_sd.so.1".as_ptr(), 2) };
        if handle.is_null() {
            return Err("libdns_sd.so.1 is unavailable".into());
        }
        unsafe fn symbol<T: Copy>(handle: *mut c_void, name: &CStr) -> Result<T, String> {
            // SAFETY: symbols are resolved from the firmware's DNS-SD library
            // using their published C ABI, with function-pointer-sized values.
            let pointer = unsafe { dlsym(handle, name.as_ptr()) };
            if pointer.is_null() || std::mem::size_of::<T>() != std::mem::size_of::<*mut c_void>() {
                return Err(format!("missing DNS-SD symbol {}", name.to_string_lossy()));
            }
            Ok(unsafe { std::mem::transmute_copy(&pointer) })
        }
        let loaded = (|| unsafe {
            Ok(Self {
                handle,
                browse: symbol(handle, c"DNSServiceBrowse")?,
                resolve: symbol(handle, c"DNSServiceResolve")?,
                get_addr: symbol(handle, c"DNSServiceGetAddrInfo")?,
                sock_fd: symbol(handle, c"DNSServiceRefSockFD")?,
                process: symbol(handle, c"DNSServiceProcessResult")?,
                deallocate: symbol(handle, c"DNSServiceRefDeallocate")?,
            })
        })();
        if loaded.is_err() {
            // SAFETY: no ServiceRef was created yet.
            unsafe { dlclose(handle) };
        }
        loaded
    }

    fn wait_until(
        &self,
        reference: Ref,
        deadline: Instant,
        ready: impl Fn() -> bool,
    ) -> Result<(), String> {
        let fd = unsafe { (self.sock_fd)(reference) };
        if fd < 0 {
            return Err("DNS-SD returned no socket".into());
        }
        while !ready() {
            let remaining = deadline.saturating_duration_since(Instant::now());
            if remaining.is_zero() {
                return Err("CarPlay DNS-SD discovery timed out".into());
            }
            let mut pfd = PollFd {
                fd,
                events: 1,
                revents: 0,
            };
            let timeout = remaining.as_millis().min(1000) as i32;
            // SAFETY: pfd is a valid one-element pollfd array.
            let polled = unsafe { poll(&mut pfd, 1, timeout) };
            if polled < 0 {
                return Err("DNS-SD poll failed".into());
            }
            if polled > 0 {
                if pfd.revents & 1 == 0 {
                    return Err(format!("DNS-SD socket event 0x{:x}", pfd.revents));
                }
                // SAFETY: reference remains live until after this call.
                let result = unsafe { (self.process)(reference) };
                if result != 0 {
                    return Err(format!("DNS-SD processing failed: {result}"));
                }
            }
        }
        Ok(())
    }
}

impl Drop for DnsSd {
    fn drop(&mut self) {
        // SAFETY: handle was returned by dlopen.
        unsafe { dlclose(self.handle) };
    }
}

struct ServiceRef<'a> {
    value: Ref,
    dns: &'a DnsSd,
}
impl Drop for ServiceRef<'_> {
    fn drop(&mut self) {
        if !self.value.is_null() {
            // SAFETY: DNSServiceRef was created by this loaded library.
            unsafe { (self.dns.deallocate)(self.value) };
        }
    }
}

#[derive(Default)]
struct BrowseResult {
    hit: Option<(String, String, String, u32)>,
    error: i32,
}

unsafe extern "C" fn browse_callback(
    _reference: Ref,
    flags: u32,
    interface: u32,
    error: i32,
    name: *const c_char,
    service_type: *const c_char,
    domain: *const c_char,
    context: *mut c_void,
) {
    let result = unsafe { &*(context as *const RefCell<BrowseResult>) };
    let mut result = result.borrow_mut();
    result.error = error;
    if error != 0 || flags & 2 == 0 || name.is_null() || service_type.is_null() || domain.is_null()
    {
        return;
    }
    let fields = unsafe {
        (
            CStr::from_ptr(name).to_str(),
            CStr::from_ptr(service_type).to_str(),
            CStr::from_ptr(domain).to_str(),
        )
    };
    if let (Ok(name), Ok(service_type), Ok(domain)) = fields {
        result.hit = Some((name.into(), service_type.into(), domain.into(), interface));
    }
}

#[derive(Default)]
struct ResolveResult {
    hit: Option<(String, u16, u32)>,
    error: i32,
}

unsafe extern "C" fn resolve_callback(
    _reference: Ref,
    _flags: u32,
    interface: u32,
    error: i32,
    _full_name: *const c_char,
    host: *const c_char,
    port: u16,
    _txt_len: u16,
    _txt: *const u8,
    context: *mut c_void,
) {
    let result = unsafe { &*(context as *const RefCell<ResolveResult>) };
    let mut result = result.borrow_mut();
    result.error = error;
    if error != 0 || host.is_null() {
        return;
    }
    if let Ok(host) = unsafe { CStr::from_ptr(host) }.to_str() {
        result.hit = Some((host.into(), u16::from_be(port), interface));
    }
}

#[derive(Default)]
struct AddressResult {
    hit: Option<Ipv4Addr>,
    error: i32,
}

unsafe extern "C" fn address_callback(
    _reference: Ref,
    flags: u32,
    _interface: u32,
    error: i32,
    _host: *const c_char,
    address: *const c_void,
    _ttl: u32,
    context: *mut c_void,
) {
    let result = unsafe { &*(context as *const RefCell<AddressResult>) };
    let mut result = result.borrow_mut();
    result.error = error;
    if error != 0 || flags & 2 == 0 || address.is_null() {
        return;
    }
    // QNX 6.5 sockaddr_in begins with sa_len, sa_family, sin_port, s_addr.
    let bytes = unsafe { std::slice::from_raw_parts(address as *const u8, 8) };
    if bytes[1] == 2 {
        let ip = Ipv4Addr::new(bytes[4], bytes[5], bytes[6], bytes[7]);
        if !ip.is_link_local() && !ip.is_unspecified() {
            result.hit = Some(ip);
        }
    }
}

pub fn discover_on(interface_name: &str, timeout: Duration) -> Result<Controller, String> {
    let name = CString::new(interface_name).map_err(|_| "invalid interface name")?;
    // SAFETY: name is a valid C string; zero means the AP interface is absent.
    let interface = unsafe { if_nametoindex(name.as_ptr()) };
    if interface == 0 {
        return Err(format!("interface {interface_name} is unavailable"));
    }
    let dns = DnsSd::open()?;
    let deadline = Instant::now() + timeout;

    let browse_result = RefCell::new(BrowseResult::default());
    let mut browse_ref = std::ptr::null_mut();
    let code = unsafe {
        (dns.browse)(
            &mut browse_ref,
            0,
            interface,
            c"_carplay-ctrl._tcp".as_ptr(),
            c"local.".as_ptr(),
            browse_callback,
            &browse_result as *const _ as *mut c_void,
        )
    };
    if code != 0 {
        return Err(format!("DNSServiceBrowse failed: {code}"));
    }
    let browse = ServiceRef {
        value: browse_ref,
        dns: &dns,
    };
    dns.wait_until(browse.value, deadline, || {
        let result = browse_result.borrow();
        result.hit.is_some() || result.error != 0
    })?;
    let browse_result = browse_result.into_inner();
    if browse_result.error != 0 {
        return Err(format!("DNS-SD browse error: {}", browse_result.error));
    }
    let (service, kind, domain, service_interface) =
        browse_result.hit.ok_or("no CarPlay controller")?;
    drop(browse);

    let resolve_result = RefCell::new(ResolveResult::default());
    let mut resolve_ref = std::ptr::null_mut();
    let service = CString::new(service).map_err(|_| "invalid DNS-SD service name")?;
    let kind = CString::new(kind).map_err(|_| "invalid DNS-SD service type")?;
    let domain = CString::new(domain).map_err(|_| "invalid DNS-SD domain")?;
    let code = unsafe {
        (dns.resolve)(
            &mut resolve_ref,
            0,
            service_interface,
            service.as_ptr(),
            kind.as_ptr(),
            domain.as_ptr(),
            resolve_callback,
            &resolve_result as *const _ as *mut c_void,
        )
    };
    if code != 0 {
        return Err(format!("DNSServiceResolve failed: {code}"));
    }
    let resolved = ServiceRef {
        value: resolve_ref,
        dns: &dns,
    };
    dns.wait_until(resolved.value, deadline, || {
        let result = resolve_result.borrow();
        result.hit.is_some() || result.error != 0
    })?;
    let resolve_result = resolve_result.into_inner();
    if resolve_result.error != 0 {
        return Err(format!("DNS-SD resolve error: {}", resolve_result.error));
    }
    let (host, port, host_interface) = resolve_result.hit.ok_or("no CarPlay host")?;
    drop(resolved);

    let address_result = RefCell::new(AddressResult::default());
    let mut address_ref = std::ptr::null_mut();
    let host_c = CString::new(host.clone()).map_err(|_| "invalid CarPlay host")?;
    let code = unsafe {
        (dns.get_addr)(
            &mut address_ref,
            0,
            host_interface,
            1,
            host_c.as_ptr(),
            address_callback,
            &address_result as *const _ as *mut c_void,
        )
    };
    if code != 0 {
        return Err(format!("DNSServiceGetAddrInfo failed: {code}"));
    }
    let addressed = ServiceRef {
        value: address_ref,
        dns: &dns,
    };
    dns.wait_until(addressed.value, deadline, || {
        let result = address_result.borrow();
        result.hit.is_some() || result.error != 0
    })?;
    let address_result = address_result.into_inner();
    if address_result.error != 0 {
        return Err(format!("DNS-SD address error: {}", address_result.error));
    }
    let address = address_result.hit.ok_or("no CarPlay IPv4 address")?;
    Ok(Controller {
        address: SocketAddr::V4(SocketAddrV4::new(address, port)),
        host_name: host,
    })
}
