#include "launch_options.h"
#include <assert.h>
#include <string.h>
int main(void){seedless_launch_options_t o;char e[128];char*a[]={"app","--save-dir","","--frames","4","game.nds"};assert(seedless_launch_parse(6,a,&o,e,sizeof e));assert(o.have_save_dir&&!strcmp(o.save_dir,"")&&o.have_frames&&o.frames==4&&!strcmp(o.rom_path,"game.nds"));char*b[]={"app","--frames","0","game.nds"};assert(!seedless_launch_parse(4,b,&o,e,sizeof e));char*c[]={"app","--help"};assert(seedless_launch_parse(2,c,&o,e,sizeof e)&&o.help);char*d[]={"app","--data-dir","x","game.nds"};assert(!seedless_launch_parse(4,d,&o,e,sizeof e));return 0;}
