export const API = 'http://127.0.0.1:8766';
export type Mode = 'original' | 'solo' | 'minus';
export type Clip = {id:string;title:string;artist:string;collection:string;partition:string;seconds:number;start:number;core_offset:number;core_seconds:number;revision:string;specialist:boolean;flags:string[];qa:Record<string,unknown>|null;stems:{id:string;label:string}[]};

export class Listening {
  context?:AudioContext; cache=new Map<string,AudioBuffer>(); sources:AudioBufferSourceNode[]=[];
  gains:GainNode[]=[]; master?:GainNode; clip?:Clip; stem='vocals'; mode:Mode='original';
  playing=false; offset=0; started=0; volume=.5; loop=true; generation=0; pending=false;
  get position(){
    if(!this.playing||!this.context||!this.clip)return this.offset;
    const start=this.clip.core_offset, length=this.clip.core_seconds;
    const p=this.offset+Math.max(0,this.context.currentTime-this.started);
    return this.loop ? start+((p-start)%length+length)%length : Math.min(p,this.clip.seconds);
  }
  async buffer(clip:Clip,stem:string){
    const key=`${clip.id}/${clip.revision}/${stem}`;
    if(!this.cache.has(key)){
      const response=await fetch(`${API}/media/${key}`);
      if(!response.ok)throw new Error('Аудио изменилось или недоступно. Обнови список.');
      this.context??=new AudioContext();
      this.cache.set(key,await this.context.decodeAudioData(await response.arrayBuffer()));
      while(this.cache.size>12)this.cache.delete(this.cache.keys().next().value!);
    }
    return this.cache.get(key)!;
  }
  async select(clip:Clip,stem:string){
    this.pause();this.clip=clip;this.stem=stem;this.offset=clip.core_offset;
    await Promise.all([this.buffer(clip,'mix'),this.buffer(clip,stem)]);
  }
  async selectStem(stem:string){
    if(!this.clip)return;
    const wasPlaying=this.playing;const position=this.position;this.pause();this.stem=stem;
    await this.buffer(this.clip,stem);this.offset=position;
    if(wasPlaying)await this.play();
  }
  setMode(mode:Mode){this.mode=mode;this.applyGains();}
  applyGains(){
    if(!this.context)return;
    const values=this.mode==='original'?[1,0]:this.mode==='solo'?[0,1]:[1,-1];
    this.gains.forEach((g,i)=>g.gain.setTargetAtTime(values[i],this.context!.currentTime,.008));
    this.master?.gain.setTargetAtTime(this.volume,this.context.currentTime,.01);
  }
  async play(){
    if(!this.clip||this.playing||this.pending)return;
    const run=this.generation;this.pending=true;
    try {
    const clip=this.clip;this.context??=new AudioContext();await this.context.resume();
    const buffers=await Promise.all([this.buffer(clip,'mix'),this.buffer(clip,this.stem)]);
    if(run!==this.generation)return;
    this.master=this.context.createGain();this.master.gain.value=this.volume;this.master.connect(this.context.destination);
    const start=this.context.currentTime+.03;this.started=start;
    this.gains=[];this.sources=buffers.map(buffer=>{
      const source=this.context!.createBufferSource();source.buffer=buffer;
      source.loop=this.loop;source.loopStart=clip.core_offset;source.loopEnd=clip.core_offset+clip.core_seconds;
      const gain=this.context!.createGain();gain.gain.value=0;this.gains.push(gain);
      source.connect(gain);gain.connect(this.master!);source.start(start,Math.min(this.offset,clip.seconds-.01));return source;
    });
    this.sources[0].onended=()=>{if(!this.loop&&this.playing){this.pause();this.offset=clip.core_offset;}};
    this.playing=true;this.applyGains();
    } finally {if(run===this.generation)this.pending=false;}
  }
  pause(){
    this.generation++;this.pending=false;
    this.offset=this.position;this.playing=false;
    for(const s of this.sources){s.onended=null;try{s.stop();}catch{}s.disconnect();}
    this.sources=[];this.gains.forEach(g=>g.disconnect());this.gains=[];this.master?.disconnect();
  }
  async seek(seconds:number){const was=this.playing;this.pause();this.offset=seconds;if(was)await this.play();}
  dispose(){this.pause();void this.context?.close();this.cache.clear();}
}
