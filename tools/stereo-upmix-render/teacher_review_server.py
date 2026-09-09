#!/usr/bin/env python3
"""Loopback-only audio review API. Explicit media allowlist, local annotations."""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
import os
from pathlib import Path
import threading
import time
from urllib.parse import unquote, urlparse
import numpy as np
import soundfile as sf
from teacher_audit_policy import triage

ROOT=Path(__file__).resolve().parents[2]
LABELS={'mix':'Оригинал','vocals':'Весь вокал · RoFormer','instrumental':'Инструменты · RoFormer',
 'lead':'Лид-вокал · оценка','backing':'Бэк-вокал / вокальный остаток',
 'drums':'Ударные · Demucs','bass':'Бас · Demucs','guitar':'Гитара · Demucs',
 'piano':'Пиано · Demucs','other':'Прочее · Demucs','demucs_vocals':'Вокал · Demucs',
 'hh':'Хай-хэт · DrumSep','kick':'Бочка · DrumSep','snare':'Малый · DrumSep',
 'toms':'Томы · DrumSep','ride':'Райд · DrumSep','crash':'Крэш · DrumSep',
 'drum_residual':'Остаток разделения ударных',
 'lead_from_mix':'Лид · karaoke от исходной песни',
 'vocal_residual_from_mix':'Вокальный остаток · karaoke от исходной песни'}

def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

class ReviewStore:
    def __init__(self, root=ROOT):
        self.root=root;self.lock=threading.Lock()
        self.base=root/'build/reference-audio'
        self.notes=self.base/'teacher-review/annotations.jsonl'
        self.notes.parent.mkdir(parents=True,exist_ok=True)
        self.registry={}

    def catalog(self):
        items=[];registry={}
        manifests=[('v13',self.base/'v13-training-source/manifest.json',self.base/'v13-expanded-teacher'),
                   ('full',self.base/'v14-full-source/manifest.json',self.base/'v14-full-teacher'),
                   ('float',self.base/'v15-full-float-source/manifest.json',self.base/'v15-full-float-teacher')]
        counts={}
        for collection,manifest,teacher in manifests:
            if not manifest.exists():continue
            source=json.loads(manifest.read_text());counts[collection]=len(source['items'])
            for row in source['items']:
                ident=row['id'];p=teacher/ident/'teacher-provenance.json'
                if not p.exists():continue
                record=json.loads(p.read_text());paths={'mix':Path(row['stereo'])}
                hashes={'mix':record['identity']['source_sha256']}
                for name,h in record['output_sha256'].items():
                    key='vocals' if '_(Vocals)_' in name else 'instrumental'
                    paths[key]=p.parent/name;hashes[key]=h
                specialist=self.base/'v14-specialist-audit'/ident/'audit.json'
                metrics=None;flags=[]
                if specialist.exists():
                    extra=json.loads(specialist.read_text())
                    if extra['source_sha256']==hashes['mix']:
                        metrics=extra['qa']
                        flags=triage(extra)
                        for key,stem in extra['stems'].items():
                            paths[key]=Path(stem['path']);hashes[key]=stem['sha256']
                retained=self.base/'v15-complete-audit/retained'/ident/'retention.json'
                if collection=='float' and retained.exists():
                    extra=json.loads(retained.read_text())
                    if extra['source_sha256']==hashes['mix']:
                        metrics=extra['qa'];flags=['Оба входа karaoke проверены аудиомоделями; музыкальная роль и чистота не подтверждены автоматически.']
                        for name,stem in extra['stems'].items():
                            paths[name]=Path(stem['path']);hashes[name]=stem['sha256']
                if not all(x.is_file() for x in paths.values()):continue
                if collection!='float':flags.append('Архивный PCM16-набор: возможны срезанные пики при декодировании. Новое обучение использует отдельный FLOAT-набор.')
                revision=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()[:20]
                key=collection+'--'+ident
                registry[key]=dict(paths=paths,hashes=hashes,revision=revision,seconds=row['frames']/48000)
                items.append(dict(id=key,title=row['title']+(' · FLOAT' if collection=='float' else ''),artist=row.get('artist','unknown'),
                    collection=collection,partition=row.get('partition','development'),
                    seconds=row['frames']/48000,start=row.get('core_start_seconds',row.get('excerpt_start_seconds',0)),
                    core_offset=row.get('core_offset_frames',0)/48000,
                    core_seconds=row.get('core_frames',row['frames'])/48000,
                    revision=revision,specialist=metrics is not None,qa=metrics,flags=flags,
                    stems=[dict(id=k,label=LABELS.get(k,k)) for k in paths]))
        items.sort(key=lambda r:(not r['specialist'],-len(r['flags']),r['artist'],r['title']))
        with self.lock:self.registry=registry
        annotations=[]
        with self.lock:
            if self.notes.exists():
                for line in self.notes.read_text().splitlines():
                    if line:annotations.append(json.loads(line))
        pipeline=self.base/'v15-complete-audit/status.json'
        return dict(items=items,counts=counts,annotations=annotations,
            pipeline=json.loads(pipeline.read_text()) if pipeline.exists() else None,
            full_source_ready=(self.base/'v14-full-source/manifest.json').exists(),
            training_policy='Все новые данные на карантине до проверки; heldout не обучается.')

    def media(self,key,stem,revision):
        with self.lock:entry=self.registry.get(key)
        if not entry or stem not in entry['paths']:raise ValueError('unknown audio')
        if entry['revision']!=revision:raise ValueError('results changed; refresh catalog')
        directory=self.base/'teacher-review/preview'/revision
        path=directory/(stem+'.wav')
        with self.lock:
            for name,source in entry['paths'].items():
                if digest(source)!=entry['hashes'][name]:raise ValueError('audio fingerprint mismatch')
            if not path.exists():
                arrays={}
                for name,source in entry['paths'].items():
                    x,rate=sf.read(source,dtype='float32',always_2d=True)
                    if rate!=48000 or x.shape[1]!=2 or not np.isfinite(x).all():raise ValueError('invalid audio')
                    arrays[name]=x
                if len({a.shape for a in arrays.values()})!=1:raise ValueError('unaligned stems')
                # Enough headroom for original-minus-stem. No per-stem leveling.
                gain=min(1.,.40/max(max(float(np.max(abs(x))) for x in arrays.values()),1e-12))
                directory.mkdir(parents=True,exist_ok=True)
                # Store only the requested preview; same gain still uses all stems.
                sf.write(path,arrays[stem]*gain,48000,subtype='PCM_16')
                (directory/'gain.json').write_text(json.dumps(dict(shared_gain=gain)))
        return path.read_bytes()

    def annotate(self,value):
        if not isinstance(value,dict):raise ValueError('annotation must be an object')
        allowed={'id','stem','revision','decision','notes','position'}
        if set(value)-allowed:raise ValueError('unexpected annotation field')
        with self.lock:
            entry=self.registry.get(value.get('id'))
            if (not entry or value.get('stem') not in entry['paths'] or value['stem']=='mix'
                    or value.get('revision')!=entry['revision']):raise ValueError('unknown or stale stem')
            if value.get('decision') not in ['good','unsure','bad','absent']:raise ValueError('invalid decision')
            if not isinstance(value.get('notes',''),str) or len(value.get('notes',''))>2000:raise ValueError('notes too long')
            position=value.get('position',0)
            if not isinstance(position,(float,int)) or not np.isfinite(position) or not 0<=position<=entry['seconds']:raise ValueError('invalid position')
            for name in ('mix',value['stem']):
                if digest(entry['paths'][name])!=entry['hashes'][name]:raise ValueError('audio fingerprint mismatch')
            record=dict(value,time=time.time(),stem_sha256=entry['hashes'][value['stem']],
                source_sha256=entry['hashes']['mix'],reviewer='human-local',training_approved=False)
            with self.notes.open('a') as f:
                f.write(json.dumps(record,ensure_ascii=False)+'\n');f.flush();os.fsync(f.fileno())
            return record

def handler(store,port,ui_port):
    origins={f'http://127.0.0.1:{ui_port}',f'http://localhost:{ui_port}'}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def valid_host(self):return self.headers.get('Host') in {f'127.0.0.1:{port}',f'localhost:{port}'}
        def reply(self,status,data,kind='application/json'):
            raw=data if isinstance(data,bytes) else json.dumps(data,ensure_ascii=False).encode()
            self.send_response(status);self.send_header('Content-Type',kind)
            self.send_header('Content-Length',str(len(raw)));self.send_header('Cache-Control','no-store')
            self.send_header('X-Content-Type-Options','nosniff')
            origin=self.headers.get('Origin')
            if origin in origins:self.send_header('Access-Control-Allow-Origin',origin)
            self.end_headers();self.wfile.write(raw)
        def do_OPTIONS(self):
            if not self.valid_host() or self.headers.get('Origin') not in origins:
                self.reply(403,{'error':'local origin required'});return
            self.send_response(204);self.send_header('Access-Control-Allow-Origin',self.headers['Origin'])
            self.send_header('Access-Control-Allow-Methods','GET, POST, OPTIONS')
            self.send_header('Access-Control-Allow-Headers','Content-Type');self.end_headers()
        def do_GET(self):
            if not self.valid_host():self.reply(403,{'error':'local host required'});return
            if self.headers.get('Origin') and self.headers['Origin'] not in origins:
                self.reply(403,{'error':'local origin required'});return
            try:
                path=unquote(urlparse(self.path).path)
                if path=='/api/catalog':self.reply(200,store.catalog())
                elif path=='/api/pipeline':
                    status=store.base/'v15-complete-audit/status.json'
                    self.reply(200,json.loads(status.read_text()) if status.exists() else {'state':'not-started'})
                elif path.startswith('/media/'):
                    _,_,key,revision,stem=path.split('/')
                    self.reply(200,store.media(key,stem,revision),'audio/wav')
                else:self.reply(404,{'error':'not found'})
            except (ValueError,KeyError,OSError) as exc:self.reply(400,{'error':str(exc)})
        def do_POST(self):
            if (not self.valid_host() or self.headers.get('Origin') not in origins
                    or self.path!='/api/annotations' or self.headers.get('Content-Type')!='application/json'):
                self.reply(403,{'error':'local JSON request required'});return
            try:
                length=int(self.headers.get('Content-Length','0'))
                if not 0<length<=16384:raise ValueError('invalid body size')
                self.reply(200,store.annotate(json.loads(self.rfile.read(length))))
            except (ValueError,KeyError,TypeError) as exc:self.reply(400,{'error':str(exc)})
    return Handler

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',type=int,default=8766)
    p.add_argument('--ui-port',type=int,default=5178);a=p.parse_args()
    store=ReviewStore();store.catalog()
    server=ThreadingHTTPServer(('127.0.0.1',a.port),handler(store,a.port,a.ui_port))
    print(f'Local review API http://127.0.0.1:{a.port}',flush=True);server.serve_forever()

if __name__=='__main__':main()
