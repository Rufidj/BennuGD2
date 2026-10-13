/* Link stub for the console's libSceVideodec2 (libmod_video's hardware decoder). Only linked against, never run:
 * the console's own library answers these calls once it is loaded with sceSysmoduleLoadModule(207).
 * Build with ps5/tools/build-videodec2-stub.sh. */
int sceVideodec2QueryComputeMemoryInfo(void *a) { (void)a; return -1; }
int sceVideodec2AllocateComputeQueue(const void *a, const void *b, void **c) { (void)a; (void)b; (void)c; return -1; }
int sceVideodec2QueryDecoderMemoryInfo(const void *a, void *b) { (void)a; (void)b; return -1; }
int sceVideodec2CreateDecoder(const void *a, const void *b, void **c) { (void)a; (void)b; (void)c; return -1; }
int sceVideodec2DeleteDecoder(void *a) { (void)a; return -1; }
int sceVideodec2Decode(void *a, const void *b, void *c, void *d) { (void)a; (void)b; (void)c; (void)d; return -1; }
int sceVideodec2Flush(void *a, void *b, void *c) { (void)a; (void)b; (void)c; return -1; }
int sceVideodec2Reset(void *a) { (void)a; return -1; }
