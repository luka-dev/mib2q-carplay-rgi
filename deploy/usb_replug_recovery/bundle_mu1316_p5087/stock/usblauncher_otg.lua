--[[
-- usblauncher replaces: mcd for mounting filesystems on USB devices;
-- enum-devices and enum-usb for starting USB drivers;
-- usbpub for publishing device, driver, and mount PPS objects
os.execute'slay mcd'
os.execute'slay enum-devices'
os.execute'slay enum-usb'
os.execute'slay usbpub'
--]]

------------------------------------------------------------------------------

    USB_CLASS_DEVICE = 0x00
    USB_CLASS_AUDIO = 0x01
        USB_AUDIO_PROTOCOL_CONTROL = 0x01
        USB_AUDIO_PROTOCOL_STREAMING = 0x02
        USB_AUDIO_PROTOCOL_MIDI_STREAMING = 0x03
    USB_CLASS_COMMS = 0x02
        USB_COMMS_SUBCLASS_DIRECT_LINE = 0x01
        USB_COMMS_SUBCLASS_MODEM = 0x02
        USB_COMMS_SUBCLASS_TELEPHONE = 0x03
        USB_COMMS_SUBCLASS_MULTICHANNEL = 0x04
        USB_COMMS_SUBCLASS_CAPI_CONTROL = 0x05
        USB_COMMS_SUBCLASS_ETHERNET = 0x06
        USB_COMMS_SUBCLASS_ATM = 0x07
        USB_COMMS_SUBCLASS_WIRELESS_HANDSET = 0x08
        USB_COMMS_SUBCLASS_DEVICE_MGMT = 0x09
        USB_COMMS_SUBCLASS_MOBILE_DIRECT_LINE = 0x0a
        USB_COMMS_SUBCLASS_OBEX = 0x0b
        USB_COMMS_SUBCLASS_NCM  = 0x0d
        USB_COMMS_SUBCLASS_ETHERNET_EMULATION = 0x0c
            USB_COMMS_PROTOCOL_ETHERNET_EMULATION = 0x07
    USB_CLASS_HID = 0x03
        USB_HID_SUBCLASS_NONE = 0x00
            USB_HID_PROTOCOL_NONE = 0x00
            USB_HID_PROTOCOL_KEYBOARD = 0x01
            USB_HID_PROTOCOL_MOUSE = 0x02
        USB_HID_SUBCLASS_BOOTINTERFACE = 0x01
    USB_CLASS_PHYSICAL = 0x05
    USB_CLASS_IMAGING = 0x06
        USB_IMAGING_SUBCLASS_STILL = 0x01
            USB_IMAGING_STILL_PROTOCOL_PTP = 0x01
    USB_CLASS_PRINTER = 0x07
    USB_CLASS_MASS_STORAGE = 0x08
    USB_CLASS_HUB = 0x09
        USB_HUB_PROTOCOL_FULL = 0x00
        USB_HUB_PROTOCOL_HI_SINGLE_TT = 0x01
        USB_HUB_PROTOCOL_HI_MULTIPLE_TT = 0x02
    USB_CLASS_CDC_DATA = 0x0a
    USB_CLASS_SMART_CARD = 0x0b
    USB_CLASS_CONTENT_SECURITY = 0x0d
    USB_CLASS_VIDEO = 0x0e
    USB_CLASS_DIAGNOSTIC = 0xdc
    USB_CLASS_WIRELESS = 0xe0
        USB_WIRELESS_SUBCLASS_RADIO = 0x01
            USB_WIRELESS_PROTOCOL_BLUETOOTH_PI = 0x01
            USB_WIRELESS_PROTOCOL_UWB_RCI = 0x02
            USB_WIRELESS_PROTOCOL_RNDIS = 0x03
        USB_WIRELESS_SUBCLASS_ADAPTER = 0x02
            USB_WIRELESS_PROTOCOL_HOST_WIRE = 0x01
            USB_WIRELESS_PROTOCOL_DEVICE_WIRE = 0x02
            USB_WIRELESS_PROTOCOL_DEVICE_WIRE_ISOCHRONOUS = 0x03
    USB_CLASS_MISC = 0xef
    USB_CLASS_APPSPEC = 0xef
        USB_APPSPEC_SUBCLASS_FIRMWARE_UPDATE = 0x01
            USB_APPSPEC_PROTOCOL_FIRMWARE_UPDATE = 0x01
        USB_APPSPEC_SUBCLASS_IRDA_BRIDGE = 0x02
            USB_APPSPEC_PROTOCOL_IRDA_BRIDGE = 0x00
        USB_APPSPEC_SUBCLASS_TEST_MEASURE = 0x03
            USB_APPSPEC_PROTOCOL_TMC = 0x00
            USB_APPSPEC_PROTOCOL_USB488 = 0x01
    USB_CLASS_VENDOR_SPECIFIC = 0xff
    
    USB_PATH = "/dev/io-usb/otg"
    
    DRVRLIM_LM_DELAY = 5000

------------------------------------------------------------------------------
stat = posix.stat
if stat("/mnt/app/iousbVerbosity-vv_enabled") then
     host_stack_cmd = 'io-usb -vv -d compiled-ehci-arc-msm8960 ioport=0x12530000,irq=247,verbose=0,cache=1';
elseif stat("/mnt/app/iousbVerbosity-vvvvvv_enabled") then
     host_stack_cmd = 'io-usb -vvvvvv -d compiled-ehci-arc-msm8960 ioport=0x12530000,irq=247,verbose=0,cache=1';
else
     host_stack_cmd = 'io-usb -d compiled-ehci-arc-msm8960 ioport=0x12530000,irq=247,verbose=0,cache=1';
end

-- #[d]: Specifies the host stack on OTG port
Host_Stack = {
        cmd = host_stack_cmd;
        path = '/dev/io-usb/otg';
}

dofile '/etc/usblauncher_carplay_descriptor.lua'
dofile '/etc/usblauncher_carlife_descriptor.lua'

-- Don't need RNDIS descriptor to override, since the devn rndis driver does a better job of loading the descriptor 
-- For rndis role reversal the usbrolereverse issues start_stack command with out of bounds descriptor index '0' ignoring
-- the descriptors list
dofile '/etc/raw_desc_usb_rndis.lua'

-- #[d]: Specifies the device stack on OTG port
Device_Stack = {
    cmd = '/etc/scripts/start_usb_device_stack.sh';
    path = '/dev/io-usb/otg_dev';
    connect_poll = 100; --poll 100ms in order to improve CarPlay connection times
    -- Two times the same device descriptor; first one is used by "start_stack::device,1"; second is used by "role_swap"
    descriptors = {usbdesc_carlife, usbdesc_carplay, rndis};
}

------------------------------------------------------------------------------

-- All devices included within ski_devices are never touched
--skip_devices = {
--}

-- foreach (skip_devices) {
--     Ignore;  -- don't even attach to these devices
--}

-- basic options for devb-umass
DISK_OPTS  = "cam quiet blk devdir=/dev/umass,cache=2m,vnode=384,auto=none,delwri=2:2,noatime disk name=umass cdrom name=umasscd";
UMASS_OPTS = "umass mpoll=2000:250:40,priority=21,path=$(USB_PATH)";

-- table of specific MSC devices that can't handle Microsoft descriptor queries
-- By specifying the vid/did, the rule is tried before any generic rules
known_msc = {
	device(0x0781, 0x5567);	-- SanDisk Cruzer Blade
        device(0x0951, 0x168e); -- Kingston Traveler USB-Stick 3.0
}

-- Intenso Ultra Line 
device(0x090c, 0x1000) { 
	class(USB_CLASS_MASS_STORAGE) { 
		driver"devb-umass cam quiet blk devdir=/dev/umass,cache=2m,vnode=384,auto=none,delwri=2:2,noatime disk name=umass,maxio=8 cdrom name=umasscd $(UMASS_OPTS),vid=$(vendor_id),did=$(product_id),busno=$(busno),devno=$(devno),iface=$(iface),ign_remove"; 
	}; 
}; 

foreach (known_msc) {
    driver"devb-umass $(DISK_OPTS) $(UMASS_OPTS),vid=$(vendor_id),did=$(product_id),busno=$(busno),devno=$(devno),iface=$(iface),ign_remove";
}

-- GAL blacklist, those device will be skipped
--[[
AOA = { 
    blacklist = { 
        { vid = 0x05ac; };  -- Any Apple device 
        { vid = 0x0781; did = 0x5530 };  -- SanDisk Cruzer  
    }; 
}
--]]

-- Huawei ME909Tu/Au-120
device(0x12d1, 0x1573) {
    configuration(2) {
                class(0x02) {
                    driver"mount -Tio-pkt -obusnum=$(busno),devnum=$(devno) /lib/dll/devnp-ecm.so";
                };
                 class(0xff) {
                    driver"/etc/scripts/huawei.sh vid=$(vendor_id),did=$(product_id),busno=$(busno),devno=$(devno)";
                };
       };
};

-- artf343343
-- Sony Experia Z1 in MSC mode is not handled generically as devb-umass is needed of 3 seconds delay approximately
device(0x0fce, 0xe19e) {
    class(USB_CLASS_MASS_STORAGE) {
        driver"devb-umass $(DISK_OPTS) $(UMASS_OPTS),vid=$(vendor_id),did=$(product_id),busno=$(busno),devno=$(devno),iface=$(iface),ign_remove,rsettle=3000";
    };
};

device(0x0fce, 0xe1a7) {
    class(USB_CLASS_MASS_STORAGE) {
        driver"devb-umass $(DISK_OPTS) $(UMASS_OPTS),vid=$(vendor_id),did=$(product_id),busno=$(busno),devno=$(devno),iface=$(iface),ign_remove,rsettle=3000";
    };
};

--artf364551
device(0x0fce, 0xe1ba) {
    class(USB_CLASS_MASS_STORAGE) {
        driver"devb-umass $(DISK_OPTS) $(UMASS_OPTS),vid=$(vendor_id),did=$(product_id),busno=$(busno),devno=$(devno),iface=$(iface),ign_remove,rsettle=5000";
    };
};

-- Cowan J3 in MTP mode is overridden
device(0x0e21, 0x0921) {
    ms_desc("MTP") {
        driver"devb-umass $(DISK_OPTS) $(UMASS_OPTS),vid=$(vendor_id),did=$(product_id),busno=$(busno),devno=$(devno),iface=$(iface),ign_remove";
    };
};

-- Cowon X7 in MTP mode is overridden (Bugfix for artf151889)
device(0x0e21, 0x0931) {
    ms_desc("MTP") {
        driver"devb-umass $(DISK_OPTS) $(UMASS_OPTS),vid=$(vendor_id),did=$(product_id),busno=$(busno),devno=$(devno),iface=$(iface),ign_remove";
    };
};

-- artf363098
device(0x18a5, 0x0243){ -- Verbatim Store-N-Go
	class(USB_CLASS_MASS_STORAGE){
		driver"devb-umass $(DISK_OPTS) umass mpoll=10:250:40,priority=21,path=$(USB_PATH),vid=$(vendor_id),did=$(product_id),busno=$(busno),devno=$(devno),iface=$(iface),ign_remove,maxbresets=50"
	};
};

-- Exclude iOS devices in case USB_CLASS_MASS_STORAGE, otherwise devb-umass is started
product(0x05AC, 0x1200, 0x12FF) {
    -- Start a fake driver for Apple device, this is needed to let usblauncher execute role_swap command as required for Gen20 media connectors 
    driver"echo dummy iOS driver";
};

-- generic mass storage rule
device() {
  class(USB_CLASS_MASS_STORAGE) {
        driver"devb-umass $(DISK_OPTS) $(UMASS_OPTS),vid=$(vendor_id),did=$(product_id),busno=$(busno),devno=$(devno),iface=$(iface),ign_remove";
    };
};

-- Rule needed for Harman during production for testing, rule provided by QC directly for integration as part of ES-5
device(0x05c6,0xf00e) {
    start"/armle/scripts/usbdc_rndis_start.sh";
    removal"/armle/scripts/usbdc_rndis_stop.sh";
}

-- # [d] Ipod service: the process that will dynamically load shared libraries that match to certain interfaces: the one for Authentication, iap2 driver, etc.
device(USB_VENDOR_ID, USB_PRODUCT_ID) {
-- For running over Evaluation-Board set the Authentication chip address to 0x10    
  class(0xFF, 0xF0, 0x00) {
      func_driver = function(obj)
          print(string.format("[usblauncher_otg.lua] %.4x/%.4x driver", USB_VENDOR_ID, USB_PRODUCT_ID))
          return "/armle/usr/sbin/io-otg-cinemo -s /dev/io-usb/otg_dev -lc 212 -v -t 8"
      end;    
      driver"$(func_driver)";    
      -- driver"/armle/usr/sbin/io-otg-cinemo -s /dev/io-usb/otg_dev -lc 212 -v";
  };            
}; 

-- EAP driver activation 
device(USB_VENDOR_ID,  USB_PRODUCT_ID) {   
    class(0xFF, 0xF0, 0x01) { 
        func_driver = function(obj)
            if posix.stat("/tmp/carlife_active") then
                print(string.format("[usblauncher_otg.lua] %.4x/%.4x ea native driver", USB_VENDOR_ID, USB_PRODUCT_ID))
                return ("/mnt/app/carlife/iap_nativeea -i $(interface)")
            end
        end;
      driver"$(func_driver)";
  }; 
}; 

-- #[d]: NCM driver activation / deactivation scripts
device(USB_VENDOR_ID,  USB_PRODUCT_ID) {    
    class(USB_CLASS_COMMS, USB_COMMS_SUBCLASS_NCM, 0x00) {
        func_start = function(obj)
            if not posix.stat("/tmp/carlife_active") then
                local interface = "carplay"
                local iopkt_instance = "io-pkt"
                print(string.format("[usblauncher_otg.lua] %.4x/%.4x start", USB_VENDOR_ID, USB_PRODUCT_ID))
                return ("/etc/scripts/startncm.sh " .. Device_Stack.path .. " " .. interface .. " " .. iopkt_instance)
            end
      end;
      func_removal = function(obj)
          print(string.format("[usblauncher_otg.lua] %.4x/%.4x removal", USB_VENDOR_ID, USB_PRODUCT_ID))
          return "/etc/scripts/stopncm.sh"
      end;    
      start"$(func_start)";    
      removal"$(func_removal)";    
  };    
};

-- D-Link DUB-E100 USB Dongles
device(0x2001, 0x3c05) {
    driver"/etc/scripts/extnet.sh -oname=en,lan=0,busnum=$(busno),devnum=$(devno),phy_88772=0,phy_check,wait=60,speed=100,duplex=1,ign_remove,path=$(USB_PATH) /lib/dll/devnp-asix.so /dev/io-net/en0";
    removal"ifconfig en0 destroy";
};

device(0x2001, 0x1a02) {
    driver"/etc/scripts/extnet.sh -oname=en,lan=0,busnum=$(busno),devnum=$(devno),phy_88772=0,phy_check,wait=60,speed=100,duplex=1,ign_remove,path=$(USB_PATH) /lib/dll/devnp-asix.so /dev/io-net/en0";
    removal"ifconfig en0 destroy";
};

-- SMSC9500
device(0x0424, 0x9500) {
	-- the extnet.sh script does an exec dhcp.client at the bottom, then usblauncher can slay the dhcp.client when the dongle is removed
	driver"/etc/scripts/extnet.sh -olan=0,busnum=$(busno),devnum=$(devno),path=$(USB_PATH) /lib/dll/devn-smsc9500.so /dev/io-net/en0";
	removal"ifconfig en0 destroy";
};

-- Germaneers LAN9514
device(0x2721, 0xec00) {
        -- the extnet.sh script does an exec dhcp.client at the bottom, then usblauncher can slay the dhcp.client when the dongle is removed
        driver"/etc/scripts/extnet.sh -olan=0,busnum=$(busno),devnum=$(devno),path=$(USB_PATH) /lib/dll/devn-smsc9500.so /dev/io-net/en0";
        removal"ifconfig en0 destroy";
};

-- ASIX AX88178
device(0x0b95, 0x1780) {
    driver"/etc/scripts/extnet.sh -oname=en,lan=0,busnum=$(busno),devnum=$(devno),wait=60,ign_remove,path=$(USB_PATH) /lib/dll/devnp-asix.so /dev/io-net/en0";
    removal"ifconfig en0 destroy";
};

-- ASIX AX88772B
device(0x0b95, 0x772B) {
    driver"/etc/scripts/extnet.sh -oname=en,lan=0,busnum=$(busno),devnum=$(devno),wait=60,ign_remove,path=$(USB_PATH) /lib/dll/devnp-asix.so /dev/io-net/en0";
    removal"ifconfig en0 destroy";
};

-- ASIX AX88179
device(0x0b95, 0x1790) {
    driver"/etc/scripts/extnet.sh -oname=en,lan=0,busnum=$(busno),devnum=$(devno),wait=60,ign_remove,path=$(USB_PATH) /lib/dll/devnp-asix.so /dev/io-net/en0";
    removal"ifconfig en0 destroy";
};

-- Delock USB Serialadapter needed for hb_testmode
device(0x0403, 0x6001) {
    interface(0)
    {
	driver"devc-serusb -b115200 -e -F -S -d name=/dev/usbserialadapter,path=$(USB_PATH),vid=$(vendor_id),did=$(product_id),busno=$(busno),devno=$(devno),ign_remove";
    };
    -- Don't try matching any generic rules for the other interfaces
    interface(1,127) {
	Ignore; 
    };
};

-- USB certification test device
product(0x1A0A, 0x0101, 0x0108) {
    test_device_inserted = function(obj)
        if posix.stat("/tmp/usblauncherFlag_test_device_present") then
            return 'echo "[usblauncher_otg.lua] 0x1A0A usb certification already started"'
        else
            os.execute("touch /tmp/usblauncherFlag_test_device_present")
            local idAsString = string.format("%.4x", obj.product_id)
	    local portAsString = string.format("%u", obj.upstream_port)
            print("[usblauncher_otg.lua] 0x1A0A test device detected - Product=" .. idAsString)
            local returnValue = "/etc/scripts/usb_cert_start.sh " .. idAsString .. " " .. portAsString
            print("[usblauncher_otg.lua] start " .. returnValue)
            return returnValue
        end
    end;
    test_device_removed = function(obj)
        os.remove("/tmp/usblauncherFlag_test_device_present")
        return 'echo "[usblauncher_otg.lua] 0x1A0A test device removed"'
    end;
    driver"$(test_device_inserted)";
    removal"$(test_device_removed)";
};

------------------------------------------------------------------------------

if verbose >= 3 then
    table.dump(new_conf)
    table.dump(new_conf.flags)
    show_config()
end

