/* Link stub for the console's libSceCommonDialog (the shared dialog subsystem the on-screen keyboard rides on, see
 * libmod_misc/m_ime.c). Only linked against, never run: the console's own library answers these calls.
 * Build with ps5/tools/build-commondialog-stub.sh. */
int sceCommonDialogInitialize(void) { return -1; }
int sceCommonDialogIsUsed(void) { return 0; }
