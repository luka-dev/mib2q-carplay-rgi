'use client';
// Imperative WebAudio and external review state intentionally synchronize through effects.
/* oxlint-disable react/react-compiler */
import {useEffect,useRef,useState} from 'react';
import {Button} from '@/components/ui/button';
import {Input} from '@/components/ui/input';
import {Textarea} from '@/components/ui/textarea';
import {Slider} from '@/components/ui/slider';
import {Switch} from '@/components/ui/switch';
import {RadioGroup,RadioGroupItem} from '@/components/ui/radio-group';
import {Play,Pause,RefreshCw,Headphones,Check,Volume2} from 'lucide-react';
import {API,Listening,type Clip,type Mode} from '@/lib/listening';
import {registerReviewTools} from '@/lib/review-tools';

type Note={id:string;stem:string;revision:string;decision:string;notes:string;position:number};
type Catalog={items:Clip[];counts:Record<string,number>;annotations:Note[];training_policy:string};
const format=(v:number)=>`${Math.floor(v/60)}:${Math.floor(v%60).toString().padStart(2,'0')}`;
const choices=[['good','Чисто'],['unsure','Сомневаюсь'],['bad','Ошибка'],['absent','Источника нет']];

export default function Home(){
  const engine=useRef<Listening|null>(null);const generation=useRef(0);
  const seeking=useRef(false);
  const [catalog,setCatalog]=useState<Catalog>();const [clip,setClip]=useState<Clip>();
  const [stem,setStem]=useState('vocals');const [mode,setMode]=useState<Mode>('original');
  const [playing,setPlaying]=useState(false);const [position,setPosition]=useState(0);
  const [loop,setLoop]=useState(true);const [volume,setVolume]=useState(50);
  const [query,setQuery]=useState('');const [busy,setBusy]=useState(false);
  const [error,setError]=useState('');const [notice,setNotice]=useState('');
  const [notes,setNotes]=useState('');const [decision,setDecision]=useState('unsure');
  const audio=()=>engine.current??=(new Listening());
  const refresh=async()=>{
    try{const r=await fetch(`${API}/api/catalog`);if(!r.ok)throw new Error('API недоступен');
      const c:Catalog=await r.json();setCatalog(c);setError('');return c;
    }catch{setError('Локальный сервер аудио недоступен. Запусти teacher_review_server.py и обнови список.');}
  };
  async function choose(c:Clip){
    if(busy)return;const run=++generation.current;setBusy(true);setPlaying(false);setError('');setNotice('');
    const s=c.stems.some(x=>x.id==='hh')?'hh':'vocals';setClip(c);setStem(s);setMode('original');setNotes('');setDecision('unsure');
    try{audio().setMode('original');await audio().select(c,s);if(run===generation.current)setPosition(audio().position);}
    catch(e){setError(String(e));}finally{if(run===generation.current)setBusy(false);}
  }
  useEffect(()=>{let live=true;void refresh().then(c=>{if(live&&c?.items[0])void choose(c.items[0]);});
    const timer=setInterval(()=>{if(engine.current){if(!seeking.current)setPosition(engine.current.position);setPlaying(engine.current.playing);}},100);
    return()=>{live=false;clearInterval(timer);engine.current?.dispose();engine.current=null;};
  // One bootstrap only: choosing another clip must not rebuild the audio engine.
  // oxlint-disable-next-line react-hooks/exhaustive-deps
  },[]);
  const toolState=useRef({catalog,clip,stem,busy,choose});
  useEffect(()=>{toolState.current={catalog,clip,stem,busy,choose};});
  useEffect(()=>registerReviewTools(()=>({selected:toolState.current.clip?.id,stem:toolState.current.stem,busy:toolState.current.busy,clips:toolState.current.catalog?.items.map(c=>({id:c.id,title:c.title,specialist:c.specialist})),trainingApproved:false}),async(id)=>{
    const state=toolState.current;if(state.busy)throw new Error('Audio is loading');
    const selected=state.catalog?.items.find(c=>c.id===id);if(!selected)throw new Error('Unknown clip');
    await state.choose(selected);await new Promise<void>(resolve=>requestAnimationFrame(()=>resolve()));
    return {selected:id,playing:false,trainingApproved:false};
  }),[]);
  useEffect(()=>{
    const prior=catalog?.annotations.filter(n=>n.id===clip?.id&&n.stem===stem&&n.revision===clip.revision).at(-1);
    setNotes(prior?.notes??'');setDecision(prior?.decision??'unsure');
  },[clip?.id,clip?.revision,stem,catalog?.annotations]);
  async function toggle(){if(busy||!clip)return;try{if(audio().playing)audio().pause();else await audio().play();setPlaying(audio().playing);}catch(e){setError(String(e));}}
  function changeMode(m:Mode){setMode(m);audio().setMode(m);}
  async function changeStem(s:string){if(busy)return;setBusy(true);setError('');setNotice('');
    try{await audio().selectStem(s);setStem(s);changeMode('solo');}catch(e){setError(String(e));}finally{setBusy(false);}}
  async function save(){if(!clip)return;setError('');
    const data={id:clip.id,revision:clip.revision,stem,decision,notes,position:audio().position};
    try{const r=await fetch(`${API}/api/annotations`,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});
      if(!r.ok)throw new Error('Оценка не сохранена. Обнови список: результаты могли измениться.');
      const saved=await r.json() as Note;setCatalog(c=>c?{...c,annotations:[...c.annotations,saved]}:c);setNotice('Оценка сохранена локально. Автоматического допуска в обучение нет.');
    }catch(e){setError(String(e));}
  }
  const filtered=catalog?.items.filter(c=>`${c.artist} ${c.title}`.toLowerCase().includes(query.toLowerCase()))??[];
  const selected=clip?.stems.find(s=>s.id===stem);
  return <main className="room">
    <header className="mast"><div className="wordmark">AUDI / LISTENING ROOM</div><div className="local">● Только этот компьютер · аудио не загружается в сеть</div><Button variant="outline" onClick={()=>void refresh()} disabled={busy}><RefreshCw size={16}/> Обновить</Button></header>
    <div className="workbench"><aside className="library"><h2>ФРАГМЕНТЫ ДЛЯ ПРОВЕРКИ</h2><Input aria-label="Поиск песни" placeholder="Песня или исполнитель" value={query} onChange={e=>setQuery(e.target.value)}/>
      <p className="fine" style={{marginTop:12}}>{catalog?.items.length??0} доступны для прослушивания · {catalog?.counts.full??0} в полном покрытии</p>
      <div className="tracklist">{filtered.map(c=><button disabled={busy} key={c.id} className={`track ${clip?.id===c.id?'active':''}`} onClick={()=>void choose(c)}><strong>{c.title}</strong><span>{c.artist} · {format(c.start)}</span><span>{c.specialist?'● Хэт / бэк-вокал / инструменты':'Вокал / инструментал'}{c.partition==='heldout'?' · КОНТРОЛЬ':''}</span></button>)}</div></aside>
    <section><div className="eyebrow"><Headphones size={17} style={{display:'inline',marginRight:8}}/>Проверка учителей</div><h1>{clip?.title??'Слушаем, прежде чем обучать'}</h1><p className="sub">{clip?`${clip.artist} · фрагмент с ${format(clip.start)} · ${clip.core_seconds.toFixed(1)} сек.`:'Загружаю доступные фрагменты…'}</p>
      {error&&<div role="alert" className="message error">{error}</div>}
      {!!clip?.flags?.length&&<output className="message" style={{display:'block'}}><strong>Приоритет ручной проверки · это сигналы риска, не вердикт</strong>{clip.flags.map(flag=><p key={flag} style={{marginTop:8}}>{flag}</p>)}</output>}
      <div className="transport"><div className="transport-row"><Button size="lg" onClick={()=>void toggle()} disabled={!clip||busy}>{playing?<Pause/>:<Play/>}{busy?'Подготовка…':playing?'Пауза':'Слушать'}</Button><span className="clock">{format(Math.max(0,position-(clip?.core_offset??0)))} / {format(clip?.core_seconds??0)}</span>
      <label htmlFor="loop-playback" className="fine" style={{display:'flex',gap:10,alignItems:'center'}}><Switch id="loop-playback" checked={loop} onCheckedChange={v=>{setLoop(v);const p=audio().position;audio().loop=v;void audio().seek(p).catch(e=>setError(String(e)));}}/>По кругу</label>
      <div className="volume"><Volume2 size={18}/><Slider aria-label="Громкость прослушивания" value={[volume]} min={0} max={100} step={1} onValueChange={v=>{const n=Array.isArray(v)?v[0]:v;setVolume(n);audio().volume=n/100;audio().applyGains();}}/><span className="fine">{volume}%</span></div></div>
      <Slider aria-label="Позиция в фрагменте" disabled={!clip||busy} min={clip?.core_offset??0} max={(clip?.core_offset??0)+(clip?.core_seconds??1)-.01} step={.1} value={[position]} onValueChange={v=>{seeking.current=true;const n=Array.isArray(v)?v[0]:v;setPosition(n);}} onValueCommitted={v=>{seeking.current=false;void audio().seek(Array.isArray(v)?v[0]:v).catch(e=>setError(String(e)));}}/>
      <div className="modebar">{([['original','Оригинал'],['solo','Только дорожка'],['minus','Оригинал − дорожка']] as [Mode,string][]).map(([m,label])=><Button key={m} variant={mode===m?'default':'outline'} onClick={()=>changeMode(m)} aria-pressed={mode===m}>{label}</Button>)}</div>
      <p className="fine" style={{marginTop:14}}>Переключение на одной позиции. Один общий уровень для всех дорожек — без скрытого усиления тихих остатков.</p></div>
      <h2 style={{fontSize:20}}>Что выделил учитель</h2><div className="stemgrid">{clip?.stems.filter(s=>s.id!=='mix').map(s=><button className="stem" key={s.id} aria-pressed={stem===s.id} disabled={busy} onClick={()=>void changeStem(s.id)}>{s.label}<small>{stem===s.id?'Выбрано для сравнения':'Слушать отдельно / вычесть из песни'}</small></button>)}
      {!clip?.specialist&&['Хай-хэт · нужен DrumSep','Бэк-вокал · нужен отдельный учитель','Бас / гитара / пиано · нужен Demucs'].map(s=><button className="stem" key={s} disabled>{s}<small>Ещё не выделено · не имитация фильтром</small></button>)}</div>
      <p className="fine">Бэк-вокал — оценка с остатками и возможными ошибками, не гарантированно чистая гармония. Хэт проверяем на примеси малого, тарелок и вокальных «с». Нулевой остаток суммы не доказывает правильное разделение.</p>
      <div className="review"><h2>Оценка: {selected?.label??'выбери дорожку'}</h2><p className="fine">Есть ли нужный звук? Не попали ли сюда чужие инструменты? В режиме «Оригинал − дорожка» слышно, что именно исчезло.</p><RadioGroup aria-label="Качество выделения" className="review-actions" value={decision} onValueChange={v=>setDecision(String(v))}>{choices.map(([value,label])=><label key={value} style={{display:'flex',alignItems:'center',gap:9,padding:10}}><RadioGroupItem value={value}/>{label}</label>)}</RadioGroup>
      <Textarea aria-label="Что слышно неправильно" value={notes} maxLength={2000} onChange={e=>setNotes(e.target.value)} placeholder="Например: в хэте слышен вокал; бэк-вокал забрал основную мелодию…"/>
      <Button style={{marginTop:14}} onClick={()=>void save()} disabled={!clip||busy}><Check size={17}/>Сохранить оценку с позицией</Button>
      {notice&&<output className="message" style={{display:'block'}}>{notice}</output>}<p className="fine" style={{marginTop:16}}>{catalog?.training_policy} Прослушать несколько примеров полезно, но это не проверка всех {catalog?.counts.full??0} фрагментов.</p></div>
    </section></div>
  </main>;
}
