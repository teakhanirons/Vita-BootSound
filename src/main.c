/*
  bootsound
  Copyright (C) 2020, teakhanirons
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <taihen.h>
#include <string.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/audioout.h>

#define FILELIMIT 1

struct HEADER {
    unsigned char riff[4];                      // RIFF string
    unsigned int overall_size   ;               // overall size of file in bytes
    unsigned char wave[4];                      // WAVE string
    unsigned char fmt_chunk_marker[4];          // fmt string with trailing null char
    unsigned int length_of_fmt;                 // length of the format data
    unsigned int format_type;                   // format type. 1-PCM, 3- IEEE float, 6 - 8bit A law, 7 - 8bit mu law
    unsigned int channels;                      // no.of channels
    unsigned int sample_rate;                   // sampling rate (blocks per second)
    unsigned int byterate;                      // SampleRate * NumChannels * BitsPerSample/8
    unsigned int block_align;                   // NumChannels * BitsPerSample/8
    unsigned int bits_per_sample;               // bits per sample, 8- 8bits, 16- 16 bits etc
    unsigned char data_chunk_header [4];        // DATA string or FLLR string
    unsigned int data_size;                     // NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk that will be read
    unsigned long num_samples;
};

#define ALIGN(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

int shellinit = 0;
struct HEADER header;

static int offset(void *mem_base, char *needle) {
	void *mem = mem_base;
	while(1) {
		if(((char*)mem)[0] == (char)needle[0]) {
			for(int i = 0; i < strlen(needle); i++) {
				if(((char*)mem)[i] != (char)needle[i]) {
					mem += 1;
					continue;
				}
			}
			return mem - mem_base;
		} else mem += 1;
	}
}

static void playSound(void *mem_base) {
		int sample_len;
		int count = 0;

		if(ALIGN(header.num_samples, 64) < SCE_AUDIO_MAX_LEN) sample_len = ALIGN(header.num_samples, 64);
		else sample_len = SCE_AUDIO_MAX_LEN;

		void *temp_mem = mem_base;

		int channel;
		if(header.channels > 2 || header.channels == 0) {
			sceClibPrintf("header channel too high or too low: %i\n", header.channels);
			return;
		} else if(header.channels == 1) channel = SCE_AUDIO_OUT_MODE_MONO;
		else channel = SCE_AUDIO_OUT_MODE_STEREO;

		while(count < ALIGN(header.num_samples, 64)) {
		sceClibPrintf("temporary variables start: %i %i %i %i\n", temp_mem, count, sample_len, ALIGN(header.num_samples, 64) - count);
		int port = sceAudioOutOpenPort(
			SCE_AUDIO_OUT_PORT_TYPE_BGM,
			sample_len,
			header.sample_rate,
			channel);
		sceClibPrintf("port open args: %i %i %i %i\n", ALIGN(header.num_samples, 64), header.sample_rate, header.channels == 2? SCE_AUDIO_OUT_MODE_STEREO : SCE_AUDIO_OUT_MODE_MONO, sample_len);
		if (port < 0) { 
			sceClibPrintf("port open error 0x%x\n", port);
			return;
		}
		int ret;
		ret = sceAudioOutOutput(port, temp_mem);
		sceClibPrintf("out1 0x%x\n", ret);
		ret = sceAudioOutOutput(port, NULL);
		sceClibPrintf("out2 0x%x\n", ret);
		ret = sceAudioOutReleasePort(port);
		sceClibPrintf("release port: 0x%x\n", ret);
		count += sample_len;
		temp_mem += sample_len * (header.channels * header.bits_per_sample) / 8;
		if(ALIGN(header.num_samples, 64) == count) sceClibPrintf("pcm play ended\n");
		else if(ALIGN(header.num_samples, 64) - count >= SCE_AUDIO_MAX_LEN) sample_len = SCE_AUDIO_MAX_LEN;
	    else sample_len = ALIGN(header.num_samples, 64) - count;
	    sceClibPrintf("temporary variables end: %i %i %i %i\n", temp_mem, count, sample_len, ALIGN(header.num_samples, 64) - count);
	}
	return;
}

int bootsoundThread(SceSize argc, void* argv) {

	while(!shellinit) {
		sceClibPrintf("bootsound, wait for shell init\n");
		sceKernelDelayThread(1000 * 1000 * 1);
	}
	sceKernelDelayThread(1000 * 1000 * 1);

	sceClibPrintf("bootsound shell init detected\n");

	SceIoStat stat;

	void *wav_addr = NULL;

	int stat_ret, uid;
	stat_ret = sceIoGetstat("ur0:tai/bootsound.wav", &stat);

	if((stat_ret < 0) || ((uint32_t)stat.st_size > FILELIMIT * 0x100000) || (SCE_SO_ISDIR(stat.st_mode) != 0)) {
		sceClibPrintf("file load issue\n");
		return 0;
	}

	sceClibPrintf("file check okay!\n");

	if((uid = sceKernelAllocMemBlock("bootsound", 0x0C208060, FILELIMIT * 0x100000, NULL)) < 0) {
		sceClibPrintf("memory allocation failed with a size of 0x%x %i\n", (uint32_t)stat.st_size, (uint32_t)stat.st_size);
		return 0;
	}

	sceClibPrintf("memory allocation okay with a size of 0x%x %i\n", (uint32_t)stat.st_size, (uint32_t)stat.st_size);

	sceKernelGetMemBlockBase(uid, (void**)&wav_addr);

	SceUID fd = sceIoOpen("ur0:tai/bootsound.wav", SCE_O_RDONLY, 0);
	if (fd < 0) {
		sceClibPrintf("file check failed\n");
		return 0;
	}
	int read = sceIoRead(fd, wav_addr, (uint32_t)stat.st_size);
	if (read < (uint32_t)stat.st_size) {
		sceClibPrintf("read into mem failed\n");
		return 0;
	} else sceClibPrintf("wav read into memory with size of %i\n", read);	
	sceIoClose(fd);

 	memcpy(header.riff, wav_addr, sizeof(header.riff));
 	sceClibPrintf("(1-4): %s \n", header.riff);

	unsigned char buffer4[4];
	unsigned char buffer2[2];

	memcpy(buffer4, wav_addr + 4,sizeof(buffer4));
 	header.overall_size  = buffer4[0] | (buffer4[1]<<8) | (buffer4[2]<<16) | (buffer4[3]<<24);
 	sceClibPrintf("(5-8) Overall size: bytes:%u, Kb:%u \n", header.overall_size, header.overall_size/1024);

 	memcpy(header.wave, wav_addr + 8, sizeof(header.wave));
 	sceClibPrintf("(9-12) Wave marker: %s\n", header.wave);

 	memcpy(header.fmt_chunk_marker, wav_addr + 12, sizeof(header.fmt_chunk_marker));
 	sceClibPrintf("(13-16) Fmt marker: %s\n", header.fmt_chunk_marker);

 	memcpy(buffer4, wav_addr + 16, sizeof(buffer4));
 	header.length_of_fmt = buffer4[0] | (buffer4[1] << 8) | (buffer4[2] << 16) | (buffer4[3] << 24);
 	sceClibPrintf("(17-20) Length of Fmt header: %u \n", header.length_of_fmt);

	memcpy(buffer2, wav_addr + 20,sizeof(buffer2)); 
 	header.format_type = buffer2[0] | (buffer2[1] << 8);
 	char format_name[10] = "";
 	if (header.format_type == 1) strcpy(format_name,"PCM");
 	else if (header.format_type == 6) strcpy(format_name, "A-law");
 	else if (header.format_type == 7) strcpy(format_name, "Mu-law");
 	sceClibPrintf("(21-22) Format type: %u %s \n", header.format_type, format_name);

 	memcpy(buffer2, wav_addr + 22, sizeof(buffer2));
 	header.channels = buffer2[0] | (buffer2[1] << 8);
 	sceClibPrintf("(23-24) Channels: %u \n", header.channels);

 	memcpy(buffer4, wav_addr + 24, sizeof(buffer4));
 	header.sample_rate = buffer4[0] | (buffer4[1] << 8) | (buffer4[2] << 16) | (buffer4[3] << 24);
 	sceClibPrintf("(25-28) Sample rate: %u\n", header.sample_rate);

 	memcpy(buffer4, wav_addr + 28, sizeof(buffer4));
 	header.byterate  = buffer4[0] | (buffer4[1] << 8) | (buffer4[2] << 16) | (buffer4[3] << 24);
 	sceClibPrintf("(29-32) Byte Rate: %u , Bit Rate:%u\n", header.byterate, header.byterate*8);

 	memcpy(buffer2, wav_addr + 32, sizeof(buffer2));
 	header.block_align = buffer2[0] | (buffer2[1] << 8);
 	sceClibPrintf("(33-34) Block Alignment: %u \n", header.block_align);

 	memcpy(buffer2, wav_addr + 34, sizeof(buffer2));
 	header.bits_per_sample = buffer2[0] | (buffer2[1] << 8);
 	sceClibPrintf("(35-36) Bits per sample: %u \n", header.bits_per_sample);

 	int dataOffset = offset(wav_addr, "data");
 	memcpy(header.data_chunk_header, wav_addr + dataOffset, sizeof(header.data_chunk_header));
 	sceClibPrintf("Data Marker: %s \n", header.data_chunk_header);

 	memcpy(buffer4, wav_addr + dataOffset + 4, sizeof(buffer4));
 	header.data_size = buffer4[0] | (buffer4[1] << 8) | (buffer4[2] << 16) | (buffer4[3] << 24 );
 	sceClibPrintf("Size of data chunk: %u \n", header.data_size);

 	header.num_samples = (8 * header.data_size) / (header.channels * header.bits_per_sample);
 	sceClibPrintf("Number of samples:%lu \n", header.num_samples);

 	long size_of_each_sample = (header.channels * header.bits_per_sample) / 8;
 	sceClibPrintf("Size of each sample:%ld bytes\n", size_of_each_sample);

 	float duration_in_seconds = (float) header.overall_size / header.byterate;
 	sceClibPrintf("Approx.Duration in seconds=%f\n", duration_in_seconds);

 	playSound(wav_addr + dataOffset + 8);

    sceKernelFreeMemBlock(uid);
    sceClibPrintf("release memory okay!\n");

	return 1;
}

static SceUID bridge_hook = -1;
static tai_hook_ref_t bridge_ref;
int bridge_patched(int a1) {
	shellinit = 1;
	sceClibPrintf("shell initialization complete\n");
	return TAI_CONTINUE(int, bridge_ref, a1);
}

void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args){
		sceClibPrintf("\nbootsound by Team CBPS\n");

		// vshKernelSendSysEvent
		bridge_hook = taiHookFunctionImport(&bridge_ref,
		TAI_MAIN_MODULE,
		TAI_ANY_LIBRARY,
		0x71D9DB5C,
		bridge_patched);
		sceClibPrintf("bridge_hook = %i\n", bridge_hook);

		SceUID thid = sceKernelCreateThread("bootsound_thread", bootsoundThread, 191, 0x4000, 0, 0, NULL);
		sceKernelStartThread(thid, 0, NULL);
		sceClibPrintf("bootsound_thread started\n");

		return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args){
	if (bridge_hook >= 0) taiHookRelease(bridge_hook, bridge_ref);
	return SCE_KERNEL_STOP_SUCCESS;
}
