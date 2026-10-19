#define _GNU_SOURCE
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void write_cfg(const char*p,const char*text){FILE*f=fopen(p,"wb");assert(f);assert(fwrite(text,1,strlen(text),f)==strlen(text));assert(!fclose(f));}
int main(void){char root[]="/tmp/seedless-config-test.XXXXXX";assert(mkdtemp(root));char cfgpath[512],bios7[512],bios9[512];snprintf(cfgpath,sizeof cfgpath,"%s/config.json",root);snprintf(bios7,sizeof bios7,"%s/a7",root);snprintf(bios9,sizeof bios9,"%s/a9",root);FILE*f=fopen(bios7,"wb");assert(f);for(int i=0;i<16384;i++)fputc(0,f);fclose(f);f=fopen(bios9,"wb");assert(f);for(int i=0;i<4096;i++)fputc(0,f);fclose(f);char text[2048];snprintf(text,sizeof text,"{\"visuals\":{\"threaded_3d\":false,\"thread_count\":0,\"disable_edge_marking\":false,\"fix_main_screen\":false,\"auto_trim\":false},\"firmware\":{\"bios_arm7\":\"%s\",\"bios_arm9\":\"%s\",\"firmware_path\":\"\"},\"user\":{\"nickname\":\"SEEDLESS\",\"birthday_month\":3,\"birthday_day\":2,\"color\":0},\"emulation\":{\"save_dir\":\"\"}}",bios7,bios9);write_cfg(cfgpath,text);seedless_config_t c={0};seedless_config_error_t e={0};assert(seedless_config_load_or_create(&c,cfgpath,&e));assert(c.visuals.thread_count==0&&c.user.birthday_month==3&&c.user.birthday_day==2&&c.user.nickname_units==8);seedless_config_free(&c);write_cfg(cfgpath,"{\"a\":1,\"a\":2}");assert(!seedless_config_load_or_create(&c,cfgpath,&e));assert(e.message[0]);seedless_config_free(&c);unlink(cfgpath);unlink(bios7);unlink(bios9);rmdir(root);return 0;}
