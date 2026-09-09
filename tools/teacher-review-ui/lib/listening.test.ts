import {test} from 'node:test';
import assert from 'node:assert/strict';
import {Listening,type Clip} from './listening.ts';
const clip={id:'test',revision:'r',seconds:24,core_offset:2,core_seconds:20} as Clip;
function fixture(){
  const starts:number[][]=[];const values:number[]=[];
  const context={currentTime:10,destination:{},resume:async()=>{},
    createGain:()=>({gain:{value:0,setTargetAtTime:(v:number)=>values.push(v)},connect:()=>{},disconnect:()=>{}}),
    createBufferSource:()=>({start:(...p:number[])=>starts.push(p),stop:()=>{},connect:()=>{},disconnect:()=>{},onended:null})};
  const player=new Listening();player.context=context as unknown as AudioContext;
  player.clip=clip;player.offset=2;player.buffer=async()=>({} as AudioBuffer);
  return {player,context,starts,values};
}
void test('two sources share clock and seek; mode uses subtraction without normalization',async()=>{
  const {player,context,starts,values}=fixture();await player.play();
  assert.deepEqual(starts,[[10.03,2],[10.03,2]]);
  assert.equal(player.position,2);context.currentTime=11.03;assert.equal(player.position,3);
  player.setMode('minus');assert.deepEqual(values.slice(-3),[1,-1,.5]);
  await player.seek(7);assert.deepEqual(starts.slice(-2),[[11.059999999999999,7],[11.059999999999999,7]]);
  player.pause();assert.equal(player.sources.length,0);
});
void test('double play does not duplicate sources',async()=>{
  const {player,starts}=fixture();await Promise.all([player.play(),player.play()]);assert.equal(starts.length,2);
});
void test('pause cancels pending play',async()=>{
  const {player,starts}=fixture();const pending=player.play();player.pause();await pending;
  assert.equal(starts.length,0);assert.equal(player.playing,false);
});
