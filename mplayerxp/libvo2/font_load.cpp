#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;

#include <iostream>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "font_load.h"
#include "sub.h"
#include "vo_msg.h"

raw_file* load_raw(const std::string& name,int verbose){
    int bpp;
    raw_file* raw=new raw_file;
    unsigned char head[32];
    std::ifstream f;
    f.open(name.c_str(),std::ios_base::in|std::ios_base::binary);
    if(!f.is_open()) { delete raw; return NULL; } // can't open
    f.read((char*)(head),32); if(!f.good()) { delete raw; f.close(); return NULL; } // too small
    if(memcmp(head,"mhwanh",6)) { delete raw; f.close(); return NULL; } // not raw file
    raw->w=head[8]*256+head[9];
    raw->h=head[10]*256+head[11];
    raw->c=head[12]*256+head[13];
    if(raw->w == 0) /* 2 bytes were not enough for the width... read 4 bytes from the end of the header */
	raw->w = ((head[28]*0x100 + head[29])*0x100 + head[30])*0x100 + head[31];
    if(raw->c>256) { delete raw; f.close(); return NULL; }  // too many colors!?
    mpxp_v<<"RAW: "<<name<<" "<<raw->w<<" x "<<raw->h<<", "<<raw->c<<" colors"<<std::endl;
    if(raw->c){
	raw->pal=new unsigned char [raw->c*3];
	f.read((char*)(raw->pal),3*raw->c);
	bpp=1;
    } else {
	raw->pal=NULL;
	bpp=3;
    }
    raw->bmp=new unsigned char [raw->h*raw->w*bpp];
    f.read((char*)(raw->bmp),raw->h*raw->w*bpp);
    f.close();
    return raw;
}

font_desc_t* read_font_desc(const std::string& fname,float factor,int verbose){
    char sor[1024];
    unsigned char sor2[1024];
    font_desc_t *desc;
    char *dn;
    char section[64];
    int i,j;
    int chardb=0;
    int fontdb=-1;

    desc=new(zeromem) font_desc_t;
    if(!desc) return NULL;

    std::ifstream f;
    f.open(fname.c_str(),std::ios_base::in);
    if(!f.is_open()) {
	mpxp_err<<"font: can't open file: "<<fname<<std::endl;
	delete desc;
	return NULL;
    }

    i = fname.length() - 9;
    if ((dn = new char [i+1])){
	strncpy (dn, fname.c_str(), i);
	dn[i]='\0';
    }

    desc->fpath = dn; // search in the same dir as fonts.desc

    desc->charspace=2;
    desc->spacewidth=12;
    desc->height=0;
    for(i=0;i<512;i++) desc->start[i]=desc->width[i]=desc->font[i]=-1;

    section[0]=0;

    while(f.getline(sor,1020)){
	char* p[8];
	int pdb=0;
	unsigned char *s=(unsigned char *)sor;
	unsigned char *d=sor2;
	int ec=' ';
	int id=0;
	sor[1020]=0;
	p[0]=(char *)d;++pdb;
	while(1){
	    int c=*s++;
	    if(c==0 || c==13 || c==10) break;
	    if(!id){
		if(c==39 || c==34){ id=c;continue;} // idezojel
		if(c==';' || c=='#') break;
		if(c==9) c=' ';
		if(c==' '){
		    if(ec==' ') continue;
		    *d=0; ++d;
		    p[pdb]=(char *)d;++pdb;
		    if(pdb>=8) break;
		    continue;
		}
	    } else {
		if(id==c){ id=0; continue; } // idezojel
	    }
	    *d=c;d++;
	    ec=c;
	}
	if(d==sor2) continue; // skip empty lines
	*d=0;

	if(pdb==1 && p[0][0]=='['){
	    int len=strlen(p[0]);
	    if(len && len<63 && p[0][len-1]==']'){
		strcpy(section,p[0]);
		mpxp_v<<"font: Reading section: "<<section<<std::endl;
		if(strcmp(section,"[files]")==0){
		    ++fontdb;
		    if(fontdb>=16) {
			mpxp_err<<"font: Too many bitmaps defined!"<<std::endl;
			delete desc;
			return NULL;
		    }
		}
		continue;
	    }
	}

	if(strcmp(section,"[fpath]")==0){
	    if(pdb==1){
		if (desc->fpath)
		    delete desc->fpath; // release previously allocated memory
		desc->fpath=mp_strdup(p[0]);
		continue;
	    }
	} else if(strcmp(section,"[files]")==0){
	    const char *default_dir=DATADIR"/font";
	    if(pdb==2 && strcmp(p[0],"alpha")==0){
		char *cp;
		if (!(cp=new char [strlen(desc->fpath)+strlen(p[1])+2])) {
		    delete desc;
		    return NULL;
		}
		snprintf(cp,strlen(desc->fpath)+strlen(p[1])+2,"%s/%s",
			desc->fpath,p[1]);
		if(!((desc->pic_a[fontdb]=load_raw(cp,verbose)))){
		    delete cp;
		    if (!(cp=new char [strlen(default_dir)+strlen(p[1])+2])) {
			delete desc;
			return NULL;
		    }
		    snprintf(cp,strlen(default_dir)+strlen(p[1])+2,"%s/%s",
			    default_dir,p[1]);
		    if (!((desc->pic_a[fontdb]=load_raw(cp,verbose)))){
			mpxp_err<<"Can't load font bitmap: "<<p[1]<<std::endl;
			delete cp;
			delete desc;
			return NULL;
		    }
		}
		delete cp;
		continue;
	    }
	    if(pdb==2 && strcmp(p[0],"bitmap")==0){
		char *cp;
		if (!(cp=new char [strlen(desc->fpath)+strlen(p[1])+2])) {
		    delete desc;
		    return NULL;
		}
		snprintf(cp,strlen(desc->fpath)+strlen(p[1])+2,"%s/%s",
			desc->fpath,p[1]);
		if(!((desc->pic_b[fontdb]=load_raw(cp,verbose)))){
		    delete cp;
		    if (!(cp=new char [strlen(default_dir)+strlen(p[1])+2])) {
			delete desc;
			return NULL;
		    }
		    snprintf(cp,strlen(default_dir)+strlen(p[1])+2,"%s/%s",
			    default_dir,p[1]);
		    if (!((desc->pic_b[fontdb]=load_raw(cp,verbose)))){
			mpxp_err<<"Can't load font bitmap: "<<p[1]<<std::endl;
			delete cp;
			delete desc;
			return NULL;
		    }
		}
		delete cp;
		continue;
	    }
	} else if(strcmp(section,"[info]")==0){
	    if(pdb==2 && strcmp(p[0],"name")==0){
		desc->name=mp_strdup(p[1]);
		continue;
	    }
	    if(pdb==2 && strcmp(p[0],"spacewidth")==0){
		desc->spacewidth=atoi(p[1]);
		continue;
	    }
	    if(pdb==2 && strcmp(p[0],"charspace")==0){
		desc->charspace=atoi(p[1]);
		continue;
	    }
	    if(pdb==2 && strcmp(p[0],"height")==0){
		desc->height=atoi(p[1]);
		continue;
	    }
	} else if(strcmp(section,"[characters]")==0){
	    if(pdb==3){
		int chr=p[0][0];
		int start=atoi(p[1]);
		int end=atoi(p[2]);
		if(sub_data.unicode && (chr>=0x80)) chr=(chr<<8)+p[0][1];
		else if(strlen(p[0])!=1) chr=strtol(p[0],NULL,0);
		if(end<start) mpxp_err<<"error in font desc: end<start for char '"<<chr<<"'"<<std::endl;
		else {
		    desc->start[chr]=start;
		    desc->width[chr]=end-start+1;
		    desc->font[chr]=fontdb;
		    ++chardb;
		}
		continue;
	    }
	}
	mpxp_err<<"Syntax error in font desc: "<<sor<<std::endl;
    }
    f.close();
    for(i=0;i<=fontdb;i++){
	if(!desc->pic_a[i] || !desc->pic_b[i]){
	    mpxp_err<<"font: Missing bitmap(s) for sub-font #"<<i<<std::endl;
	    delete desc;
	    return NULL;
	}
	// re-sample alpha
	int ff=factor*256.0f;
	int size=desc->pic_a[i]->w*desc->pic_a[i]->h;
	mpxp_v<<"font: resampling alpha by factor "<<factor<<" ("<<ff<<")"<<std::endl;
	for(j=0;j<size;j++){
	    int x=desc->pic_a[i]->bmp[j];	// alpha
	    int y=desc->pic_b[i]->bmp[j];	// bitmap

#ifdef FAST_OSD
	    x=(x<(255-ff))?0:1;
#else

	    x=255-((x*ff)>>8); // scale

	    if(x+y>255) x=255-y; // to avoid overflows

	    if(x<1) x=1; else
	    if(x>=252) x=0;
#endif

	    desc->pic_a[i]->bmp[j]=x;
//          desc->pic_b[i]->bmp[j]=0; // hack
	}
	mpxp_v<<"DONE!"<<std::endl;
	if(!desc->height) desc->height=desc->pic_a[i]->h;
    }

    j='_';
    if(desc->font[j]<0) j='?';
    for(i=0;i<512;i++)
	if(desc->font[i]<0){
	    desc->start[i]=desc->start[j];
	    desc->width[i]=desc->width[j];
	    desc->font[i]=desc->font[j];
	}
    desc->font[' ']=-1;
    desc->width[' ']=desc->spacewidth;

    mpxp_ok<<"Font "<<fname<<" loaded successfully! ("<<chardb<<" chars)"<<std::endl;
    return desc;
}

#if 0
int main(){

read_font_desc("high_arpi.desc",1);

}
#endif
