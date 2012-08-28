#ifndef AESCAN_H
#define AESCAN_H

extern void SMessageReceived(C_word, BMessage *);

___abstract class Proto
{
public:
virtual void MessageReceived(BMessage* ) = 0;
virtual void ReadyToRun() { be_app->PostMessage(B_QUIT_REQUESTED); }
};
class AEScan: public BApplication
{
public:
AEScan(const char *sig) : BApplication(sig) {}
~AEScan() { be_app_messenger.SendMessage(B_QUIT_REQUESTED); }
void MessageReceived(BMessage *m) { SMessageReceived(CHICKEN_gc_root_ref(this), m); }
};

// !
static long RunProxy(void *_this) {
        AEScan *scanner = (AEScan *) _this;

        // signal(SIGINT, RunThreadQuit); // Handle C-c, do i really need this?
        scanner->Lock();
        return scanner->Run();
}
static long SpawnThreadProxy(const char *s, long p, AEScan *o) {
        return spawn_thread(RunProxy /* ! */, s, p, o);
}

#endif
