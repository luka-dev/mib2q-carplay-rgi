#ifndef CURSOR_GLASS_SHADER_H
#define CURSOR_GLASS_SHADER_H
/* Procedural optical ring: no sprite, scene texture or video-buffer access. */
static const char *cursor_glass_fragment =
#if defined(__APPLE__) && !defined(__QNX__)
    "#version 120\n"
#else
    "precision mediump float;\n"
#endif
    "uniform float alpha;uniform float lightPhase;uniform vec2 motion;varying vec2 uv;\n"
    "float lip(float r){float a=(r-35.3)/0.85;float b=(r-28.9)/0.95;"
    "return exp(-a*a)+0.55*exp(-b*b);}\n"
    "void main(){\n"
    "vec2 p=(uv*2.0-1.0)*56.0;float speed=min(length(motion),1.0);\n"
    "vec2 axis=motion/max(length(motion),0.001);\n"
    "vec2 q=p-axis*dot(p,axis)*(0.065*speed);\n"
    "q+=(p-axis*dot(p,axis))*(0.025*speed);\n"
    "float r=length(q);vec2 radial=q/max(r,0.001);\n"
    "vec2 orbit=vec2(cos(lightPhase),sin(lightPhase));\n"
    "float sweep=pow(max(dot(radial,orbit),0.0),8.0);\n"
    "float ripple=0.5+0.5*sin(lightPhase*2.0+radial.x*3.2+radial.y*2.4);\n"
    "float d=abs(r-32.0);float cover=1.0-smoothstep(4.15,5.25,d);\n"
    "float b=clamp((r-32.0)/4.8,-1.0,1.0);\n"
    "float z=sqrt(max(0.0,1.0-b*b));vec3 n=vec3(radial*b,z);\n"
    "float fresnel=pow(1.0-z,2.0);\n"
    "vec3 light=normalize(vec3(-0.42+motion.x*0.26+orbit.x*0.14,-0.62+motion.y*0.26+orbit.y*0.14,0.72));\n"
    "float spec=pow(max(dot(n,light),0.0),20.0);\n"
    "float reflection=pow(0.5+0.5*sin(n.x*3.8+n.y*4.7+n.z*2.2+0.38*sin(lightPhase)),3.0);\n"
    "float front=max(dot(radial,motion),0.0);\n"
    "vec3 tint=mix(vec3(0.18,0.30,0.43),vec3(0.48,0.29,0.66),"
    "clamp(0.5+radial.y*0.42-radial.x*0.12,0.0,1.0));\n"
    "vec3 col=vec3(0.22,0.26,0.30)+tint*reflection*(0.55+ripple*0.15)+vec3(spec*0.9);\n"
    "col=mix(col,vec3(0.93,0.97,1.0),fresnel*0.88);\n"
    "vec2 dispersionAxis=normalize(vec2(-0.65,-0.75)+motion*0.65+orbit*0.22);\n"
    "float shift=dot(radial,dispersionAxis)*(1.05+speed*0.70+sweep*0.45+ripple*0.12);\n"
    "vec3 chroma=vec3(lip(r+shift),lip(r),lip(r-shift));\n"
    "col+=chroma*(0.90+front*0.30+sweep*0.35);\n"
    "float a=cover*clamp(0.28+fresnel*0.48+spec*0.28+max(chroma.r,max(chroma.g,chroma.b))*0.42,0.0,0.96);\n"
    "float sr=length(q-vec2(0.0,2.0));\n"
    "float shadow=(1.0-smoothstep(35.0,43.5,sr))*smoothstep(25.5,29.5,sr)*0.30;\n"
    "float key=(1.0-smoothstep(0.45,1.65,abs(r-37.0)))*0.56;\n"
    "float dark=key+shadow*(1.0-key);\n"
    "vec3 rgb=clamp(col,0.0,1.0)*a;\n"
    "a+=dark*(1.0-a);\n"
    "gl_FragColor=vec4(min(rgb,vec3(a)),a)*alpha;\n"
    "}\n";
#endif
