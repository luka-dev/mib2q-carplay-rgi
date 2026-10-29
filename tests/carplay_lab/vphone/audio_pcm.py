"""Read the bounded QSA peripheral tap and verify the real sender's two tones."""
import argparse
import hashlib
import json
import socket
import struct
import wave
from . import wireless_audio_lab as audio
from tests.carplay_lab.common import qnx_receiver

def receive(peer,length):
    data=bytearray()
    while len(data)<length:
        chunk=peer.recv(length-len(data))
        if not chunk:raise EOFError('incomplete native PCM response')
        data.extend(chunk)
    return bytes(data)

def verify(data,rate,channels,source_channels=2):
    import numpy as np
    from .audio_playback import FREQUENCIES
    if source_channels not in (2,6) or channels<source_channels:
        raise ValueError('output cannot prove the requested source channels')
    if rate not in (44100,48000) or channels not in (2,6,8) or len(data)%(2*channels) or len(data)<16384:
        raise ValueError('insufficient native PCM or unexpected geometry')
    pcm=np.frombuffer(data,dtype='<i2').reshape(-1,channels).astype(float)
    if len(pcm)<4096:raise ValueError('insufficient PCM frames for tone/dropout oracle')
    window=np.hanning(len(pcm));frequencies=np.fft.rfftfreq(len(pcm),1/rate)
    result=[]
    expected_frequencies=FREQUENCIES[:source_channels]
    for channel,expected in enumerate(expected_frequencies):
        samples=pcm[:,channel];spectrum=np.abs(np.fft.rfft(samples*window))
        spectrum[frequencies<40]=0
        peak=float(frequencies[int(np.argmax(spectrum))]);rms=float(np.sqrt(np.mean(samples*samples)))
        def energy(hz):return float(np.max(spectrum[np.abs(frequencies-hz)<8]))
        separation=energy(expected)/max(*(energy(hz) for hz in expected_frequencies if hz!=expected),1)
        # A spectral peak alone also accepts dropouts or repeated damaged
        # packets. Fit the known source frequency after one AAC startup AU;
        # discontinuities elsewhere must fail this independent PCM oracle.
        steady=samples[1024:];t=np.arange(len(steady))/rate
        basis=np.column_stack((np.sin(2*np.pi*expected*t),np.cos(2*np.pi*expected*t),np.ones(len(t))))
        fitted=basis@np.linalg.lstsq(basis,steady,rcond=None)[0]
        signal=float(np.mean(fitted*fitted));noise=float(np.mean((steady-fitted)**2))
        snr=float(10*np.log10(max(signal,1e-12)/max(noise,1e-12)))
        okay=abs(peak-expected)<=max(5,rate/len(pcm)) and 10<rms<20000 and separation>4 and snr>30
        result.append({'expected_hz':expected,'peak_hz':peak,'rms':rms,'separation_ratio':separation,
                       'steady_snr_db':snr,'excluded_startup_frames':1024,'passed':okay})
    return {'passed':all(row['passed'] for row in result),'frames':len(pcm),'source_channels':source_channels,'channels':result}

def run(spatial=False,source_channels=2):
    audio.configure(spatial);record=json.loads((audio.OUT/'build-record.json').read_text())
    if not qnx_receiver.running() or not record.get('audio_peripheral'):
        raise RuntimeError('owned explicit HMI/QSA peripheral profile is not running')
    with socket.create_connection(('127.0.0.1',record['input_host_port']),timeout=3) as peer:
        peer.settimeout(5);peer.sendall(b'MHA'+bytes(4));length,rate,channels=struct.unpack('>III',receive(peer,12))
        if length>65536:raise ValueError('native PCM capture exceeds its bound')
        data=receive(peer,length)
    result={'passed':False,'qnx_pid':qnx_receiver.running(),'rate':rate,'channels':channels,'bytes':length,
            'sha256':hashlib.sha256(data).hexdigest(),'ifs_sha256':record['output_sha256'],
            'real':['native vphone player (see playback receipt)','wireless sender (codec/transport require SETUP evidence)','native QSA PCM tap'],
            'modeled':['HMI ACK via native DSI','QSA playback hardware','receive-only MainHigh RTCP endpoint'],
            'rtcp_clock_retransmission_verified':False,'physical_audio_verified':False}
    try:
        result['tones']=verify(data,rate,channels,source_channels);result['passed']=result['tones']['passed']
        with wave.open(str(audio.OUT/'native-audio.wav'),'wb') as out:
            out.setnchannels(channels);out.setsampwidth(2);out.setframerate(rate);out.writeframes(data)
    except ValueError as error:result['error']=str(error)
    (audio.OUT/'audio-pcm.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
    if not result['passed']:raise RuntimeError('native PCM tone oracle failed')
    return result

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--spatial',action='store_true')
    parser.add_argument('--source-channels',type=int,choices=(2,6),default=2)
    args=parser.parse_args();run(args.spatial,args.source_channels)
