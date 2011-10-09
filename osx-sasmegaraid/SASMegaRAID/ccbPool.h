#ifndef _CCBP_
#define _CCBP_

#include <IOKit/IOCommand.h>
#define super IOCommand

#include "Various.h"

enum state {
    MRAID_CCB_FREE,
    MRAID_CCB_READY,
    MRAID_CCB_DONE
};
class mraid_ccbCommand: public IOCommand {
    OSDeclareDefaultStructors(mraid_ccbCommand);
    
    struct mraid_softc              *ccb_sc;
    mraid_iokitframe_header         *ccb_frame_header;
    //union mraid_frame               *ccb_frame;
    u_long                          ccb_pframe;
    u_long                          ccb_pframe_offset;
    UInt32                          ccb_frame_size;
    UInt32                          ccb_extra_frames;
    
    struct mraid_sense              *ccb_sense;
    u_long                          ccb_psense;
    
    //bus_dmamap_t                 ccb_dmamap;
    
    union mraid_sgl                 *ccb_sgl;
    
    /* Data for sgl */
    IOMemoryDescriptor              *ccb_data;
    UInt64                           ccb_len;
    
    UInt8                           ccb_direction;
#define MRAID_DATA_NONE 0
#define MRAID_DATA_IN   1
#define MRAID_DATA_OUT  2
    
    void                            *ccb_cookie;
    typedef void                    (*ccb_done_ptr)(struct mraid_ccb *);
public:
    ccb_done_ptr                    ccb_done;
private:
    /* Do not optimize */
    volatile state                  ccb_state;
    UInt32                          ccb_flags;
#define MRAID_CCB_F_ERR   (1 << 0)
public:    
    void initCommand() {
        ccb_frame_header->mrh_cmd_status = 0x0;
        ccb_state = MRAID_CCB_FREE;
        ccb_done = NULL;
        ccb_direction = 0;
        ccb_frame_size = 0;
        ccb_extra_frames = 0;
        ccb_sgl = NULL;
        ccb_data = NULL;
        ccb_len = 0;
    }
protected:
    virtual bool init() {
        if(!super::init())
            return false;
        
        initCommand();
        
        return true;
    }
    virtual void free() { NRelease(ccb_data); super::free(); }
public:
    UInt8 getCmdStatus(void) {
        return ccb_frame_header->mrh_cmd_status;
    }
    void setCmdStatus(UInt8 in_cmd_status) {
        ccb_frame_header->mrh_cmd_status = in_cmd_status;
    }
    
    state getState(void) {
        return ccb_state;
    }
    void setState(state in_ccb_state) {
        ccb_state = in_ccb_state;
    }
    
    void setDone(ccb_done_ptr ccb_done_func) {
        ccb_done = ccb_done_func;
    }
    
    UInt8 getDirection(void) {
        return ccb_direction;
    }
    void setDirection(UInt8 in_ccb_direction) {
        ccb_direction = in_ccb_direction;
    }
    
    UInt32 getFrameSize(void) {
        return ccb_frame_size;
    }
    void setFrameSize(UInt32 in_ccb_frame_size) {
        ccb_frame_size = in_ccb_frame_size;
    }
    
    UInt32 getExtraFrames(void) {
        return ccb_extra_frames;
    }
    void setExtraFrames(UInt32 in_ccb_extra_frames) {
        ccb_frame_size = in_ccb_extra_frames;
    }
    
    mraid_sgl *getsgl(void) {
        return ccb_sgl;
    }
    void setsgl(mraid_sgl *in_ccb_sgl) {
        ccb_sgl = in_ccb_sgl;
    }
    
    void setData(IOMemoryDescriptor *in_ccb_data) {
        ccb_data = in_ccb_data;
    }
    IOMemoryDescriptor* getData(void) {
        return ccb_data;
    }
    
    void setLen(UInt64 in_ccb_len) {
        ccb_len = in_ccb_len;
    }
    UInt64 getLen(void) {
        return ccb_len;
    }
};

#endif /* ifndef */