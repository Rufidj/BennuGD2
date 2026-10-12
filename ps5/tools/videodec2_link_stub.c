/* Link stub for the console's libSceVideodec2 (used by libmod_video's hardware decoder).
 * Only linked against, never run: the console's own library answers these calls once it is loaded
 * with sceSysmoduleLoadModule(207).
 * Build: see ps5/tools/build-videodec2-stub.sh */
int sceVideodec2QueryComputeMemoryInfo(void *info) { (void)info; return -1; }
int sceVideodec2AllocateComputeQueue(const void *config, const void *memory, void **queue) { (void)config; (void)memory; (void)queue; return -1; }
int sceVideodec2ReleaseComputeQueue(void *queue) { (void)queue; return -1; }
int sceVideodec2QueryDecoderMemoryInfo(const void *config, void *memory) { (void)config; (void)memory; return -1; }
int sceVideodec2CreateDecoder(const void *config, const void *memory, void **decoder) { (void)config; (void)memory; (void)decoder; return -1; }
int sceVideodec2DeleteDecoder(void *decoder) { (void)decoder; return -1; }
int sceVideodec2Decode(void *decoder, const void *input, void *frame, void *output) { (void)decoder; (void)input; (void)frame; (void)output; return -1; }
int sceVideodec2Flush(void *decoder, void *frame, void *output) { (void)decoder; (void)frame; (void)output; return -1; }
int sceVideodec2Reset(void *decoder) { (void)decoder; return -1; }
int sceVideodec2GetPictureInfo(const void *output, void *first, void *second) { (void)output; (void)first; (void)second; return -1; }
