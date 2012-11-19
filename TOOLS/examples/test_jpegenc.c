#define		W	32
#define		H	32

int quant_store[MBR+1][MBC+1];
unsigned char buf[W*H*3/2];
char code[256*1024];


main() {
	int i, size;
	FILE *fp;

	memset(buf, 0, W*H);
	memset(buf+W*H, 255, W*H/4);
	memset(buf+5*W*H/4, 0, W*H/4);
	mjpeg_encoder_init(W, H, 1, W, 1, W/2, 1, W/2, 1, 1, 0);

	size = mjpeg_encode_frame(buf, buf+W*H, buf+5*W*H/4, code);
	fp = fopen("test.jpg", "w");
	fwrite(code, 1, size, fp);
	fclose(fp);
}
