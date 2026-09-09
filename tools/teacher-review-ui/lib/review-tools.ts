type Tool={name:string;description:string;inputSchema:object;annotations:{readOnlyHint:boolean;untrustedContentHint:boolean};execute:(input:unknown)=>unknown};
export function registerReviewTools(read:()=>unknown,stage:(id:string)=>Promise<unknown>){
  const context=(document as Document & {modelContext?:{registerTool:(tool:Tool,options:{signal:AbortSignal})=>void|Promise<void>}}).modelContext;
  if(!context?.registerTool)return;
  const lifecycle=new AbortController();
  const register=(tool:Tool)=>{try{void Promise.resolve(context.registerTool(tool,{signal:lifecycle.signal})).catch(console.warn);}catch(e){console.warn(e);}};
  register({name:'read_teacher_review',description:'Read selected audio, available clip IDs and review state; no playback or evaluation.',inputSchema:{type:'object',properties:{},additionalProperties:false},annotations:{readOnlyHint:true,untrustedContentHint:true},execute:read});
  register({name:'stage_teacher_clip',description:'Select a clip for the human to listen to. Pauses audio; does not play, judge or submit a review.',inputSchema:{type:'object',properties:{id:{type:'string'}},required:['id'],additionalProperties:false},annotations:{readOnlyHint:false,untrustedContentHint:true},execute:async(input)=>{
    if(!input||typeof input!=='object'||Object.keys(input).length!==1||!('id' in input)||typeof input.id!=='string')throw new Error('Expected one clip id');
    return stage(input.id);
  }});
  return ()=>lifecycle.abort();
}
