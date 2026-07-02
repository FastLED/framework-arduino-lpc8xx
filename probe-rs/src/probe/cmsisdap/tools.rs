use super::CmsisDapDevice;
use crate::probe::{
    BoxedProbeError, DebugProbeInfo, DebugProbeSelector, ProbeCreationError,
    cmsisdap::{CmsisDapFactory, commands::CmsisDapError, commands::DEFAULT_USB_TIMEOUT},
};
#[cfg(feature = "cmsisdap_v1")]
use hidapi::HidApi;
use nusb::{DeviceInfo, MaybeFuture, descriptors::TransferType, transfer::Direction};

const USB_CLASS_HID: u8 = 0x03;
const USB_CMSIS_DAP_CLASS: u8 = 0xFF;
const USB_CMSIS_DAP_SUBCLASS: u8 = 0;

/// Finds all CMSIS-DAP devices, either v1 (HID) or v2 (WinUSB Bulk).
///
/// This method uses nusb to read device strings, which might fail due
/// to permission or driver errors, so it falls back to listing only
/// HID devices if it does not find any suitable devices.
#[tracing::instrument(skip_all)]
pub fn list_cmsisdap_devices() -> Vec<DebugProbeInfo> {
    tracing::debug!("Searching for CMSIS-DAP probes using nusb");

    #[cfg_attr(not(feature = "cmsisdap_v1"), expect(unused_mut))]
    let mut probes = match nusb::list_devices().wait() {
        Ok(devices) => devices
            .flat_map(|device| get_cmsisdap_info(&device, false))
            .collect(),
        Err(e) => {
            tracing::warn!("error listing devices with nusb: {e}");
            vec![]
        }
    };

    #[cfg(feature = "cmsisdap_v1")]
    tracing::debug!(
        "Found {} CMSIS-DAP probes using nusb, searching HID",
        probes.len()
    );

    #[cfg(feature = "cmsisdap_v1")]
    if let Ok(api) = hidapi::HidApi::new() {
        for device in api.device_list() {
            if let Some(info) = get_cmsisdap_hid_info(device) {
                if !probes.iter().any(|p| {
                    p.vendor_id == info.vendor_id
                        && p.product_id == info.product_id
                        && p.serial_number.as_deref().unwrap_or("")
                            == info.serial_number.as_deref().unwrap_or("")
                }) {
                    tracing::trace!("Adding new HID-only probe {:?}", info);
                    probes.push(info)
                } else {
                    tracing::trace!("Ignoring duplicate {:?}", info);
                }
            }
        }
    }

    tracing::debug!("Found {} CMSIS-DAP probes total", probes.len());
    probes
}

/// Checks if a given Device is a CMSIS-DAP probe, returning Some(DebugProbeInfo) if so.
///
/// If `list_both_versions` is true, both v1 and v2 interfaces will be returned.
/// Otherwise, only v2 interfaces will be returned, unless there are no v2 interfaces,
/// in which case v1 interfaces will be returned.
///
/// To list probes, this function should be called with `list_both_versions` set to false,
/// so that devices with both v1 and v2 interfaces are listed only once.
///
/// To open a probe, this function should be called with `list_both_versions` set to true,
/// so that the user can manually fall back to the v1 interface.
fn get_cmsisdap_info(device: &DeviceInfo, list_both_versions: bool) -> Vec<DebugProbeInfo> {
    // Open device handle and read basic information
    let prod_str = device.product_string().unwrap_or("");
    let sn_str = device.serial_number();

    // Most CMSIS-DAP probes say something like "CMSIS-DAP"
    let cmsis_dap_product = is_cmsis_dap(prod_str) || is_known_cmsis_dap_dev(device);

    let mut v1_ifaces = vec![];
    let mut v2_ifaces = vec![];

    // Iterate all interfaces, looking for:
    // 1. Any with CMSIS-DAP in their interface string
    // 2. Any that are HID, if the product string says CMSIS-DAP,
    //    to save for potential HID-only operation.
    for interface in device.interfaces() {
        let Some(interface_desc) = interface.interface_string() else {
            tracing::trace!(
                "interface {} has no string, skipping",
                interface.interface_number()
            );
            continue;
        };
        if is_cmsis_dap(interface_desc) {
            tracing::trace!(
                "  Interface {}: {}",
                interface.interface_number(),
                interface_desc,
            );
            let selected_interface = Some(interface.interface_number());
            let is_hid_interface = if interface.class() == USB_CLASS_HID {
                tracing::trace!("    HID interface found");
                true
            } else if (interface.class(), interface.subclass())
                != (USB_CMSIS_DAP_CLASS, USB_CMSIS_DAP_SUBCLASS)
            {
                tracing::trace!(
                    "Interface {} has a cmsis-dap description but wrong classes ({}, {}), skipping",
                    interface.interface_number(),
                    interface.class(),
                    interface.subclass(),
                );
                // Not a CMSIS-DAP v2 interface, skip.
                continue;
            } else {
                false
            };

            let info = DebugProbeInfo::new(
                prod_str.to_string(),
                device.vendor_id(),
                device.product_id(),
                sn_str.map(Into::into),
                &CmsisDapFactory,
                selected_interface,
                is_hid_interface,
            );

            if is_hid_interface {
                v1_ifaces.push(info);
            } else {
                v2_ifaces.push(info);
            }
        }
    }

    if cmsis_dap_product {
        tracing::trace!(
            "{}: CMSIS-DAP device with {} interfaces",
            prod_str,
            device.interfaces().count()
        );
        if !v1_ifaces.is_empty() {
            tracing::trace!("Device has {} CMSIS-DAPv1 interfaces", v1_ifaces.len());
        }
        if !v2_ifaces.is_empty() {
            tracing::trace!("Device has {} CMSIS-DAPv2 interfaces", v2_ifaces.len());
        }
    }
    // Make sure cmsis-dap v2 interfaces are tried first
    let mut results = v2_ifaces;
    if list_both_versions || results.is_empty() {
        results.extend(v1_ifaces);
    }
    results
}

/// Checks if a given HID device is a CMSIS-DAP v1 probe, returning Some(DebugProbeInfo) if so.
#[cfg(feature = "cmsisdap_v1")]
fn get_cmsisdap_hid_info(device: &hidapi::DeviceInfo) -> Option<DebugProbeInfo> {
    let prod_str = device.product_string().unwrap_or("");
    let path = device.path().to_str().unwrap_or("");
    if is_cmsis_dap(prod_str) || is_cmsis_dap(path) {
        tracing::trace!("CMSIS-DAP device with USB path: {:?}", device.path());
        tracing::trace!("                product_string: {:?}", prod_str);
        tracing::trace!(
            "                     interface: {}",
            device.interface_number()
        );

        Some(DebugProbeInfo::new(
            prod_str.to_owned(),
            device.vendor_id(),
            device.product_id(),
            device.serial_number().map(|s| s.to_owned()),
            &CmsisDapFactory,
            Some(device.interface_number() as u8),
            true,
        ))
    } else {
        None
    }
}

/// Attempt to open the given device as a CMSIS-DAP v1 probe over
/// nusb (WinUSB on Windows, usbfs on Linux, IOKit on macOS) rather
/// than hidapi.
///
/// FastLED/fbuild#936. Composite CMSIS-DAP devices — most notably NXP
/// LPC-Link2 (`1FC9:0090` / `1FC9:0132`) — expose the DAP HID as one
/// interface in a multi-interface composite that also carries a CDC
/// UART bridge. On Windows, hidapi's interface picker + the usbccgp
/// composite driver stack combine to prevent it from opening the DAP
/// HID at all. MCUXpresso works because it uses WinUSB directly.
/// This function does the same via nusb.
///
/// The function scans the device's interfaces for one that has an
/// interrupt IN + interrupt OUT endpoint pair on the CMSIS-DAP HID
/// interface (class `0x03`) and returns a
/// [`CmsisDapDevice::V1Nusb`] wrapping it.
pub fn open_v1_nusb_device(
    device_info: &DeviceInfo,
    selected_interface: Option<u8>,
) -> Result<Option<CmsisDapDevice>, ProbeCreationError> {
    let vid = device_info.vendor_id();
    let pid = device_info.product_id();

    tracing::trace!(
        "Trying to open {:04x}:{:04x} in cmsis-dap v1 mode via nusb (bypassing hidapi)",
        vid,
        pid
    );

    let device = match device_info.open().wait() {
        Ok(d) => d,
        Err(e) => {
            tracing::debug!(
                vendor_id = %format!("{vid:04x}"),
                product_id = %format!("{pid:04x}"),
                error = %e,
                "failed to open device for CMSIS-DAP v1 via nusb"
            );
            return Ok(None);
        }
    };

    let Some(c_desc) = device.configurations().next() else {
        tracing::trace!("No configurations exposed by device");
        return Ok(None);
    };

    for interface in c_desc.interfaces() {
        if let Some(iface) = selected_interface
            && interface.interface_number() != iface
        {
            continue;
        }

        for i_desc in interface.alt_settings() {
            // CMSIS-DAP v1 lives on a HID interface (class 0x03).
            if i_desc.class() != USB_CLASS_HID {
                continue;
            }

            // Skip interfaces that don't LOOK like a CMSIS-DAP surface
            // on the parent device. On NXP LPC-Link2 the v1.0.7
            // firmware doesn't populate a per-interface string; fall
            // back to matching by product string in that case.
            let interface_str = device_info
                .interfaces()
                .find(|i| i.interface_number() == interface.interface_number())
                .and_then(|i| i.interface_string());
            let product_string = device_info.product_string().unwrap_or("");
            let looks_like_dap =
                interface_str.is_some_and(is_cmsis_dap) || is_cmsis_dap(product_string);
            if !looks_like_dap {
                continue;
            }

            let eps: Vec<_> = i_desc.endpoints().collect();
            if eps.len() < 2 {
                continue;
            }

            // Find one interrupt-OUT and one interrupt-IN endpoint.
            let mut out_ep = None;
            let mut in_ep = None;
            let mut max_packet_size = 64usize;
            for ep in &eps {
                if ep.transfer_type() != TransferType::Interrupt {
                    continue;
                }
                match ep.direction() {
                    Direction::Out => {
                        out_ep = Some(ep.address());
                        max_packet_size = max_packet_size.max(ep.max_packet_size());
                    }
                    Direction::In => {
                        in_ep = Some(ep.address());
                        max_packet_size = max_packet_size.max(ep.max_packet_size());
                    }
                }
            }

            let (Some(out_ep), Some(in_ep)) = (out_ep, in_ep) else {
                tracing::trace!(
                    "Interface {} HID class but no interrupt IN+OUT pair; skipping",
                    interface.interface_number()
                );
                continue;
            };

            match device.claim_interface(interface.interface_number()).wait() {
                Ok(handle) => {
                    tracing::debug!(
                        "Opening {:04x}:{:04x} in CMSIS-DAP v1 mode over nusb (interface {}, out_ep {:#x}, in_ep {:#x}, mps {})",
                        vid, pid, interface.interface_number(), out_ep, in_ep, max_packet_size
                    );
                    reject_probe_by_version(
                        device_info.vendor_id(),
                        device_info.product_id(),
                        device_info.device_version(),
                    )?;
                    return Ok(Some(CmsisDapDevice::V1Nusb {
                        handle,
                        out_ep,
                        in_ep,
                        // CMSIS-DAP v1 HID report size is fixed to the
                        // endpoint max_packet_size on almost every
                        // conforming probe; the exact size gets
                        // negotiated by `find_packet_size()` at the
                        // start of the session.
                        report_size: max_packet_size,
                        usb_timeout: DEFAULT_USB_TIMEOUT,
                    }));
                }
                Err(e) => {
                    tracing::debug!(
                        interface = interface.interface_number(),
                        error = %e,
                        "failed to claim CMSIS-DAP v1 HID interface via nusb"
                    );
                    // Don't `continue` — if we saw the right interface
                    // and couldn't claim it, keep looking, but a claim
                    // failure on the DAP HID typically means someone
                    // else has the handle open. Fall through.
                    continue;
                }
            }
        }
    }

    tracing::debug!(
        "Could not open {:04x}:{:04x} in CMSIS-DAP v1 mode via nusb",
        vid,
        pid
    );
    Ok(None)
}

/// Attempt to open the given device in CMSIS-DAP v2 mode
pub fn open_v2_device(
    device_info: &DeviceInfo,
    selected_interface: Option<u8>,
) -> Result<Option<CmsisDapDevice>, ProbeCreationError> {
    // Open device handle and read basic information
    let vid = device_info.vendor_id();
    let pid = device_info.product_id();

    tracing::trace!(
        "Trying to open {:04x}:{:04x} in cmsis-dap v2 mode",
        vid,
        pid
    );

    let device = match device_info.open().wait() {
        Ok(device) => device,
        Err(e) => {
            tracing::debug!(
                vendor_id = %format!("{vid:04x}"),
                product_id = %format!("{pid:04x}"),
                error = %e,
                "failed to open device for CMSIS-DAP v2"
            );
            return Ok(None);
        }
    };

    // Go through interfaces to try and find a v2 interface.
    // The CMSIS-DAPv2 spec says that v2 interfaces should use a specific
    // WinUSB interface GUID, but in addition to being hard to read, the
    // official DAPLink firmware doesn't use it. Instead, we scan for an
    // interface whose string like "CMSIS-DAP" and has two or three
    // endpoints of the correct type and direction.
    let Some(c_desc) = device.configurations().next() else {
        tracing::trace!("No cmsis-dap v2 interface found");
        return Ok(None);
    };
    for interface in c_desc.interfaces() {
        tracing::trace!("Checking interface {}", interface.interface_number());
        if let Some(iface) = selected_interface
            && interface.interface_number() != iface
        {
            tracing::trace!(
                "Interface number does not match selector {} != {}",
                iface,
                interface.interface_number()
            );
            continue;
        }
        for i_desc in interface.alt_settings() {
            // Skip interfaces without "CMSIS-DAP" like pattern in their string
            let Some(interface_str) = device_info
                .interfaces()
                .find(|i| i.interface_number() == interface.interface_number())
                .and_then(|i| i.interface_string())
            else {
                tracing::trace!("Interface does not have interface string");
                continue;
            };
            if !is_cmsis_dap(interface_str) {
                tracing::trace!("Interface does not have 'CMSIS-DAP' in string");
                continue;
            }

            // Skip interfaces without 2 or 3 endpoints
            let n_ep = i_desc.num_endpoints();
            if !(2..=3).contains(&n_ep) {
                tracing::trace!(
                    "Interface does not have the correct number of endpoints ({})",
                    n_ep
                );
                continue;
            }

            let eps: Vec<_> = i_desc.endpoints().collect();

            // Check the first endpoint is bulk out
            if eps[0].transfer_type() != TransferType::Bulk || eps[0].direction() != Direction::Out
            {
                tracing::trace!("First interface endpoint is not bulk out");
                continue;
            }

            // Check the second endpoint is bulk in
            if eps[1].transfer_type() != TransferType::Bulk || eps[1].direction() != Direction::In {
                tracing::trace!("Second interface endpoint is not bulk in");
                continue;
            }

            // Detect a third bulk EP which will be for SWO streaming
            let mut swo_ep = None;

            if eps.len() > 2
                && eps[2].transfer_type() == TransferType::Bulk
                && eps[2].direction() == Direction::In
            {
                swo_ep = Some((eps[2].address(), eps[2].max_packet_size()));
            }
            // Attempt to claim this interface
            match device.claim_interface(interface.interface_number()).wait() {
                Ok(handle) => {
                    tracing::debug!("Opening {:04x}:{:04x} in CMSIS-DAPv2 mode", vid, pid);
                    reject_probe_by_version(
                        device_info.vendor_id(),
                        device_info.product_id(),
                        device_info.device_version(),
                    )?;
                    return Ok(Some(CmsisDapDevice::V2 {
                        handle,
                        out_ep: eps[0].address(),
                        in_ep: eps[1].address(),
                        swo_ep,
                        max_packet_size: eps[1].max_packet_size(),
                        usb_timeout: DEFAULT_USB_TIMEOUT,
                    }));
                }
                Err(e) => {
                    tracing::debug!(
                        interface = interface.interface_number(),
                        error = %e,
                        "failed to claim interface"
                    );
                    continue;
                }
            }
        }
    }

    // Could not open in v2
    tracing::debug!(
        "Could not open {:04x}:{:04x} in CMSIS-DAP v2 mode",
        vid,
        pid
    );
    Ok(None)
}

fn reject_probe_by_version(
    vendor_id: u16,
    product_id: u16,
    device_version: u16,
) -> Result<(), ProbeCreationError> {
    let denylist = [
        |vid, pid, version| (vid == 0x2e8a && pid == 0x000c && version < 0x0220).then_some("2.2.0"), // Old RPi debugprobe
    ];

    tracing::debug!(
        "Checking against denylist: {:04x}:{:04x} v{:04x}",
        vendor_id,
        product_id,
        device_version
    );
    for deny in denylist {
        if let Some(min_version) = deny(vendor_id, product_id, device_version) {
            return Err(ProbeCreationError::ProbeSpecific(BoxedProbeError::from(
                CmsisDapError::ProbeFirmwareOutdated(min_version),
            )));
        }
    }

    Ok(())
}

/// Attempt to open the given DebugProbeInfo in CMSIS-DAP v2 mode if possible,
/// otherwise in v1 mode.
pub fn open_device_from_selector(
    selector: &DebugProbeSelector,
) -> Result<CmsisDapDevice, ProbeCreationError> {
    tracing::trace!("Attempting to open device matching {}", selector);

    // We need to use nusb to detect the proper HID interface to use
    // if a probe has multiple HID interfaces. The hidapi lib unfortunately
    // offers no method to get the interface description string directly,
    // so we retrieve the device information using nusb and store it here.
    //
    // If nusb cannot be used, we will just use the first HID interface and
    // try to open that.
    #[cfg(feature = "cmsisdap_v1")]
    let mut hid_device_info = None;

    // Try using nusb to open a v2 device. This might fail if
    // the device does not support v2 operation or due to driver
    // or permission issues with opening bulk devices.
    match nusb::list_devices().wait() {
        Ok(devices) => {
            for device in devices {
                tracing::trace!("Trying device {:?}", device);

                if selector.matches(&device)
                    && let Some(device_info) =
                        get_cmsisdap_info(&device, true).into_iter().find(|dpi| {
                            tracing::trace!("DebugProbeInfo: {:?}", dpi);
                            // Only compare if the selector has an interface to compare to
                            selector.interface.is_none_or(|i| Some(i) == dpi.interface)
                        })
                {
                    // If the VID, PID, and potentially SN all match,
                    // and the device is a valid CMSIS-DAP probe,
                    // attempt to open the device in v2 mode.
                    if let Some(device) = open_v2_device(&device, device_info.interface)? {
                        return Ok(device);
                    }

                    // FastLED/fbuild#936. Before falling back to
                    // hidapi (which cannot open composite CMSIS-DAP
                    // HID interfaces on Windows for e.g. NXP
                    // LPC-Link2 debuggers), try to open the v1 HID
                    // interface directly through nusb. This is the
                    // same USB stack MCUXpresso uses and it bypasses
                    // hidapi's Windows interface picker.
                    if device_info.is_hid_interface
                        && let Some(handle) =
                            open_v1_nusb_device(&device, device_info.interface)?
                    {
                        return Ok(handle);
                    }

                    #[cfg(feature = "cmsisdap_v1")]
                    {
                        // Otherwise, save as a potential CMSIS-DAP v1 HID
                        // device and continue — hidapi fallback below.
                        hid_device_info = Some(device_info);
                    }
                }

                tracing::trace!("Device did not match");
            }
        }
        Err(e) => {
            tracing::debug!("No devices matched using nusb: {e}");
        }
    }

    #[cfg(not(feature = "cmsisdap_v1"))]
    return Err(ProbeCreationError::NotFound);

    #[cfg(feature = "cmsisdap_v1")]
    {
        // If nusb failed or the device didn't support v2, try using hidapi to open in v1 mode.
        let vid = selector.vendor_id;
        let pid = selector.product_id;
        let sn = selector.serial_number.as_deref();

        tracing::debug!(
            "Attempting to open {:04x}:{:04x} in CMSIS-DAP v1 mode",
            vid,
            pid
        );

        // Attempt to open provided VID/PID/SN with hidapi

        let Ok(hid_api) = HidApi::new() else {
            return Err(ProbeCreationError::NotFound);
        };

        let mut device_list = hid_api.device_list();

        // We have to filter manually so that we can check the correct HID interface number.
        // Using HidApi::open() will return the first device which matches PID and VID,
        // which is not always what we want.
        let device_info = device_list
            .find(|info| {
                let mut device_match = info.vendor_id() == vid && info.product_id() == pid;

                tracing::trace!(
                    "hidapi candidate: {:04x}:{:04x} interface_number={} usage_page={:04x} usage={:04x} path={:?}",
                    info.vendor_id(),
                    info.product_id(),
                    info.interface_number(),
                    info.usage_page(),
                    info.usage(),
                    info.path(),
                );

                if let Some(sn) = sn {
                    device_match &= Some(sn) == info.serial_number();
                }

                // FastLED/fbuild#935: NXP LPC-Link2 composite quirk.
                //
                // The device exposes CMSIS-DAP on HID interface 0
                // plus a UART-bridge / SWO sink on other interfaces
                // that will `hid_open()` fine but silently drop every
                // DAP command. Mirrors OpenOCD's guard at
                // cmsis_dap_usb_hid.c:107-109 (PID 0x0090 upstream;
                // extended here to 0x0132 for the LPC845-BRK v1.0.7
                // firmware variant).
                //
                // On Windows, hidapi cannot always determine
                // interface_number for a composite HID and returns -1.
                // Since only the DAP HID gets enumerated by hidapi on
                // Windows (the CDC parts of the composite aren't HID
                // and don't show up here at all), a -1 reading is
                // safe to accept.
                let is_lpc_link2 = info.vendor_id() == 0x1fc9
                    && matches!(info.product_id(), 0x0090 | 0x0132);

                if is_lpc_link2 && info.interface_number() > 0 {
                    return false;
                }

                if let Some(hid_interface) = hid_device_info
                    .as_ref()
                    .and_then(|info| info.interface.filter(|_| info.is_hid_interface))
                {
                    // Skip the sub-filter for LPC-Link2 when hidapi
                    // returned -1 — the interface_number > 0 guard
                    // above already excludes the wrong candidates.
                    if !(is_lpc_link2 && info.interface_number() < 0) {
                        device_match &= info.interface_number() == hid_interface as i32;
                    }
                }

                device_match
            })
            .ok_or(ProbeCreationError::NotFound)?;

        let device = match device_info.open_device(&hid_api) {
            Ok(d) => d,
            Err(e) => {
                tracing::debug!(
                    vendor_id = %format!("{vid:04x}"),
                    product_id = %format!("{pid:04x}"),
                    path = ?device_info.path(),
                    error = %e,
                    "hidapi failed to open CMSIS-DAP v1 HID device"
                );
                return Err(ProbeCreationError::NotFound);
            }
        };

        match device.get_product_string() {
            Ok(Some(s)) if is_cmsis_dap(&s) => {
                reject_probe_by_version(
                    device_info.vendor_id(),
                    device_info.product_id(),
                    device_info.release_number(),
                )?;
                Ok(CmsisDapDevice::V1 {
                    handle: device,
                    report_size: hid_report_size(device_info),
                    usb_timeout: DEFAULT_USB_TIMEOUT,
                })
            }
            _ => {
                // Return NotFound if this VID:PID was not a valid CMSIS-DAP probe,
                // or if it couldn't be opened, so that other probe modules can
                // attempt to open it instead.
                Err(ProbeCreationError::NotFound)
            }
        }
    }
}

/// We recognise cmsis dap interfaces if they have string like "CMSIS-DAP"
/// in them. As devices spell CMSIS DAP differently we go through known
/// spellings/patterns looking for a match
fn is_cmsis_dap(id: &str) -> bool {
    id.contains("CMSIS-DAP") || id.contains("CMSIS_DAP")
}

/// Some devices don't have a CMSIS-DAP interface string, but are still
/// CMSIS-DAP probes. We hardcode a list of known VID/PID pairs here.
fn is_known_cmsis_dap_dev(device: &DeviceInfo) -> bool {
    // - 1a86:8012 WCH-Link in DAP mode, This shares the same description string as the
    //   WCH-Link in RV mode, so we have to check by vendor ID and product ID.
    const KNOWN_DAPS: &[(u16, u16)] = &[(0x1a86, 0x8012)];

    KNOWN_DAPS
        .iter()
        .any(|&(vid, pid)| device.vendor_id() == vid && device.product_id() == pid)
}

/// Manual override of HID report size
///
/// This is only needed for devices which:
///
/// 1. Don't use the default 64 bytes, and
/// 2. Don't respond to being asked the report size until they've received a full packet,
///    causing a long delay at startup as many 64-byte packets must be sent with a timeout
///    between each one.
///
/// Devices not on this list will still work but with a slow startup time
/// as the packet size is auto-determined.
#[cfg(feature = "cmsisdap_v1")]
fn hid_report_size(device: &hidapi::DeviceInfo) -> usize {
    // EDBG are 512-bytes and don't respond until you give them 512 bytes.
    if device.vendor_id() == 0x03eb
        && let Some(s) = device.product_string()
        && s.contains("EDBG")
    {
        tracing::debug!("Overriding packet size to 512 bytes for EDBG device");
        return 512;
    }

    // Default for almost all CMSIS-DAPv1 devices.
    // Devices are queried at startup for their packet size so it can usually be increased quickly
    // if needed; only some devices are annoying to query the packet size so are worth an override.
    64
}
