#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include <setjmp.h>
#include <png.h>

#include "rom.h"
#include "decompress.h"
#include "lex.h"


int fput32(uint32_t i, FILE *f)
{
  for (int r = 0; r < 32; r += 8)
  {
    int s = fputc((i >> r) & 0xff, f);
    if (s == EOF) return EOF;
  }
  return 0;
}

int fput24(uint32_t i, FILE *f)
{
  for (int r = 0; r < 24; r += 8)
  {
    int s = fputc((i >> r) & 0xff, f);
    if (s == EOF) return EOF;
  }
  return 0;
}

int fput16(uint16_t i, FILE *f)
{
  for (int r = 0; r < 16; r += 8)
  {
    int s = fputc((i >> r) & 0xff, f);
    if (s == EOF) return EOF;
  }
  return 0;
}


char emsg[256];

void pngerr(png_structp p, png_const_charp e)
{
  sprintf(emsg, "libpng error: %s", e);
  longjmp(png_jmpbuf(p),1);
}



uint8_t *rombuf = NULL;
size_t romsize = 0;

uint8_t *mcmbuf = NULL;
size_t mcmsize = 0;



/****************************************************************/

#include "script.h"


int exportdsc(FILE *f, uint8_t *p)
{
	int status = 0;
	
	uint32_t dscindex = p-rombuf;
	uint32_t prefillbase = get32(p+0x04);
	uint32_t subscripts = get32(p+0x0c);
	uint8_t *prefill = prefillbase ? rombuf+(prefillbase-MAPBASE) : NULL;
	
	uint32_t labeltbl[256];
	int labels = 0;
	uint8_t *scriptbuf = mcmbuf;
	size_t datasize = mcmsize;
	
	/* PASS 1 - identify all labels */
	uint32_t cursuboffs = -1;
	int i = 0;
	while (i < datasize)
	{
		/* check for subscript start */
		for (int j = 0; j < subscripts; j++)
		{
			uint32_t suboffs = get32(p+0x10+(j*4));
			if (i == suboffs)
			{
				cursuboffs = suboffs;
			}
		}
		/* check params for any jumps */
		uint16_t opcode = get16(scriptbuf+i);
		i += 2;
		if (opcode < 0x23)
		{
			const uint8_t *arglist = cmdtbl[opcode].arglist;
			for (int j = 0; j < ARGLISTMAX; j++)
			{
				uint8_t type = arglist[j];
				if (!type) break;
				switch (type)
				{
					case A_STR:
						i += 2 + get16(scriptbuf+i);
						break;
					case A_JUMPOFFS:
					{
						uint16_t offs = get16(scriptbuf+i);
						uint32_t finaloffs = cursuboffs + offs*2;
						int foundlabel = 0;
						while (foundlabel < labels)
						{
							if (labeltbl[foundlabel] == finaloffs) break;
							foundlabel++;
						}
						if (foundlabel == labels)
						{
							if (labels == 256)
							{
								sprintf(emsg,"Too many labels");
								return 1;
							}
							labeltbl[labels++] = finaloffs;
						}
						/* replace jump distance with label id */
						write16(scriptbuf+i, foundlabel);
						i += 2;
						break;
					}
					case A_JUMPTABLE:
					{
						uint16_t len = get16(scriptbuf+i);
						i += 2;
						while (len)
						{
							uint16_t offs = get16(scriptbuf+i);
							uint32_t finaloffs = cursuboffs + offs*2;
							int foundlabel = 0;
							while (foundlabel < labels)
							{
								if (labeltbl[foundlabel] == finaloffs) break;
								foundlabel++;
							}
							if (foundlabel == labels)
							{
								if (labels == 256)
								{
									sprintf(emsg,"Too many labels");
									return 1;
								}
								labeltbl[labels++] = finaloffs;
							}
							/* replace jump distance with label id */
							write16(scriptbuf+i, foundlabel);
							i += 2;
							len--;
						}
						break;
					}
					default:
						i += 2;
						break;
				}
			}
		}
	}
	
	
	/* PASS 2 - output text to file */
	
	fprintf(f, "DSCBase $%06X\n\n", dscindex);
	
	uint16_t prvop = -1;
	int strnum = 0;
	i = 0;
	cursuboffs = -1;
	while (i < datasize)
	{
		/* check for subscript start */
		for (int j = 0; j < subscripts; j++)
		{
			uint32_t suboffs = get32(p+0x10+(j*4));
			if (i == suboffs)
			{
				fprintf(f, "\n\n");
				/* if there is any prefill data, output it in a comment */
				if (prefill)
				{
					uint8_t pfcnt = *(prefill++);
					if (pfcnt)
					{
						fprintf(f, "### Prefill data: ");
						for (int i = 0; i < pfcnt; i++)
						{
							fprintf(f, "$%02X", *(prefill++));
							if (i < pfcnt-1) fputc(',',f);
						}
					}
				}
				
				/* output subscript header */
				fprintf(f, "\nSubscript %i\n", j);
				cursuboffs = suboffs;
				prvop = -1; /* reset double-end detection */
			}
		}
		/* check for label start */
		for (int j = 0; j < labels; j++)
		{
			uint32_t loffs = labeltbl[j];
			if (i == loffs)
			{
				fprintf(f, "Label %i\n", j);
			}
		}
		
		/* get opcode */
		uint16_t opcode = get16(scriptbuf+i);
		i += 2;
		if (opcode >= 0x23)
		{
			fprintf(f, "NoOp%X\n", opcode);
		} /* don't output multiple EndScripts in a row */
		else if (opcode || prvop)
		{
			/* see if there is any text which must be commented */
			int argstart = i;
			const uint8_t *arglist = cmdtbl[opcode].arglist;
			for (int j = 0; j < ARGLISTMAX; j++)
			{
				uint8_t type = arglist[j];
				if (!type) break;
				switch (type)
				{
					case A_STR:
					{
						uint16_t strlen = get16(scriptbuf+i);
						i += 2;
						int stri = i;
						fprintf(f, "# ");
						while (1)
						{
							uint8_t c = scriptbuf[i++];
							if (!c)
							{ /* null termination */
								break;
							}
							else if (c == 0x0a)
							{ /* treat newlines separately */
								fprintf(f, "\n# ");
							}
							else if (c < 0x20 || c == 0x7f)
							{ /* control code */
								fprintf(f, "\\x%02x", c);
							}
							else if ((c >= 0x20 && c < 0x7f) || (c >= 0xa1 && c < 0xe0))
							{ /* printable char */
								fputc(c, f);
							}
							else
							{ /* JIS character */
								fputc(c, f);
								fputc(scriptbuf[i++], f);
							}
						}
						i = stri + strlen;
						fputc('\n', f);
						break;
					}
					default:
						i += 2;
						break;
				}
			}
			
			/* now output the command and args */
			fputs(cmdtbl[opcode].name, f);
			i = argstart;
			for (int j = 0; j < ARGLISTMAX; j++)
			{
				uint8_t type = arglist[j];
				if (!type) break;
				switch (type)
				{
					case A_STR:
						fprintf(f, " \"!!! %06X %i\"", dscindex, strnum++);
						i += 2 + get16(scriptbuf+i);
						break;
					case A_JUMPTABLE:
					{
						uint16_t len = get16(scriptbuf+i);
						i += 2;
						fprintf(f, " %i", len);
						while (len)
						{
							fprintf(f, " %i", (uint16_t)get16(scriptbuf+i));
							i += 2;
							len--;
						}
						break;
					}
					default:
						fprintf(f, " %i", (int16_t)get16(scriptbuf+i));
						i += 2;
						break;
				}
			}
			fputc('\n', f);
		}
		
		prvop = opcode;
	}
	
	
	return status;
}



/****************************************************************/

#define scale5to8(i) (((i) << 3) | ((i) >> 2))

int exportmbm(FILE *f, uint8_t *p)
{
	int status = 0;
	
	uint16_t palsize = get16(p+0x0a);
	uint8_t bpp = *(p+5); /* note: bits/pixel is equal to bytes/tile row */
	int tilemapflag = *(p+4) & 4;
	uint16_t width = get16(p+6);
	uint16_t height = get16(p+8);
	uint32_t palbase = get32(p+0x10);
	uint32_t tilemapbase = get32(p+0x14);
	uint32_t palindex = palbase-MAPBASE;
	uint32_t tilemapindex = tilemapbase-MAPBASE;
	if (palindex >= romsize)
	{
		sprintf(emsg,"Palette pointer $%07X out of bounds", palbase);
		return 1;
	}
	if (tilemapindex >= romsize)
	{
		sprintf(emsg,"Tilemap pointer $%07X out of bounds", tilemapbase);
		return 1;
	}
	if (bpp != 4 && bpp != 8)
	{
		sprintf(emsg,"Bad bits per pixel value %u", bpp);
		return 1;
	}
	unsigned bppcolors = 1 << bpp;
	int oversizepal = palsize > bppcolors;
	unsigned outbpp = (oversizepal || bpp == 8) ? 8 : 4;
	if (width % 8)
	{
		sprintf(emsg,"Width %u is not a multiple of 8", width);
		return 1;
	}
	if (height % 8)
	{
		sprintf(emsg,"Height %u is not a multiple of 8", height);
		return 1;
	}
	unsigned bmprowsize = outbpp == 4 ? (width/2) : (width);
	unsigned tilewidth = width / 8;
	unsigned tileheight = height / 8;
	
	
	
	uint8_t *tilebuf = mcmbuf;
	uint8_t *palptr = rombuf+palindex;
	uint8_t *tilemapptr = rombuf+tilemapindex;
	
	png_byte *bmptilerow = malloc(bmprowsize * 8);
	png_color *palette = malloc(palsize * sizeof(*palette));
	png_byte *trns = malloc(palsize);
	
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,pngerr,pngerr);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		status = 1;
	}
	else
	{
		png_init_io(png_ptr,f);
		
		png_set_IHDR(png_ptr,info_ptr,
				width, height, outbpp, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		
		for (unsigned i = 0; i < palsize; i++)
		{
			uint16_t col = get16(palptr+i*2);
			uint8_t red = col & 0x1f;
			uint8_t green = (col >> 5) & 0x1f;
			uint8_t blue = (col >> 10) & 0x1f;
			palette[i].red = scale5to8(red);
			palette[i].green = scale5to8(green);
			palette[i].blue = scale5to8(blue);
		}
		png_set_PLTE(png_ptr,info_ptr,palette,palsize);
		
		for (unsigned i = 0; i < palsize; i++)
		{
			trns[i] = (i%bppcolors == 0) ? 0 : 0xff;
		}
		png_set_tRNS(png_ptr,info_ptr,trns,palsize,NULL);
		
		png_write_info(png_ptr,info_ptr);
		
		png_byte *rows[8];
		for (unsigned i = 0; i < 8; i++) rows[i] = &bmptilerow[bmprowsize*i];
		
		for (unsigned ty = 0; ty < tileheight; ty++)
		{
			memset(bmptilerow,0,bmprowsize*8);
			for (unsigned tx = 0; tx < tilewidth; tx++)
			{
				uint16_t tm;
				unsigned tmindex = (ty * tilewidth) + tx;
				if (tilemapflag) tm = get16(tilemapptr + (tmindex)*2);
				else tm = tmindex;
				uint16_t t = tm & 0x3ff;
				uint16_t xflip = tm & 0x400;
				uint16_t yflip = tm & 0x800;
				uint16_t pal = (tm >> 12);
				
				uint8_t *tile = tilebuf + ((8 * bpp) * t);
				for (unsigned tyf = 0; tyf < 8; tyf++)
				{
					for (unsigned txf = 0; txf < 8; txf++)
					{
						unsigned ixf = xflip ? txf ^ 7 : txf;
						unsigned iyf = yflip ? tyf ^ 7 : tyf;
						
						uint8_t col;
						if (bpp == 4)
						{
							uint8_t cb = tile[(iyf*4)+(ixf/2)];
							col = ((ixf % 2) ? (cb >> 4) : (cb & 0xf)) | (pal << 4);
						}
						else
						{
							col = tile[(iyf*8)+ixf];
						}
						
						if (outbpp == 4)
						{
							png_byte *p = &bmptilerow[bmprowsize*tyf + ((tx*8) + txf)/2];
							if (txf % 2) *p |= col;
							else *p |= (col << 4);
						}
						else
						{
							bmptilerow[bmprowsize*tyf + (tx*8) + txf] = col;
						}
					}
				}
			}
			png_write_rows(png_ptr,rows,8);
		}
		
		png_write_end(png_ptr,info_ptr);
		
		sprintf(emsg, "%ux%u, %s, %ubpp, %u colors"
			, width, height
			, tilemapflag ? "tilemapped" : "bitmapped"
			, bpp
			, palsize);
			
	}
	
	png_destroy_write_struct(&png_ptr, &info_ptr);
	
	free(palette);
	free(trns);
	free(bmptilerow);
	
	return status;
}


/***************************************************************/

int exportmfm(FILE *f, uint8_t *p)
{
	/* because i need to rewrite this to export as png */
	sprintf(emsg,"Not implemented");
	return 1;
}




/****************************************************************/

#include "text-extract.h"





/****************************************************************/

int main(int argc, char *argv[])
{
	if (argc <= 2)
	{
		puts(
			"Di Gi Charat DiGiCommunication data extractor by karmic\n"
			"Usage: extract ROMFILE COMMAND...\n"
			"\n"
			"Valid commands are:\n"
			"  custom LISTFILE          (extracts raw data)\n"
			"  dsc OUTDIR               (extracts scripts)\n"
			"  mbm OUTDIR               (extracts graphics)\n"
			"  mcm OUTDIR               (extracts raw compressed data)\n"
			"  mfm OUTDIR               (extracts fonts)\n"
			"  mrm OUTDIR               (extracts audio)\n"
			"  text STRINGLIST OUTFILE  (extracts text strings in ROM)"
			);
		return EXIT_FAILURE;
	}
	
	char *romname = argv[1];
	
	/* interpret command line args */
	char *dscdir = NULL;
	char *mbmdir = NULL;
	char *mcmdir = NULL;
	char *mfmdir = NULL;
	char *mrmdir = NULL;
	
	char *customfile = NULL;
	char *stringlistfile = NULL;
	char *textoutfile = NULL;
	
	for (int i = 2; i < argc; i++)
	{
		char *cmd = argv[i++];
		if (i == argc)
		{
			printf("Not enough parameters for command %s\n",cmd);
			return EXIT_FAILURE;
		}
		
		if (!strcasecmp(cmd,"custom"))
		{
			customfile = argv[i];
			puts("WARNING: Custom export is NOT implemented yet");
		}
		else if (!strcasecmp(cmd,"dsc"))
		{
			dscdir = argv[i];
		}
		else if (!strcasecmp(cmd,"mbm"))
		{
			mbmdir = argv[i];
		}
		else if (!strcasecmp(cmd,"mcm"))
		{
			mcmdir = argv[i];
		}
		else if (!strcasecmp(cmd,"mfm"))
		{
			mfmdir = argv[i];
		}
		else if (!strcasecmp(cmd,"mrm"))
		{
			mrmdir = argv[i];
		}
		else if (!strcasecmp(cmd,"text"))
		{
			stringlistfile = argv[i++];
			if (i == argc)
			{
				printf("Not enough parameters for command %s\n",cmd);
				return EXIT_FAILURE;
			}
			textoutfile = argv[i];
		}
		else
		{
			printf("Unrecognized command %s\n",cmd);
		}
	}
	
	
	/* open rom */
	struct stat st;
	if (stat(romname,&st))
	{
		printf("Couldn't stat %s: %s",romname,strerror(errno));
		return EXIT_FAILURE;
	}
	romsize = st.st_size;
	FILE *romfile = fopen(romname,"rb");
	if (!romfile)
	{
		printf("Couldn't open %s: %s",romname,strerror(errno));
		return EXIT_FAILURE;
	}
	rombuf = malloc(romsize);
	if (!rombuf)
	{
		puts("Not enough memory for ROM data");
		return EXIT_FAILURE;
	}
	size_t romread = fread(rombuf,1,romsize,romfile);
	fclose(romfile);
	if (romread != romsize)
	{
		printf("Error while reading %s: %s",romname,strerror(errno));
		return EXIT_FAILURE;
	}
	
	
	
	char *initialcwd = getcwd(NULL,0);
	unsigned errors = 0;
	
	
	/******* do all "export everything" commands *********/
	for (uint8_t *p = rombuf; p < rombuf+romsize-0x20; p += 4)
	{
		int status = 0;
		emsg[0] = '\0';
		
		enum {
			TYPE_DSC = 0,
			TYPE_MBM,
			TYPE_MCM,
			TYPE_MFM,
			TYPE_MRM,
			AMT_TYPES
		};
		const char *dirnames[] = {dscdir,mbmdir,mcmdir,mfmdir,mrmdir};
		const char *sigs[] = {"DSC","MBM","MCM","MFM","MRM"};
		const char *exts[] = {"txt","png","bin","png","wav"};
		
		/* see if there is a block we are looking for here */
		int type = -1;
		for (int i = TYPE_DSC; i < AMT_TYPES; i++)
		{
			if (dirnames[i] && !memcmp(sigs[i],p,4))
			{
				type = i;
				break;
			}
		}
		if (type == -1) continue;
		
		unsigned index = p-rombuf;
		
		char outnamebuf[128];
		sprintf(outnamebuf, "%06X.%s",index,exts[type]);
		FILE *f = NULL;
		
		/* if this block uses MCM data, decompress it first */
		uint8_t *mcmsrc = NULL;
		mcmbuf = NULL;
		switch (type)
		{
			case TYPE_DSC:
				mcmsrc = rombuf + (get32(p+8) - MAPBASE);
				break;
			case TYPE_MBM:
				mcmsrc = p+0x18;
				break;
			case TYPE_MCM:
				mcmsrc = p;
				break;
		}
		
		if (mcmsrc)
		{
			unsigned mcmindex = mcmsrc-rombuf;
			mcmsize = getmcmsize(mcmsrc);
			mcmbuf = malloc(getmcmbufsize(mcmsrc));
			status = mcmuncomp(mcmbuf, rombuf, mcmsrc);
			if (status)
			{
				sprintf(emsg,"MCM decompressing at $%06X failed with errorcode %i",mcmindex,status);
				goto mcm_export_fail;
			}
		}
		
		/* setup the output file and get ready to export */
		const char *dirname = dirnames[type];
		if ((status = chdir(dirname)))
		{
			sprintf(emsg,"Couldn't change to directory %s: %s", dirname,strerror(errno));
			goto mcm_export_fail;
		}
		f = fopen(outnamebuf,"wb");
		if (!f)
		{
			sprintf(emsg,"Couldn't open %s for writing: %s", outnamebuf,strerror(errno));
			status = 1;
			goto mcm_export_fail;
		}
		
		switch (type)
		{
			case TYPE_DSC:
			{
				status = exportdsc(f, p);
				break;
			}
			case TYPE_MBM:
			{
				status = exportmbm(f, p);
				break;
			}
			case TYPE_MCM:
			{
				fwrite(mcmbuf,1,get32(p+4),f);
				break;
			}
			case TYPE_MFM:
			{
				status = exportmfm(f, p);
				break;
			}
			case TYPE_MRM:
			{
				uint32_t datasize = get32(p+4);
				
				fwrite("RIFF", 1, 4, f);
				fput32(36 + datasize, f);
				fwrite("WAVE", 1, 4, f);

				fwrite("fmt ", 1, 4, f);
				fput32(16, f);
				fput16(1, f);
				fput16(1, f);
				fput32(22050, f);
				fput32(22050, f);
				fput16(1, f);
				fput16(8, f);
				
				fwrite("data", 1, 4, f);
				fput32(datasize, f);
				
				for (int i = 0; i < datasize; i++)
				{
					fputc(p[8+i] + 0x80, f);
				}
				break;
			}
		}
		
		
		
		/* either done, or failed */
mcm_export_fail:
		free(mcmbuf);
		if (f) fclose(f);
		if (status)
		{
			remove(outnamebuf);
			printf("Error exporting %s at $%06X: %s\n", sigs[type], index, emsg);
			errors++;
		}
		else
		{
			printf("Successfully exported %s at $%06X", sigs[type], index);
			if (emsg[0]) printf(" (%s)",emsg);
			putchar('\n');
		}
		
		if (chdir(initialcwd))
		{
			printf("Couldn't return to %s\n",initialcwd);
			return EXIT_FAILURE;
		}
	}
	
	
	
	/* do text export */
	if (stringlistfile)
	{
		puts("Extracting text...");
		extracttext(stringlistfile,textoutfile,rombuf,romsize);
		puts("Completed.");
	}
	
	
}