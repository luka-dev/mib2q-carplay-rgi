// Short-lived single-pass aperture. The expensive glass lighting is rendered
// once into a native-resolution RGBA cache; video stays in its native external image.
uniform PRISM_VIDEO_SAMPLER uVideo;
uniform sampler2D uArt;
uniform vec4 uArtRect; // native-resolution crop in panel coordinates
uniform sampler2D uMaterial;
uniform vec4 uCamera; // centre.xy, reciprocal zoom, logo unit
uniform vec4 uMix;    // eased progress, camera body offset, reveal, final dissolve
varying vec2 v_uv;
void main() {
    vec2 panel=vec2(1440.0,542.0);
    vec2 px=v_uv*panel;
    vec2 q=(px-uCamera.xy)*(uCamera.z/uCamera.w)+vec2(0.0,uMix.y);
    vec2 materialUV=q/0.46+0.5;
    float inside=step(0.0,materialUV.x)*step(0.0,materialUV.y)
                *step(materialUV.x,1.0)*step(materialUV.y,1.0);
    vec4 material=texture2D(uMaterial,materialUV);
    float aperture=material.a*inside;
    vec2 artUV=(uCamera.xy+q*uCamera.w-uArtRect.xy)/uArtRect.zw;
    float inArt=step(0.0,artUV.x)*step(0.0,artUV.y)*step(artUV.x,1.0)*step(artUV.y,1.0);
    vec3 glass=texture2D(uArt,vec2(artUV.x,1.0-artUV.y)).rgb*inArt;
    // Fade the signature before it could grow with the camera.
    glass*=mix(1.0,aperture,smoothstep(0.0,0.14,uMix.x));
    // Reuse the material's baked normal and depth: a lens at the glass edge,
    // with one native-video fetch, no RGB split, blur, or extra render pass.
    vec2 normal=material.gb*2.0-1.0;
    float rim=1.0-smoothstep(0.50,0.64,material.r);
    float lens=4.0*uMix.x*(1.0-uMix.x)*(1.0-uMix.w);
    vec2 bend=normal*vec2(0.004,0.010)*rim*aperture*lens;
    vec3 video=texture2D(uVideo,clamp(v_uv+bend,0.0,1.0)).rgb;
    vec3 col=mix(glass,video*aperture,uMix.z);
    float glint=max(0.0,dot(normal,vec2(-0.65,-0.76)));
    col+=vec3(0.32,0.56,0.85)*glint*glint*rim*aperture*lens*0.24;
    // Exact video at the endpoint, including all compact/classic layouts.
    gl_FragColor=vec4(mix(col,video,uMix.w),1.0);
}
