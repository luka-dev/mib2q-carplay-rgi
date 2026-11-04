/* Real desktop GL validation of the exact shipping flight shader. */
#define main prism_preview_main
#include "prism_preview.c"
#undef main
#include <assert.h>

static void draw_art(scene_t *scene, const prism_layout_t *layout, GLuint material,
                     GLuint signature, int width, int height) {
    float raster[4]={0,0,W,H};
    if(width==PRISM_CACHE_WIDTH && height==PRISM_CACHE_HEIGHT)prism_cache_rect(layout,raster);
    glViewport(0,0,width,height);glUseProgram(scene->id);
    glUniform4fv(scene->raster,1,raster);
    glUniform2f(scene->res,(float)width,(float)height);glUniform1f(scene->time,3.7f);
    glUniform4fv(scene->content,1,layout->content);
    glUniform4fv(scene->sig_rect,1,layout->signature);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,material);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,signature);
    float vertices[24];prism_waiting_vertices_in_rect(layout,raster,vertices);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLES);
    for(int i=0;i<12;i++)glVertex2fv(vertices+i*2);
    glEnd();
}

int main(void) {
    assert(glfwInit());
    glfwWindowHint(GLFW_VISIBLE,GL_FALSE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER,GL_FALSE);
    GLFWwindow *win=glfwCreateWindow(W,H,"PRISM flight test",NULL,NULL);assert(win);
    glfwMakeContextCurrent(win);glfwSwapInterval(0);
    glDisable(GL_BLEND);glDisable(GL_DITHER);glViewport(0,0,W,H);
    GLuint flight=flight_program(),video=demo_video(NULL);assert(flight && video);
    int w,h;
    GLuint material=texture(PRISM_MATERIAL_PNG,sizeof(PRISM_MATERIAL_PNG),&w,&h);
    GLuint signature=texture(PRISM_SIGNATURE_PNG,sizeof(PRISM_SIGNATURE_PNG),&w,&h);
    scene_t scene;assert(load_scene(&scene));
    GLuint art,fbo;
    glGenTextures(1,&art);glBindTexture(GL_TEXTURE_2D,art);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,PRISM_CACHE_WIDTH,PRISM_CACHE_HEIGHT,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glGenFramebuffersEXT(1,&fbo);glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,art,0);
    assert(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
    size_t bytes=(size_t)W*H*4;
    unsigned char *reference=malloc(bytes),*pixels=malloc(bytes),*waiting=malloc(bytes);assert(reference && pixels && waiting);
    glUseProgram(0);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,video);
    glEnable(GL_TEXTURE_2D);glColor4f(1,1,1,1);video_quad();glDisable(GL_TEXTURE_2D);
    glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,reference);
    double total=0,maximum=0,first_draw=0,warm_maximum=0;
    for(int view=0;view<2;view++)for(int style=0;style<2;style++) {
        prism_layout_t layout=prism_layout(view,style,PRISM_SIGNATURE_W,PRISM_SIGNATURE_H);
        draw_art(&scene,&layout,material,signature,W,H);
        glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,waiting);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo);
        draw_art(&scene,&layout,material,signature,PRISM_CACHE_WIDTH,PRISM_CACHE_HEIGHT);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);glViewport(0,0,W,H);
        float art_rect[4];prism_cache_rect(&layout,art_rect);
        float previous_zoom=1;
        for(int frame=0;frame<=21;frame++) {
            float camera[4],mix[4];
            prism_transition_uniforms(&layout,frame/21.0f,camera,mix);
            assert(camera[2]<=previous_zoom && camera[2]>0);previous_zoom=camera[2];
            glUseProgram(flight);
            glUniform4fv(glGetUniformLocation(flight,"uArtRect"),1,art_rect);
            glUniform4fv(glGetUniformLocation(flight,"uCamera"),1,camera);
            glUniform4fv(glGetUniformLocation(flight,"uMix"),1,mix);
            glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,video);
            glUniform1i(glGetUniformLocation(flight,"uVideo"),0);
            glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,art);
            glUniform1i(glGetUniformLocation(flight,"uArt"),1);
            glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,material);
            glUniform1i(glGetUniformLocation(flight,"uMaterial"),2);
            double begin=glfwGetTime();video_quad();glFinish();
            double elapsed=glfwGetTime()-begin;total+=elapsed;if(elapsed>maximum)maximum=elapsed;
            if(view==0 && style==0 && frame==0)first_draw=elapsed;
            else if(elapsed>warm_maximum)warm_maximum=elapsed;
            glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
            assert(glGetError()==GL_NO_ERROR);
            for(size_t i=3;i<bytes;i+=4)assert(pixels[i]==255);
            if(frame==0) {
                unsigned largest=0;
                for(size_t i=0;i<bytes;i++){unsigned d=(unsigned)abs((int)pixels[i]-waiting[i]);if(d>largest)largest=d;}
                printf("Waiting -> flight0 style=%d view=%d max channel error=%u\n",style,view,largest);fflush(stdout);
                assert(largest<=2); /* no lower-resolution flash before the camera moves */
            }
            if(frame==21) {
                unsigned largest=0;
                for(size_t i=0;i<bytes;i++){unsigned d=(unsigned)abs((int)pixels[i]-reference[i]);if(d>largest)largest=d;}
                assert(largest<=1); /* no UV mirror/flip, crop, tint, or leftover logo */
            }
        }
    }
    printf("PRISM flight: four layouts, 88 opaque GPU frames, exact video endpoints PASS; desktop draw+finish mean %.3f ms max %.3f ms; first %.3f ms, subsequent mean %.3f ms max %.3f ms (not HU timing)\n",total*1000/88,maximum*1000,first_draw*1000,(total-first_draw)*1000/87,warm_maximum*1000);
    free(reference);free(pixels);free(waiting);glDeleteProgram(flight);glDeleteProgram(scene.id);
    glDeleteFramebuffersEXT(1,&fbo);glDeleteTextures(1,&signature);
    glDeleteTextures(1,&video);glDeleteTextures(1,&material);glDeleteTextures(1,&art);
    glfwDestroyWindow(win);glfwTerminate();return 0;
}
