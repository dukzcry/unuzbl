#define super IOCommand

enum state {
#define MRAID_CCB_F_ERR   (1 << 0)
    MRAID_CCB_FREE,
};
class mraid_ccbCommand: public IOCommand {
    OSDeclareDefaultStructors(mraid_ccbCommand);
public:
    mraid_frame_header              ccb_frame_header;
    typedef void                    (*ccb_done_ptr)(mraid_ccbCommand *);
    UInt32                          ccb_frame_size;
    UInt32                          ccb_extra_frames;
    
    union mraid_sgl                 *ccb_sgl;
    
    UInt8                           ccb_direction;
#define MRAID_DATA_NONE	(1<<0)
#define MRAID_DATA_IN   (1<<1)
#define MRAID_DATA_OUT	(1<<2)
#define MRAID_CMD_POLL  (1<<3)
    
    void                            *ccb_cookie;
    /* Do not optimize */
    volatile state                  ccb_state;

    int                             ccb_num; /* Debug */
    
    union mraid_frame               *ccb_frame;
    u_long                          ccb_pframe;
    u_long                          ccb_pframe_offset;
    
    struct mraid_sense              *ccb_sense;
    u_long                          ccb_psense;
    
    UInt32                          ccb_flags;
    ccb_done_ptr                    ccb_done;
    
    struct mraid_sgl_mem            ccb_sglmem;

    void initCommand() {
        ccb_frame_header.mrh_cmd_status = 0x0;
        ccb_frame_header.mrh_flags = 0x0;
        
        ccb_state = MRAID_CCB_FREE;
        ccb_cookie = NULL;
        ccb_flags = 0;
        ccb_done = NULL;
        ccb_direction = 0;
        ccb_frame_size = 0;
        ccb_extra_frames = 0;
        ccb_sgl = NULL;
        
        memset(&ccb_sglmem, '\0', sizeof(struct mraid_sgl_mem));
    }
protected:
    virtual bool init() {
        if(!super::init())
            return false;
        
        initCommand();
        
        return true;
    }
    virtual void free() {
        FreeSGL(&ccb_sglmem);
        
        super::free();
    }
public:
    static mraid_ccbCommand *NewCommand() {
        mraid_ccbCommand *me = new mraid_ccbCommand;
        
        return me;
    }
};