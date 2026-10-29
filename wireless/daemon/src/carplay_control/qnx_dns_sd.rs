//! DNS-SD discovery using the `libdns_sd.so.1` already in the MHI2Q image.
//! Symbols are loaded at runtime so the daemon can still start in a QEMU
//! image without mdnsd. No DNS-SD library is copied into the mod.

use super::Controller;
use std::cell::RefCell;
use std::collections::{BTreeSet, VecDeque};
use std::ffi::{CStr, CString, c_char, c_void};
use std::net::{Ipv4Addr, Ipv6Addr, SocketAddr, SocketAddrV4, SocketAddrV6};
use std::sync::atomic::{AtomicBool, Ordering};
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
        permission: &AtomicBool,
        ready: impl Fn() -> bool,
    ) -> Result<(), String> {
        let fd = unsafe { (self.sock_fd)(reference) };
        if fd < 0 {
            return Err("DNS-SD returned no socket".into());
        }
        while !ready() {
            if !permission.load(Ordering::Acquire) {
                return Err("cancelled CarPlay discovery".into());
            }
            let remaining = deadline.saturating_duration_since(Instant::now());
            if remaining.is_zero() {
                return Err("CarPlay DNS-SD discovery timed out".into());
            }
            let mut pfd = PollFd {
                fd,
                events: 1,
                revents: 0,
            };
            let timeout = remaining.as_millis().min(250) as i32;
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
    hits: VecDeque<(String, String, String, u32)>,
    seen: BTreeSet<(String, String, String, u32)>,
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
        let item = (name.into(), service_type.into(), domain.into(), interface);
        if result.seen.insert(item.clone()) {
            result.hits.push_back(item);
        }
    }
}

#[derive(Default)]
struct ResolveResult {
    done: bool,
    expected: String,
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
    txt_len: u16,
    txt: *const u8,
    context: *mut c_void,
) {
    let result = unsafe { &*(context as *const RefCell<ResolveResult>) };
    let mut result = result.borrow_mut();
    result.error = error;
    result.done = true;
    if txt.is_null()
        || !super::txt_matches_peer(
            unsafe { std::slice::from_raw_parts(txt, txt_len as usize) },
            &result.expected,
        )
    {
        return;
    }
    if error != 0 || host.is_null() {
        return;
    }
    if let Ok(host) = unsafe { CStr::from_ptr(host) }.to_str() {
        result.hit = Some((host.into(), u16::from_be(port), interface));
    }
}

#[derive(Default)]
struct AddressResult {
    hit: Option<SocketAddr>,
    interface: u32,
    port: u16,
    error: i32,
}

unsafe extern "C" fn address_callback(
    _reference: Ref,
    flags: u32,
    interface: u32,
    error: i32,
    _host: *const c_char,
    address: *const c_void,
    _ttl: u32,
    context: *mut c_void,
) {
    let result = unsafe { &*(context as *const RefCell<AddressResult>) };
    let mut result = result.borrow_mut();
    result.error = error;
    if error != 0 || flags & 2 == 0 || address.is_null() || interface != result.interface {
        return;
    }
    // QNX 6.5 sockaddr_in begins with sa_len, sa_family, sin_port, s_addr.
    let bytes = unsafe { std::slice::from_raw_parts(address as *const u8, 8) };
    if bytes[1] == 2 {
        let ip = Ipv4Addr::new(bytes[4], bytes[5], bytes[6], bytes[7]);
        if !ip.is_link_local() && !ip.is_unspecified() {
            result.hit = Some(SocketAddr::V4(SocketAddrV4::new(ip, result.port)));
        }
    } else if bytes[1] == 24 {
        let bytes = unsafe { std::slice::from_raw_parts(address as *const u8, 28) };
        let ip = Ipv6Addr::from(<[u8; 16]>::try_from(&bytes[8..24]).unwrap());
        let scope = if ip.is_unicast_link_local() {
            interface
        } else {
            u32::from_ne_bytes(bytes[24..28].try_into().unwrap())
        };
        if !ip.is_unspecified() && !ip.is_multicast() {
            result.hit = Some(SocketAddr::V6(SocketAddrV6::new(ip, result.port, 0, scope)));
        }
    }
}

pub fn discover_for(
    interface_name: &str,
    peer: &str,
    timeout: Duration,
    permission: &AtomicBool,
) -> Result<Controller, String> {
    let name = CString::new(interface_name).map_err(|_| "invalid interface name")?;
    let interface = unsafe { if_nametoindex(name.as_ptr()) };
    if interface == 0 {
        return Err(format!("interface {interface_name} is unavailable"));
    }
    let dns = DnsSd::open()?;
    let deadline = Instant::now() + timeout;
    let browse_result = RefCell::new(BrowseResult::default());
    let mut reference = std::ptr::null_mut();
    let code = unsafe {
        (dns.browse)(
            &mut reference,
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
        value: reference,
        dns: &dns,
    };
    loop {
        dns.wait_until(browse.value, deadline, permission, || {
            let r = browse_result.borrow();
            !r.hits.is_empty() || r.error != 0
        })?;
        if browse_result.borrow().error != 0 {
            return Err("DNS-SD browse error".into());
        }
        let (service, kind, domain, index) = browse_result.borrow_mut().hits.pop_front().unwrap();
        if index != interface {
            continue;
        }
        if let Some(controller) = resolve_peer(
            &dns, &service, &kind, &domain, index, peer, deadline, permission,
        )? {
            return Ok(controller);
        }
    }
}

fn resolve_peer(
    dns: &DnsSd,
    service: &str,
    kind: &str,
    domain: &str,
    interface: u32,
    peer: &str,
    deadline: Instant,
    permission: &AtomicBool,
) -> Result<Option<Controller>, String> {
    let result = RefCell::new(ResolveResult {
        expected: peer.into(),
        ..ResolveResult::default()
    });
    let mut reference = std::ptr::null_mut();
    let service = CString::new(service).map_err(|_| "invalid service")?;
    let kind = CString::new(kind).map_err(|_| "invalid service type")?;
    let domain = CString::new(domain).map_err(|_| "invalid domain")?;
    let code = unsafe {
        (dns.resolve)(
            &mut reference,
            0,
            interface,
            service.as_ptr(),
            kind.as_ptr(),
            domain.as_ptr(),
            resolve_callback,
            &result as *const _ as *mut c_void,
        )
    };
    if code != 0 {
        return Err(format!("DNSServiceResolve failed: {code}"));
    }
    let resolved = ServiceRef {
        value: reference,
        dns,
    };
    dns.wait_until(resolved.value, deadline, permission, || {
        result.borrow().done
    })?;
    let result = result.into_inner();
    if result.error != 0 {
        return Err(format!("DNS-SD resolve error: {}", result.error));
    }
    let Some((host, port, index)) = result.hit else {
        return Ok(None);
    };
    if index != interface || port == 0 {
        return Ok(None);
    }
    drop(resolved);
    let result = RefCell::new(AddressResult {
        interface,
        port,
        ..AddressResult::default()
    });
    let mut reference = std::ptr::null_mut();
    let hostname = CString::new(host.clone()).map_err(|_| "invalid host")?;
    let code = unsafe {
        (dns.get_addr)(
            &mut reference,
            0,
            interface,
            3,
            hostname.as_ptr(),
            address_callback,
            &result as *const _ as *mut c_void,
        )
    };
    if code != 0 {
        return Err(format!("DNSServiceGetAddrInfo failed: {code}"));
    }
    let addressed = ServiceRef {
        value: reference,
        dns,
    };
    dns.wait_until(addressed.value, deadline, permission, || {
        let r = result.borrow();
        r.hit.is_some() || r.error != 0
    })?;
    let result = result.into_inner();
    if result.error != 0 {
        return Err(format!("DNS-SD address error: {}", result.error));
    }
    Ok(Some(Controller {
        address: result.hit.ok_or("no CarPlay address")?,
        host_name: host,
    }))
}
