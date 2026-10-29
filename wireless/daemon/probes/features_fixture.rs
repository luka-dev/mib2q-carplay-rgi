//! Independent phone/HMI peripherals for the complete-features QNX profile.
use iap2_link::{LinkEngine,CONTROL_SESSION_ID,FILE_TRANSFER_SESSION_ID};
use std::io::{self,Read,Write};
use std::net::{TcpListener,Ipv4Addr};
use std::time::{Duration,Instant};
pub fn enabled()->bool {matches!(std::env::var("MHI2Q_INTEGRATED_FAULT").as_deref(),Ok("features"|"rfcomm-gap"))}
fn frame(id:u16,fields:&[(u16,&[u8])])->Vec<u8> {
    let mut p=vec![0x40,0x40,0,0];p.extend(id.to_be_bytes());
    for (id,value) in fields {p.extend(((value.len()+4)as u16).to_be_bytes());p.extend(id.to_be_bytes());p.extend_from_slice(value);}
    let len=p.len()as u16;p[2..4].copy_from_slice(&len.to_be_bytes());p
}
pub fn artwork(engine:&mut LinkEngine,now:u64) {
    let jpeg=include_bytes!("../../../tests/fixtures/coverart/exif-thumbnail.jpg");
    let mut setup=vec![129,4];setup.extend((jpeg.len()as u64).to_be_bytes());setup.extend([0,2]);
    engine.send(FILE_TRANSFER_SESSION_ID,setup,now);
}
pub fn file(engine:&mut LinkEngine,data:&[u8],now:u64)->io::Result<()> {
    if !enabled(){return Ok(());}
    match data {
        [129,1]=> {
            let jpeg=include_bytes!("../../../tests/fixtures/coverart/exif-thumbnail.jpg");
            let mut bytes=vec![129,0xc0];bytes.extend(jpeg);engine.send(FILE_TRANSFER_SESSION_ID,bytes,now);
        },
        [129,5]=>println!("[integrated-features] real artwork file Start/Data/Success handshake PASS"),
        _=>return Err(io::Error::other("unexpected artwork file reply")),
    }
    Ok(())
}
pub fn control(engine:&mut LinkEngine,id:u16,data:&[u8],now:u64)->io::Result<bool> {
    if !enabled(){
        // Other fault profiles model a phone with no active navigation.
        // A valid accessory subscription is still part of its protocol.
        if id==0x5200 {
            if data!=frame(0x5200,&[(0,&16u16.to_be_bytes()),(1,&[]),(2,&[]),(3,&[])]) {
                return Err(io::Error::other("RGI subscription component differs"));
            }
            return Ok(true);
        }
        return Ok(false);
    }
    match id {
        0x5200=> {
            if data!=frame(0x5200,&[(0,&16u16.to_be_bytes()),(1,&[]),(2,&[]),(3,&[])]) {return Err(io::Error::other("RGI subscription component differs"));}
            let reattached=std::env::var("MHI2Q_INTEGRATED_FAULT").as_deref()==Ok("rfcomm-gap")&&
                std::path::Path::new("/dev/shmem/integrated-rfcomm-gap").exists();
            let road=if reattached{&b"QNX Reattached Road\0"[..]}else{&b"QNX Road\0"[..]};
            engine.send(CONTROL_SESSION_ID,frame(0x5201,&[(0,&[0,16]),(1,&[1]),(3,road),(4,b"QNX Destination\0")]),now);
            engine.send(CONTROL_SESSION_ID,frame(0x5202,&[(0,&[0,16]),(1,&[0,0]),(3,&[0,2]),(4,b"QNX Next Road\0")]),now);
            let lane=frame(0,&[(0,&[0,0]),(1,&[1]),(2,&[0,0]),(3,&[0,0])]);
            engine.send(CONTROL_SESSION_ID,frame(0x5204,&[(0,&[0,16]),(1,&[0,0]),(2,&lane[6..]),(3,b"QNX Lane\0")]),now);
            engine.send(CONTROL_SESSION_ID,frame(0xfffa,&[(1,&[]),(2,&[]),(4,&[])]),now);
            engine.send(CONTROL_SESSION_ID,frame(0xa100,&[(3,&[]),(4,&[]),(6,&[])]),now);
            println!("[integrated-features] native RGI subscription and phone route/maneuver response PASS");
        },
        0xa101=> {
            if data!=frame(0xa101,&[(3,&[1,144]),(4,&[0,22]),(6,&[1])]) {return Err(io::Error::other("vehicle telemetry differs from HMI peripheral"));}
            let context=std::fs::read_to_string("/dev/shmem/integrated-context")?;
            let session=context.split_whitespace().nth(1).ok_or_else(||io::Error::other("missing GPS session"))?;
            std::fs::write(format!("/dev/shmem/integrated-gps-{session}"),b"native queue\n")?;
            println!("[integrated-features] phone received native vehicle range/temperature/warning PASS");
        },
        0xfffb=> {
            if data.len()<11||data[6..10]!=((data.len()-6)as u16).to_be_bytes().into_iter().chain([0,0]).collect::<Vec<_>>()[..] {
                return Err(io::Error::other("invalid location TLV"));
            }
            let sentence=std::str::from_utf8(&data[10..data.len()-1]).map_err(io::Error::other)?;
            let Some((body,checksum))=sentence.strip_prefix('$').and_then(|s|s.rsplit_once('*')) else{return Err(io::Error::other("invalid NMEA"));};
            let expected=body.bytes().fold(0u8,|a,b|a^b);
            if u8::from_str_radix(checksum,16).ok()!=Some(expected) {return Err(io::Error::other("bad native NMEA checksum"));}
            if sentence.starts_with("$GPGGA,") {println!("[integrated-features] phone received vehicle GPS GGA from native DIO queue PASS");}
            if sentence.starts_with("$GPRMC,") {println!("[integrated-features] phone received vehicle GPS RMC from native DIO queue PASS");}
            if sentence.starts_with("$PASCD,") {println!("[integrated-features] phone received subscribed wheel speed PASS");}
        },
        _=>return Ok(false),
    }
    Ok(true)
}
pub fn bus()->io::Result<()> {
    if !enabled(){return Ok(());}
    let listener=TcpListener::bind((Ipv4Addr::LOCALHOST,19810))?;listener.set_nonblocking(true)?;
    println!("[integrated-features] Java bus protocol peer listening");
    std::thread::spawn(move|| {
        let end=Instant::now()+Duration::from_secs(240);
        while Instant::now()<end {
            let (mut stream,_)=match listener.accept(){Ok(v)=>v,Err(e)if e.kind()==io::ErrorKind::WouldBlock=>{std::thread::sleep(Duration::from_millis(20));continue;},Err(_)=>return};
            if stream.set_nonblocking(false).is_err(){return;}
            println!("[integrated-features] native hook connected to Java bus protocol peer");
            if stream.set_read_timeout(Some(Duration::from_millis(100))).is_err(){return;}
            if stream.set_write_timeout(Some(Duration::from_millis(100))).is_err(){return;}
            let mut sequence=0u32;let mut sent=Instant::now()-Duration::from_secs(1);
            let mut buffered=Vec::new();
            let mut route=false;let mut art=false;let mut lane=false;let mut reattached=false;
            while Instant::now()<end {
                if sent.elapsed()>=Duration::from_millis(100) {
                    sequence+=1;let seq=sequence;
                    let mut command=Vec::from(*b"CPHB");command.extend(seq.to_be_bytes());command.extend([1,24,2,0,0,0,0,14]);
                    command.extend([1,31,232,3,1,144,1,215,0,1,0,0,0,0]);
                    if let Err(error)=stream.write_all(&command){println!("[integrated-features] bus HMI write ended: {error}");break;}
                    sent=Instant::now();
                }
                let mut input=[0;4096];
                match stream.read(&mut input) {
                    Ok(0)=>break,Ok(size)=>buffered.extend_from_slice(&input[..size]),
                    Err(error)if matches!(error.kind(),io::ErrorKind::WouldBlock|io::ErrorKind::TimedOut)=>continue,
                    Err(error)=>{println!("[integrated-features] bus receive ended: {error}");break;}
                }
                if buffered.len()>128*1024+4096+16{return;}
                while buffered.len()>=16 {
                let header=&buffered[..16];
                let length=u32::from_be_bytes(header[12..16].try_into().unwrap())as usize;
                if &header[..4]!=b"CPHB"||header[11]!=0||length>128*1024{println!("[integrated-features] invalid bus header {header:02x?}");return;}
                if buffered.len()<16+length {break;}
                let kind=u16::from_be_bytes(header[8..10].try_into().unwrap());
                if !route {println!("[integrated-features] bus type={kind:x} bytes={length}");}
                let payload=&buffered[16..16+length];
                let text=String::from_utf8_lossy(&payload);
                if kind==0x20&&!route&&text.contains("QNX Road") {route=true;println!("[integrated-features] native RGI reached Java bus protocol peer PASS");}
                if kind==0x20&&!reattached&&text.contains("QNX Reattached Road") {
                    if std::fs::write("/dev/shmem/integrated-semantic-reattached",b"native RGI\n").is_err(){return;}
                    reattached=true;println!("[integrated-media] native iAP2 features resumed on reauthenticated RFCOMM PASS");
                }
                if kind==0x20&&!lane&&text.contains("lg0_lane_count:n:1")&&text.contains("QNX Lane") {
                    lane=true;println!("[integrated-features] native lane guidance reached Java bus protocol peer PASS");
                }
                if kind==0x10&&!art&&text.contains("path:s:/var/app/icab/tmp/37/coverart.png") {
                    let image=std::fs::read("/var/app/icab/tmp/37/coverart.png").unwrap_or_default();
                    if image.starts_with(b"\x89PNG\r\n\x1a\n")&&image.get(16..24)==Some(&[0,0,1,0,0,0,1,0]) {
                        art=true;println!("[integrated-features] artwork decoded 256x256 PNG and published over Java bus PASS");
                    }
                }
                buffered.drain(..16+length);
                }
            }
        }
    });Ok(())
}
