// PRISM: light forms the glass Apple; the living material is the waiting state.
// No external light streak, enclosing orb, separate spinner or sharp caustic.
uniform vec2 iRes;
uniform float iTime;
uniform vec4 uContent;
uniform vec4 uSigRect;
uniform sampler2D uSig;
uniform sampler2D uMaterial;

float ramp(float a,float b,float t) { return smoothstep(a,b,t); }
float glow(float d,float w) { float z=d/w; return exp(-z*z); }
// Soft coverage of an arc, repeated across the angular seam. At a complete
// turn the three intervals sum to one, including their overlapping ends.
float softArc(float phase,float length) {
    return ramp(-0.09,0.09,phase)-ramp(length-0.09,length+0.09,phase);
}
float pool(vec2 p,vec2 center,vec2 spread) {
    vec2 v=(p-center)/spread;
    return exp(-dot(v,v));
}
vec3 spectrum(float x) {
    return 0.55+0.45*cos(6.2831853*(x+vec3(0.04,0.37,0.69)));
}
// Curved light volumes and soft internal caustics; no straight flare or glare.
vec3 liquidLight(vec2 p,float t) {
    float a=t*0.72;
    p+=0.023*vec2(sin(p.y*11.0+a*0.74),sin(p.x*9.0-a*0.67));
    vec2 cool=vec2(-0.085+0.085*sin(a),0.025+0.060*cos(a*0.81));
    vec2 violet=vec2(0.090*cos(a*0.73+0.8),0.065+0.060*sin(a*0.91));
    vec2 warm=vec2(0.085*cos(a*0.67+2.0),-0.090+0.060*sin(a*0.83+1.0));
    vec3 c=vec3(0.08,0.67,1.0)*pool(p,cool,vec2(0.14,0.080))*0.90;
    c+=vec3(0.61,0.22,0.95)*pool(p,violet,vec2(0.14,0.085))*0.63;
    c+=vec3(1.0,0.57,0.27)*pool(p,warm,vec2(0.15,0.080))*0.58;
    vec2 curl=(p-vec2(0.025*sin(a*0.63),-0.030*cos(a*0.57)))/vec2(0.20,0.14);
    float fold=glow(length(curl)-0.83,0.23)*ramp(-0.6,0.7,curl.y);
    c+=spectrum(atan(curl.y,curl.x)/6.2831853+a*0.040)*fold*0.27;
    return c;
}
void main() {
    float t=max(0.0,iTime);
    vec2 panel=vec2(1440.0,542.0);
    vec2 px=vec2(gl_FragCoord.x/iRes.x,1.0-gl_FragCoord.y/iRes.y)*panel;
    vec2 size=uContent.zw,local=px-uContent.xy;
    vec3 col=vec3(0.0);
    float edge=min(min(local.x,local.y),min(size.x-local.x,size.y-local.y));
    // Waiting artwork stays within the selected VC safe area.
    if(edge>=0.0) {
        float unit=min(size.x*0.78,(size.y-30.0)*1.02);
        vec2 center=uContent.xy+vec2(size.x*0.5,size.y*0.45);
        vec2 q=(px-center)/unit;
        float awaken=ramp(0.18,0.58,t);
        float fill=ramp(1.0,2.6,t);
        float traceProgress=ramp(0.18,1.75,t);
        // Constant silhouette size: only the glass and its lighting develop.
        vec2 uv=q/0.46+0.5;
        if(min(uv.x,uv.y)>=0.0 && max(uv.x,uv.y)<=1.0) {
            vec4 material=texture2D(uMaterial,uv);
            float mask=material.a;
            float sdf=(material.r-0.5)*0.25;
            vec2 normal=material.gb*2.0-1.0;
            float depth=max(0.0,sdf);
            float bevel=1.0-ramp(0.0,0.027,depth);
            float refraction=exp(-depth/0.040);
            float silhouette=1.0-ramp(0.001,0.024,max(0.0,sdf));

            // Preserve the thin contour stage. The whole translucent surface
            // then appears together: never erode a bright rim toward the core.
            float phase=fract(atan(q.y,q.x)/6.2831853+0.75);
            float drawn=softArc(phase,traceProgress)
                       +softArc(phase-1.0,traceProgress)
                       +softArc(phase+1.0,traceProgress);
            // Periodic light and tint have no cut at phase 0/1. The moving
            // light breathes in, then settles before the ends meet.
            float headEnvelope=ramp(0.0,0.12,traceProgress)
                              *(1.0-ramp(0.80,1.0,traceProgress));
            float travelling=glow(sin(3.14159265*(phase-traceProgress)),0.36)
                            *headEnvelope;
            vec3 contour=mix(vec3(0.59,0.70,0.88),spectrum(phase-0.1),0.26);
            float traceFade=1.0-ramp(1.8,2.8,t);
            col+=mask*silhouette*contour*(0.32*drawn+0.90*travelling)*awaken*traceFade;

            // Convex refraction and visibly separated spectral light paths.
            // Continuous lights make the Apple itself an activity indicator.
            vec2 warped=q+normal*refraction*0.050;
            float dispersion=refraction*(0.010+bevel*0.022);
            vec3 optical;
            optical.r=liquidLight(warped+normal*dispersion,t).r;
            optical.g=liquidLight(warped,t).g;
            optical.b=liquidLight(warped-normal*dispersion,t).b;
            float lightAngle=-2.1+0.40*sin(t*0.54);
            vec2 lightDir=vec2(cos(lightAngle),sin(lightAngle));
            float key=pow(max(0.0,dot(normal,lightDir)),3.0);
            float back=pow(max(0.0,dot(normal,-lightDir)),4.0);
            float upper=pool(q,vec2(-0.09+0.05*sin(t*0.48),-0.10),vec2(0.23,0.085));
            float lower=pool(q,vec2(0.10,-0.04+0.08*cos(t*0.39)),vec2(0.09,0.19));
            vec3 glass=vec3(0.065,0.075,0.10)+optical*0.76;
            glass+=vec3(0.11,0.13,0.17)*upper+vec3(0.09,0.11,0.14)*lower;
            glass+=bevel*(vec3(0.52,0.64,0.86)*key+vec3(0.37,0.33,0.38)*back);
            glass+=bevel*spectrum(atan(normal.y,normal.x)/6.2831853-t*0.060)*0.23;
            // A slight moving lens deformation develops as opacity rises. It
            // changes the transmitted lights, not the silhouette or fill mask.
            col+=mask*glass*fill;

        }

        // Typography has its own metrics; all pixels remain in the shared view.
        vec2 suv=(px-uSigRect.xy)/uSigRect.zw;
        if(min(suv.x,suv.y)>=0.0 && max(suv.x,suv.y)<=1.0) {
            float signature=texture2D(uSig,suv).a*ramp(1.65,2.6,t)*0.48;
            col=mix(col,vec3(0.78,0.82,0.90),signature);
        }
        col*=awaken;
        col*=ramp(0.0,9.0,edge);
    }
    gl_FragColor=vec4(col,1.0);
}
