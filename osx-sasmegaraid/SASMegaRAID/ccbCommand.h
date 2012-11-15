#define super IOCommand

class mraid_ccbCommand: public IOCommand {
    OSDeclareDefaultStructors(mraid_ccbCommand);
private:
    typedef void                    (*ccb_done_ptr)(mraid_ccbCommand *);    
public:
    struct st {
    mraid_frame               *ccb_frame;
    
    struct {
        IOLock                      *holder;
        bool                        event;
    } ccb_lock;
    
    ccb_done_ptr                    ccb_done;
    
    UInt8                           ccb_direction;
#define MRAID_DATA_NONE	0
#define MRAID_DATA_IN   1
#define MRAID_DATA_OUT	2
    
    UInt32                          ccb_frame_size;
    UInt32                          ccb_extra_frames;
    
    mraid_sgl                 *ccb_sgl;
    mraid_sgl_mem                   ccb_sglmem;
public:
    u_long                          ccb_pframe;
    u_long                          ccb_pframe_offset;
    
    mraid_sense                     *ccb_sense;
    u_long                          ccb_psense;
    } s;

    void scrubCommand() {
        s.ccb_frame->mrr_header.mrh_cmd_status = 0x0;
        s.ccb_frame->mrr_header.mrh_flags = 0x0;
        
        s.ccb_lock.event = false;
        s.ccb_done = NULL;
        s.ccb_direction = 0;
        s.ccb_frame_size = 0;
        s.ccb_extra_frames = 0;
        s.ccb_sgl = NULL;
        
        memset(&s.ccb_sglmem, '\0', sizeof(mraid_sgl_mem));
    }
    void initCommand() {
        memset(&s, '\0', sizeof(struct st));
    }
protected:
    virtual bool init() {
        if(!super::init())
            return false;
        
        initCommand();
        
        return true;
    }
    virtual void free() {
        FreeSGL(&s.ccb_sglmem);
        
        super::free();
    }
public:
    static mraid_ccbCommand *NewCommand() {
        mraid_ccbCommand *me = new mraid_ccbCommand;
        
        return me;
    }
};