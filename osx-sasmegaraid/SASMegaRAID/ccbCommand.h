#define super IOCommand

class mraid_ccbCommand: public IOCommand {
    OSDeclareDefaultStructors(mraid_ccbCommand);
private:
    typedef void                    (*ccb_done_ptr)(mraid_ccbCommand *);    
public:
    struct st {
    union mraid_frame               *ccb_frame;
    
    void                            *ccb_cookie;
    
    ccb_done_ptr                    ccb_done;
    
    UInt8                           ccb_direction;
#define MRAID_DATA_NONE	(1<<0)
#define MRAID_DATA_IN   (1<<1)
#define MRAID_DATA_OUT	(1<<2)
#define MRAID_CMD_POLL  (1<<3)
    
    UInt32                          ccb_frame_size;
    UInt32                          ccb_extra_frames;
    
    union mraid_sgl                 *ccb_sgl;
    struct mraid_sgl_mem            ccb_sglmem;
public:
    u_long                          ccb_pframe;
    u_long                          ccb_pframe_offset;
    
    struct mraid_sense              *ccb_sense;
    u_long                          ccb_psense;
    } s;

    void scrubCommand() {
        s.ccb_frame->mrr_header.mrh_cmd_status = 0x0;
        s.ccb_frame->mrr_header.mrh_flags = 0x0;
        
        s.ccb_cookie = NULL;
        s.ccb_done = NULL;
        s.ccb_direction = 0;
        s.ccb_frame_size = 0;
        s.ccb_extra_frames = 0;
        s.ccb_sgl = NULL;
        
        memset(&s.ccb_sglmem, '\0', sizeof(struct mraid_sgl_mem));
    }
    void initCommand() {
        memset(&s, '\0', sizeof(struct st));
    }
protected:
    virtual bool init() {
        if(!super::init())
            return false;
        
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