/*
 * Copyright (c) 2005, Adaptive Digital Technologies, Inc.
 *
 * File Name: GpakApi.c
 *
 * Description:
 *   This file contains user API functions to communicate with DSPs executing
 *   G.PAK software. The file is integrated into the host processor connected
 *   to C55X G.PAK DSPs via a Host Port Interface.
 *
 * Version: 1.0
 *
 * Revision History:
 *   06/15/05 - Initial release.
 *   11/15/2006  - 24 TDM-TDM Channels EC release
 *
 * This program has been released under the terms of the GPL version 2 by
 * permission of Adaptive Digital Technologies, Inc.
 *
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#include "GpakHpi.h"
#include "GpakCust.h"
#include "GpakApi.h"
#include "gpakenum.h"

/* DSP to Host interface block offsets. */
#define REPLY_MSG_PNTR_OFFSET 0     /* I/F blk offset to Reply Msg Pointer */
#define CMD_MSG_PNTR_OFFSET 2       /* I/F blk offset to Command Msg Pointer */
#define EVENT_MSG_PNTR_OFFSET 4     /* I/F blk offset to Event Msg Pointer */
#define PKT_BUFR_MEM_OFFSET 6       /* I/F blk offset to Packet Buffer memory */
#define DSP_STATUS_OFFSET 8         /* I/F blk offset to DSP Status */
#define VERSION_ID_OFFSET 9         /* I/F blk offset to G.PAK Version Id */
#define MAX_CMD_MSG_LEN_OFFSET 10   /* I/F blk offset to Max Cmd Msg Length */
#define CMD_MSG_LEN_OFFSET 11       /* I/F blk offset to Command Msg Length */
#define REPLY_MSG_LEN_OFFSET 12     /* I/F blk offset to Reply Msg Length */
#define NUM_CHANNELS_OFFSET 13      /* I/F blk offset to Num Built Channels */
#define NUM_PKT_CHANNELS_OFFSET 14  /* I/F blk offset to Num Pkt Channels */
#define NUM_CONFERENCES_OFFSET 15   /* I/F blk offset to Num Conferences */
//#define CPU_USAGE_OFFSET_1MS 16     /* I/F blk offset to CPU Usage statistics */
#define CPU_USAGE_OFFSET 18         /* I/F blk offset to CPU Usage statistics */
//#define CPU_USAGE_OFFSET_10MS 20    /* I/F blk offset to CPU Usage statistics */
#define FRAMING_STATS_OFFSET 22     /* I/F blk offset to Framing statistics */

//#define GPAK_RELEASE_Rate rate10ms
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = 
// Macro to reconstruct a 32-bit value from two 16-bit values.
// Parameter p32: 32-bit-wide destination
// Parameter p16: 16-bit-wide source array of length 2 words
#define RECONSTRUCT_LONGWORD(p32, p16) p32 = (DSP_ADDRESS)p16[0]<<16; \
								       p32 |= (unsigned long)p16[1]
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = 

/* DSP Status value definitions. */
#define DSP_INIT_STATUS 0x5555		/* DSP Initialized status value */
#define HOST_INIT_STATUS 0xAAAA		/* Host Initialized status value */

/* Circular packet buffer information structure offsets. */
#define CB_BUFR_BASE 0              /* pointer to base of circular buffer */
#define CB_BUFR_SIZE 2              /* size of buffer (words) */
#define CB_BUFR_PUT_INDEX 3         /* offset in buffer for next write */
#define CB_BUFR_TAKE_INDEX 4        /* offset in buffer for next read */
#define CIRC_BUFFER_INFO_STRUCT_SIZE 6 

/* Miscellaneous definitions. */
#define MSG_BUFFER_SIZE 100 /* size (words) of Host msg buffer */
#define WORD_BUFFER_SIZE 84  /* size of DSP Word buffer (words) */

#ifdef __TMS320C55XX__ // debug sections if not on host
#pragma DATA_SECTION(pDspIfBlk,"GPAKAPIDEBUG_SECT")
#pragma DATA_SECTION(MaxCmdMsgLen,"GPAKAPIDEBUG_SECT")
#pragma DATA_SECTION(MaxChannels,"GPAKAPIDEBUG_SECT")
#pragma DATA_SECTION(DlByteBufr,"GPAKAPIDEBUG_SECT")
#pragma DATA_SECTION(DlWordBufr,"GPAKAPIDEBUG_SECT")
#pragma DATA_SECTION(pEventFifoAddress,"GPAKAPIDEBUG_SECT")
#endif

/* Host variables related to Host to DSP interface. */
static DSP_ADDRESS pDspIfBlk[MAX_DSP_CORES];    /* DSP address of I/F block */
static DSP_WORD MaxCmdMsgLen[MAX_DSP_CORES]; /* max Cmd msg length (octets) */
static unsigned short int MaxChannels[MAX_DSP_CORES];    /* max num channels */

//static unsigned short int MaxPktChannels[MAX_DSP_CORES]; /* max num pkt channels */
//static unsigned short int MaxConfs[MAX_DSP_CORES];    /* max num conferences */
//static DSP_ADDRESS pPktInBufr[MAX_DSP_CORES][MAX_PKT_CHANNELS];  /* Pkt In buffer */
//static DSP_ADDRESS pPktOutBufr[MAX_DSP_CORES][MAX_PKT_CHANNELS]; /* Pkt Out buffer */
static DSP_ADDRESS pEventFifoAddress[MAX_DSP_CORES]; /* event fifo */

static unsigned char DlByteBufr[DOWNLOAD_BLOCK_SIZE * 2]; /* Dowload byte buf */
static DSP_WORD DlWordBufr[DOWNLOAD_BLOCK_SIZE];      /* Dowload word buffer */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * CheckDspReset - Check if the DSP was reset.
 *
 * FUNCTION
 *  This function determines if the DSP was reset and is ready. If reset
 *  occurred, it reads interface parameters and calculates DSP addresses.
 *
 * RETURNS
 *  -1 = DSP is not ready.
 *   0 = Reset did not occur.
 *   1 = Reset occurred.
 *
 */
static int __CheckDspReset(
    int DspId               /* DSP Identifier (0 to MaxDSPCores-1) */
    )
{
    DSP_ADDRESS IfBlockPntr; /* Interface Block pointer */
    DSP_WORD DspStatus;      /* DSP Status */
    DSP_WORD DspChannels;    /* number of DSP channels */
    DSP_WORD  Temp[2];

    /* Read the pointer to the Interface Block. */
    gpakReadDspMemory(DspId, DSP_IFBLK_ADDRESS, 2, Temp);
    RECONSTRUCT_LONGWORD(IfBlockPntr, Temp);

    /* If the pointer is zero, return with an indication the DSP is not
       ready. */
    if (IfBlockPntr == 0)
        return (-1);

    /* Read the DSP's Status. */
    gpakReadDspMemory(DspId, IfBlockPntr + DSP_STATUS_OFFSET, 1, &DspStatus);

    /* If status indicates the DSP was reset, read the DSP's interface
       parameters and calculate DSP addresses. */
    if (DspStatus == DSP_INIT_STATUS ||
        ((DspStatus == HOST_INIT_STATUS) && (pDspIfBlk[DspId] == 0))) 
    {
        /* Save the address of the DSP's Interface Block. */
        pDspIfBlk[DspId] = IfBlockPntr;

        /* Read the DSP's interface parameters. */
        gpakReadDspMemory(DspId, IfBlockPntr + MAX_CMD_MSG_LEN_OFFSET, 1,
                          &(MaxCmdMsgLen[DspId]));

        /* read the number of configured DSP channels */
        gpakReadDspMemory(DspId, IfBlockPntr + NUM_CHANNELS_OFFSET, 1,
                          &DspChannels);
        if (DspChannels > MAX_CHANNELS)
            MaxChannels[DspId] = MAX_CHANNELS;
        else
            MaxChannels[DspId] = (unsigned short int) DspChannels;

        /* read the pointer to the event fifo info struct */
        gpakReadDspMemory(DspId, IfBlockPntr + EVENT_MSG_PNTR_OFFSET, 2, Temp);
        RECONSTRUCT_LONGWORD(pEventFifoAddress[DspId], Temp);

        /* Set the DSP Status to indicate the host recognized the reset. */
        DspStatus = HOST_INIT_STATUS;
        gpakWriteDspMemory(DspId, IfBlockPntr + DSP_STATUS_OFFSET, 1,
                           &DspStatus);

        /* Return with an indication that a reset occurred. */
        return (1);
    }

    /* If status doesn't indicate the host recognized a reset, return with an
       indication the DSP is not ready. */
    if ((DspStatus != HOST_INIT_STATUS) || (pDspIfBlk[DspId] == 0))
        return (-1);

    /* Return with an indication that a reset did not occur. */
    return (0);
}

static int CheckDspReset(
    int DspId               /* DSP Identifier (0 to MaxDSPCores-1) */
    )
{
	int ret;
	int retries = 20;
	while (--retries) {
		ret = __CheckDspReset(DspId);
		if (-1 != ret)
			return ret;
		msleep(5);
	}
	return ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * WriteDspCmdMessage - Write a Host Command/Request message to DSP.
 *
 * FUNCTION
 *  This function writes a Host Command/Request message into DSP memory and
 *  informs the DSP of the presence of the message.
 *
 * RETURNS
 *  -1 = Unable to write message (msg len or DSP Id invalid or DSP not ready)
 *   0 = Temporarily unable to write message (previous Cmd Msg busy)
 *   1 = Message written successfully
 *
 */
static int WriteDspCmdMessage(
    int DspId,                      /* DSP Identifier (0 to MaxDSPCores-1) */
    DSP_WORD *pMessage,          /* pointer to Command message */
    DSP_WORD MsgLength              /* length of message (octets) */
    )
{
    DSP_WORD CmdMsgLength;      /* current Cmd message length */
    DSP_WORD Temp[2];
    DSP_ADDRESS BufferPointer;     /* message buffer pointer */

    /* Check if the DSP was reset and is ready. */
    if (CheckDspReset(DspId) == -1)
        return (-1);

    /* Make sure the message length is valid. */
    if ((MsgLength < 1) || (MsgLength > MaxCmdMsgLen[DspId]))
        return (-1);

    /* Make sure a previous Command message is not in use by the DSP. */
    gpakReadDspMemory(DspId, pDspIfBlk[DspId] + CMD_MSG_LEN_OFFSET, 1,
                      &CmdMsgLength);
    if (CmdMsgLength != 0)
        return (0);

    /* Purge any previous Reply message that wasn't read. */
    gpakWriteDspMemory(DspId, pDspIfBlk[DspId] + REPLY_MSG_LEN_OFFSET, 1,
                       &CmdMsgLength);

    /* Copy the Command message into DSP memory. */
    gpakReadDspMemory(DspId, pDspIfBlk[DspId] + CMD_MSG_PNTR_OFFSET, 2, Temp);
    RECONSTRUCT_LONGWORD(BufferPointer, Temp);
    gpakWriteDspMemory(DspId, BufferPointer, (MsgLength + 1) / 2, pMessage);

    /* Store the message length in DSP's Command message length (flags DSP that
       a Command message is ready). */
    CmdMsgLength = MsgLength;
    gpakWriteDspMemory(DspId, pDspIfBlk[DspId] + CMD_MSG_LEN_OFFSET, 1,
                       &CmdMsgLength);

    /* Return with an indication the message was written. */
    return (1);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * ReadDspReplyMessage - Read a DSP Reply message from DSP.
 *
 * FUNCTION
 *  This function reads a DSP Reply message from DSP memory.
 *
 * RETURNS
 *  -1 = Unable to write message (msg len or DSP Id invalid or DSP not ready)
 *   0 = No message available (DSP Reply message empty)
 *   1 = Message read successfully (message and length stored in variables)
 *
 */
static int ReadDspReplyMessage(
    int DspId,                      /* DSP Identifier (0 to MaxDSPCores-1) */
    DSP_WORD *pMessage,             /* pointer to Reply message buffer */
    DSP_WORD *pMsgLength            /* pointer to msg length var (octets) */
    )
{
    DSP_WORD MsgLength;         /* message length */
    DSP_ADDRESS BufferPointer;  /* message buffer pointer */
    DSP_WORD Temp[2];

    /* Check if the DSP was reset and is ready. */
    if (CheckDspReset(DspId) == -1)
        return (-1);

    /* Check if a Reply message is ready. */
    gpakReadDspMemory(DspId, pDspIfBlk[DspId] + REPLY_MSG_LEN_OFFSET, 1,
                      &MsgLength);
    if (MsgLength == 0)
        return (0);

    /* Make sure the message length is valid. */
    if (MsgLength > *pMsgLength)
        return (-1);

    /* Copy the Reply message from DSP memory. */
    gpakReadDspMemory(DspId, pDspIfBlk[DspId] + REPLY_MSG_PNTR_OFFSET, 2, Temp);
    RECONSTRUCT_LONGWORD(BufferPointer, Temp);
    gpakReadDspMemory(DspId, BufferPointer, (MsgLength + 1) / 2, pMessage);

    /* Store the message length in the message length variable. */
    *pMsgLength = MsgLength;

    /* Indicate a Reply message is not ready. */
    MsgLength = 0;
    gpakWriteDspMemory(DspId, pDspIfBlk[DspId] + REPLY_MSG_LEN_OFFSET, 1,
                       &MsgLength);

    /* Return with an indication the message was read. */
    return (1);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * ReadCircBuffer - Read from a DSP circular buffer.
 *
 * FUNCTION
 *  This function reads a block of words from a DSP circular buffer. The Take
 *  address is incremented by the number of words read adjusting for buffer
 *  wrap.
 *
 * RETURNS
 *  nothing
 *
 */
static void ReadCircBuffer(
    int DspId,                  /* DSP Identifier (0 to MaxDSPCores-1) */
    DSP_ADDRESS BufrBaseAddress,   /* address of base of circular buffer */
    DSP_ADDRESS BufrLastAddress,   /* address of last word in buffer */
    DSP_ADDRESS *TakeAddress,      /* pointer to address in buffer for read */
    DSP_WORD *pWordBuffer,      /* pointer to buffer for words read */
    DSP_WORD NumWords           /* number of words to read */
    )
{
    DSP_WORD WordsTillEnd;      /* number of words until end of buffer */

    /* Determine the number of words from the start address until the end of the
       buffer. */
    WordsTillEnd = BufrLastAddress - *TakeAddress + 1;

    /* If a buffer wrap will occur, read the first part at the end of the
       buffer followed by the second part at the beginning of the buffer. */
    if (NumWords > WordsTillEnd)
    {
        gpakReadDspMemory(DspId, *TakeAddress, WordsTillEnd, pWordBuffer);
        gpakReadDspMemory(DspId, BufrBaseAddress, NumWords - WordsTillEnd,
                           &(pWordBuffer[WordsTillEnd]));
        *TakeAddress = BufrBaseAddress + NumWords - WordsTillEnd;
    }

    /* If a buffer wrap will not occur, read all words starting at the current
       take address in the buffer. */
    else
    {
        gpakReadDspMemory(DspId, *TakeAddress, NumWords, pWordBuffer);
        if (NumWords == WordsTillEnd)
            *TakeAddress = BufrBaseAddress;
        else
            *TakeAddress = *TakeAddress + NumWords;
    }
    return;
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * VerifyReply - Verify the reply message is correct for the command sent.
 *
 * FUNCTION
 *  This function verifies correct reply message content for the command that
 *  was just sent.
 *
 * RETURNS
 *  0 = Incorrect
 *  1 = Correct
 *
 */
static int VerifyReply(
    DSP_WORD *pMsgBufr,     /* pointer to Reply message buffer */
    int CheckType,          /* reply check type */
    DSP_WORD CheckValue     /* reply check value */
    )
{

    /* Verify Channel or Conference Id. */
    if (CheckType == 1)
    {
        if (((pMsgBufr[1] >> 8) & 0xFF) != CheckValue)
            return (0);
    }

    /* Verify Test Mode Id. */
    else if (CheckType == 2)
    {
        if (pMsgBufr[1] != CheckValue)
            return (0);
    }

    /* Return with an indication of correct reply. */
    return (1);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * TransactCmd - Send a command to the DSP and receive it's reply.
 *
 * FUNCTION
 *  This function sends the specified command to the DSP and receives the DSP's
 *  reply.
 *
 * RETURNS
 *  Length of reply message (0 = Failure)
 *
 */
static unsigned int TransactCmd(
    int DspId,                  /* DSP Identifier (0 to MaxDSPCores-1) */
    DSP_WORD *pMsgBufr,         /* pointer to Cmd/Reply message buffer */
    DSP_WORD CmdLength,         /* length of command message (octets) */
    DSP_WORD ReplyType,         /* required type of reply message */
    DSP_WORD ReplyLength,       /* required length of reply message (octets) */
    int ReplyCheckType,         /* reply check type */
    DSP_WORD ReplyCheckValue    /* reply check value */
    )
{
    int FuncStatus;             /* function status */
    int LoopCount;              /* wait loop counter */
    DSP_WORD RcvReplyLength;    /* received Reply message length */
    DSP_WORD RcvReplyType;      /* received Reply message type code */
    DSP_WORD RetValue;          /* return value */

    /* Default the return value to indicate a failure. */
    RetValue = 0;

    /* Lock access to the DSP. */
    gpakLockAccess(DspId);

    /* Attempt to write the command message to the DSP. */
    LoopCount = 0;
    while ((FuncStatus = WriteDspCmdMessage(DspId, pMsgBufr, CmdLength)) != 1)
    {
        if (FuncStatus == -1)
            break;
        if (++LoopCount > MAX_WAIT_LOOPS)
            break;
        gpakHostDelay();
    }

    /* Attempt to read the reply message from the DSP if the command message was
       sent successfully. */
    if (FuncStatus == 1)
    {
        for (LoopCount = 0; LoopCount < MAX_WAIT_LOOPS; LoopCount++)
        {
            RcvReplyLength = MSG_BUFFER_SIZE * 2;
            FuncStatus = ReadDspReplyMessage(DspId, pMsgBufr, &RcvReplyLength);
            if (FuncStatus == 1)
            {
                RcvReplyType = (pMsgBufr[0] >> 8) & 0xFF;
                if ((RcvReplyLength >= ReplyLength) &&
                    (RcvReplyType == ReplyType) &&
                    VerifyReply(pMsgBufr, ReplyCheckType, ReplyCheckValue))
                {
                    RetValue = RcvReplyLength;
                    break;
                }
                else if (RcvReplyType == MSG_NULL_REPLY)
                    break;
            }
            else if (FuncStatus == -1)
                break;
            gpakHostDelay();
        }
    }

    /* Unlock access to the DSP. */
    gpakUnlockAccess(DspId);

    /* Return the length of the reply message (0 = failure). */
    return (RetValue);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakConfigurePorts - Configure a DSP's serial ports.
 *
 * FUNCTION
 *  This function configures a DSP's serial ports.
 *
 * RETURNS
 *  Status code indicating success or a specific error.
 *
 */
gpakConfigPortStatus_t gpakConfigurePorts(
    unsigned short int DspId,       /* DSP Id (0 to MaxDSPCores-1) */
    const GpakPortConfig_t *pPortConfig,  /* pointer to Port Config info */
    GPAK_PortConfigStat_t *pStatus  /* pointer to Port Config Status */
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (CpsInvalidDsp);

    /* Build the Configure Serial Ports message. */
    MsgBuffer[0] = MSG_CONFIGURE_PORTS << 8;
    MsgBuffer[1] = (DSP_WORD)
                   ((pPortConfig->SlotsSelect1 << 12) |
                    ((pPortConfig->FirstBlockNum1 << 8) & 0x0F00) |
                    ((pPortConfig->SecBlockNum1 << 4) & 0x00F0));
    MsgBuffer[2] = (DSP_WORD) pPortConfig->FirstSlotMask1;
    MsgBuffer[3] = (DSP_WORD) pPortConfig->SecSlotMask1;
    MsgBuffer[4] = (DSP_WORD)
                   ((pPortConfig->SlotsSelect2 << 12) |
                    ((pPortConfig->FirstBlockNum2 << 8) & 0x0F00) |
                    ((pPortConfig->SecBlockNum2 << 4) & 0x00F0));
    MsgBuffer[5] = (DSP_WORD) pPortConfig->FirstSlotMask2;
    MsgBuffer[6] = (DSP_WORD) pPortConfig->SecSlotMask2;
    MsgBuffer[7] = (DSP_WORD)
                   ((pPortConfig->SlotsSelect3 << 12) |
                    ((pPortConfig->FirstBlockNum3 << 8) & 0x0F00) |
                    ((pPortConfig->SecBlockNum3 << 4) & 0x00F0));
    MsgBuffer[8] = (DSP_WORD) pPortConfig->FirstSlotMask3;
    MsgBuffer[9] = (DSP_WORD) pPortConfig->SecSlotMask3;

    MsgBuffer[10] = (DSP_WORD)
                   (((pPortConfig->DxDelay1 << 11) & 0x0800) |
                    ((pPortConfig->RxDataDelay1 << 9) & 0x0600) |
                    ((pPortConfig->TxDataDelay1 << 7) & 0x0180) |
                    ((pPortConfig->RxClockPolarity1 << 6) & 0x0040) |
                    ((pPortConfig->TxClockPolarity1 << 5) & 0x0020) |
                    ((pPortConfig->RxFrameSyncPolarity1 << 4) & 0x0010) |
                    ((pPortConfig->TxFrameSyncPolarity1 << 3) & 0x0008) |
                    ((pPortConfig->CompandingMode1 << 1) & 0x0006) |
                    (pPortConfig->SerialWordSize1 & 0x0001));

    MsgBuffer[11] = (DSP_WORD)
                   (((pPortConfig->DxDelay2 << 11) & 0x0800) |
                    ((pPortConfig->RxDataDelay2 << 9) & 0x0600) |
                    ((pPortConfig->TxDataDelay2 << 7) & 0x0180) |
                    ((pPortConfig->RxClockPolarity2 << 6) & 0x0040) |
                    ((pPortConfig->TxClockPolarity2 << 5) & 0x0020) |
                    ((pPortConfig->RxFrameSyncPolarity2 << 4) & 0x0010) |
                    ((pPortConfig->TxFrameSyncPolarity2 << 3) & 0x0008) |
                    ((pPortConfig->CompandingMode2 << 1) & 0x0006) |
                    (pPortConfig->SerialWordSize2 & 0x0001));

    MsgBuffer[12] = (DSP_WORD)
                   (((pPortConfig->DxDelay3 << 11) & 0x0800) |
                    ((pPortConfig->RxDataDelay3 << 9) & 0x0600) |
                    ((pPortConfig->TxDataDelay3 << 7) & 0x0180) |
                    ((pPortConfig->RxClockPolarity3 << 6) & 0x0040) |
                    ((pPortConfig->TxClockPolarity3 << 5) & 0x0020) |
                    ((pPortConfig->RxFrameSyncPolarity3 << 4) & 0x0010) |
                    ((pPortConfig->TxFrameSyncPolarity3 << 3) & 0x0008) |
                    ((pPortConfig->CompandingMode3 << 1) & 0x0006) |
                    (pPortConfig->SerialWordSize3 & 0x0001));

    MsgBuffer[13] = (DSP_WORD) pPortConfig->ThirdSlotMask1;
    MsgBuffer[14] = (DSP_WORD) pPortConfig->FouthSlotMask1;
    MsgBuffer[15] = (DSP_WORD) pPortConfig->FifthSlotMask1;
    MsgBuffer[16] = (DSP_WORD) pPortConfig->SixthSlotMask1;
    MsgBuffer[17] = (DSP_WORD) pPortConfig->SevenSlotMask1;
    MsgBuffer[18] = (DSP_WORD) pPortConfig->EightSlotMask1;
    
    MsgBuffer[19] = (DSP_WORD) pPortConfig->ThirdSlotMask2;;
    MsgBuffer[20] = (DSP_WORD) pPortConfig->FouthSlotMask2;            
    MsgBuffer[21] = (DSP_WORD) pPortConfig->FifthSlotMask2;;             
    MsgBuffer[22] = (DSP_WORD) pPortConfig->SixthSlotMask2;            
    MsgBuffer[23] = (DSP_WORD) pPortConfig->SevenSlotMask2;;             
    MsgBuffer[24] = (DSP_WORD) pPortConfig->EightSlotMask2;            

    MsgBuffer[25] = (DSP_WORD) pPortConfig->ThirdSlotMask3;;
    MsgBuffer[26] = (DSP_WORD) pPortConfig->FouthSlotMask3;            
    MsgBuffer[27] = (DSP_WORD) pPortConfig->FifthSlotMask3;;             
    MsgBuffer[28] = (DSP_WORD) pPortConfig->SixthSlotMask3;            
    MsgBuffer[29] = (DSP_WORD) pPortConfig->SevenSlotMask3;;             
    MsgBuffer[30] = (DSP_WORD) pPortConfig->EightSlotMask3;            


    /* Attempt to send the Configure Serial Ports message to the DSP and receive
       it's reply. */
	if (!TransactCmd(DspId, MsgBuffer, 62, MSG_CONFIG_PORTS_REPLY, 4, 0, 0))
        return (CpsDspCommFailure);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    *pStatus = (GPAK_PortConfigStat_t) (MsgBuffer[1] & 0xFF);
    if (*pStatus == Pc_Success)
        return (CpsSuccess);
    else
        return (CpsParmError);
}
EXPORT_SYMBOL(gpakConfigurePorts);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakConfigureChannel - Configure a DSP's Channel.
 *
 * FUNCTION
 *  This function configures a DSP's Channel.
 *
 * RETURNS
 *  Status code indicating success or a specific error.
 *
 */
gpakConfigChanStatus_t gpakConfigureChannel(
    unsigned short int DspId,           /* DSP Id (0 to MaxDSPCores-1) */
    unsigned short int ChannelId,       /* Channel Id (0 to MaxChannels-1) */
    GpakChanType ChannelType,           /* Channel Type */
    GpakChannelConfig_t *pChanConfig,   /* pointer to Channel Config info */
    GPAK_ChannelConfigStat_t *pStatus   /* pointer to Channel Config Status */
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */
    DSP_WORD MsgLength;                     /* message length */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (CcsInvalidDsp);

    /* Make sure the Channel Id is valid. */
    if (ChannelId >= MaxChannels[DspId])
        return (CcsInvalidChannel);

    /* Build the Configure Channel message based on the Channel Type. */
    switch (ChannelType)
    {

    /* PCM to Packet channel type. */
    case tdmToTdm:

        MsgBuffer[2] = (DSP_WORD)
                       ((pChanConfig->PcmInPortA << 8) |
                        (pChanConfig->PcmInSlotA & 0xFF));
        MsgBuffer[3] = (DSP_WORD)
                       ((pChanConfig->PcmOutPortA << 8) |
                        (pChanConfig->PcmOutSlotA & 0xFF));

        MsgBuffer[4] = (DSP_WORD)
                       ((pChanConfig->PcmInPortB << 8) |
                        (pChanConfig->PcmInSlotB & 0xFF));
        MsgBuffer[5] = (DSP_WORD)
                       ((pChanConfig->PcmOutPortB << 8) |
                        (pChanConfig->PcmOutSlotB & 0xFF));

        MsgBuffer[6] = (DSP_WORD)
                       (
                       ((pChanConfig->FaxCngDetB <<11) & 0x0800)  |
                       ((pChanConfig->FaxCngDetA <<10) & 0x0400)  |
                       ((pChanConfig->MuteToneB << 9) & 0x0200)  |
                       ((pChanConfig->MuteToneA << 8) & 0x0100)  |
                       ((pChanConfig->FrameRate << 6)  & 0x00C0) |  
                       ((pChanConfig->ToneTypesB << 5) & 0x0020) |
                       ((pChanConfig->ToneTypesA << 4) & 0x0010) |
                       ((pChanConfig->SoftwareCompand & 3) << 2) |
                        (pChanConfig->EcanEnableB << 1) | 
                        (pChanConfig->EcanEnableA & 1)
                        );
                        
        MsgBuffer[7]   = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanTapLength;       
        MsgBuffer[8]   = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanNlpType;         
        MsgBuffer[9]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanAdaptEnable;     
        MsgBuffer[10]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanG165DetEnable;   
        MsgBuffer[11]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanDblTalkThresh;   
        MsgBuffer[12]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanNlpThreshold;    
        MsgBuffer[13]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanNlpConv;    
        MsgBuffer[14]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanNlpUnConv;    
        MsgBuffer[15]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanNlpMaxSuppress;    

        MsgBuffer[16]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanCngThreshold;    
        MsgBuffer[17]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanAdaptLimit;      
        MsgBuffer[18]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanCrossCorrLimit;  
        MsgBuffer[19]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanNumFirSegments;  
        MsgBuffer[20]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanFirSegmentLen;   

        MsgBuffer[21]   = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanTapLength;       
        MsgBuffer[22]   = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanNlpType;         
        MsgBuffer[23]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanAdaptEnable;     
        MsgBuffer[24]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanG165DetEnable;   
        MsgBuffer[25]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanDblTalkThresh;   
        MsgBuffer[26]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanNlpThreshold;    
        MsgBuffer[27]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanNlpConv;    
        MsgBuffer[28]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanNlpUnConv;    
        MsgBuffer[29]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanNlpMaxSuppress;    
        MsgBuffer[30]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanCngThreshold;    
        MsgBuffer[31]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanAdaptLimit;      
        MsgBuffer[32]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanCrossCorrLimit;  
        MsgBuffer[33]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanNumFirSegments;  
        MsgBuffer[34]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanFirSegmentLen;   

        MsgBuffer[35] = (DSP_WORD)
                       (
                       ((pChanConfig->EcanParametersB.EcanReconvergenceCheckEnable <<5) & 0x20)  |
                       ((pChanConfig->EcanParametersA.EcanReconvergenceCheckEnable <<4) & 0x10)  |
                       ((pChanConfig->EcanParametersB.EcanTandemOperationEnable <<3) & 0x8)  |
                       ((pChanConfig->EcanParametersA.EcanTandemOperationEnable <<2) & 0x4)  |
                       ((pChanConfig->EcanParametersB.EcanMixedFourWireMode << 1) & 0x2) | 
                        (pChanConfig->EcanParametersA.EcanMixedFourWireMode & 1)
                        );
        MsgBuffer[36]  = (DSP_WORD)
                         pChanConfig->EcanParametersA.EcanMaxDoubleTalkThres;   

        MsgBuffer[37]  = (DSP_WORD)
                         pChanConfig->EcanParametersB.EcanMaxDoubleTalkThres;   

	MsgBuffer[38]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanSaturationLevel;
	MsgBuffer[39]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanSaturationLevel;
	MsgBuffer[40]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanNLPSaturationThreshold;
	MsgBuffer[41]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanNLPSaturationThreshold;
	MsgLength = 84; /* byte number == 42*2 */
	break;

    /* PCM to Packet channel type. */
    case tdmToTdmDebug:

	MsgBuffer[2] = (DSP_WORD)
			((pChanConfig->PcmInPortA << 8) |
			(pChanConfig->PcmInSlotA & 0xFF));
	MsgBuffer[3] = (DSP_WORD)
			((pChanConfig->PcmOutPortA << 8) |
			(pChanConfig->PcmOutSlotA & 0xFF));

	MsgBuffer[4] = (DSP_WORD)
			((pChanConfig->PcmInPortB << 8) |
			(pChanConfig->PcmInSlotB & 0xFF));
	MsgBuffer[5] = (DSP_WORD)
			((pChanConfig->PcmOutPortB << 8) |
			(pChanConfig->PcmOutSlotB & 0xFF));

	MsgBuffer[6] = (DSP_WORD)
			(
			((pChanConfig->FaxCngDetB << 11) & 0x0800)  |
			((pChanConfig->FaxCngDetA << 10) & 0x0400)  |
			((pChanConfig->MuteToneB << 9) & 0x0200)  |
			((pChanConfig->MuteToneA << 8) & 0x0100)  |
			((pChanConfig->FrameRate << 6)  & 0x00C0) |
			((pChanConfig->ToneTypesB << 5) & 0x0020) |
			((pChanConfig->ToneTypesA << 4) & 0x0010) |
			((pChanConfig->SoftwareCompand & 3) << 2) |
			(pChanConfig->EcanEnableB << 1) |
			(pChanConfig->EcanEnableA & 1)
			);

	MsgBuffer[7]   = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanTapLength;
	MsgBuffer[8]   = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanNlpType;
	MsgBuffer[9]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanAdaptEnable;
	MsgBuffer[10]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanG165DetEnable;
	MsgBuffer[11]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanDblTalkThresh;
	MsgBuffer[12]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanNlpThreshold;
	MsgBuffer[13]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanNlpConv;
	MsgBuffer[14]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanNlpUnConv;
	MsgBuffer[15]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanNlpMaxSuppress;

	MsgBuffer[16]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanCngThreshold;
	MsgBuffer[17]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanAdaptLimit;
	MsgBuffer[18]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanCrossCorrLimit;
	MsgBuffer[19]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanNumFirSegments;
	MsgBuffer[20]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanFirSegmentLen;

	MsgBuffer[21]   = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanTapLength;
	MsgBuffer[22]   = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanNlpType;
	MsgBuffer[23]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanAdaptEnable;
	MsgBuffer[24]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanG165DetEnable;
	MsgBuffer[25]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanDblTalkThresh;
	MsgBuffer[26]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanNlpThreshold;
	MsgBuffer[27]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanNlpConv;
	MsgBuffer[28]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanNlpUnConv;
	MsgBuffer[29]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanNlpMaxSuppress;
	MsgBuffer[30]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanCngThreshold;
	MsgBuffer[31]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanAdaptLimit;
	MsgBuffer[32]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanCrossCorrLimit;
	MsgBuffer[33]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanNumFirSegments;
	MsgBuffer[34]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanFirSegmentLen;

	MsgBuffer[35] = (DSP_WORD)
		       (
		       ((pChanConfig->EcanParametersB.EcanReconvergenceCheckEnable << 5) & 0x20)  |
		       ((pChanConfig->EcanParametersA.EcanReconvergenceCheckEnable << 4) & 0x10)  |
		       ((pChanConfig->EcanParametersB.EcanTandemOperationEnable << 3) & 0x8)  |
		       ((pChanConfig->EcanParametersA.EcanTandemOperationEnable << 2) & 0x4)  |
		       ((pChanConfig->EcanParametersB.EcanMixedFourWireMode << 1) & 0x2) |
			(pChanConfig->EcanParametersA.EcanMixedFourWireMode & 1)
			);
	MsgBuffer[36]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanMaxDoubleTalkThres;

	MsgBuffer[37]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanMaxDoubleTalkThres;

	MsgBuffer[38]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanSaturationLevel;
	MsgBuffer[39]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanSaturationLevel;
	MsgBuffer[40]  = (DSP_WORD)
			 pChanConfig->EcanParametersA.EcanNLPSaturationThreshold;
	MsgBuffer[41]  = (DSP_WORD)
			 pChanConfig->EcanParametersB.EcanNLPSaturationThreshold;
	MsgBuffer[42]  = (DSP_WORD)
			 pChanConfig->ChannelId_tobe_Debug;

	MsgLength = 86; /* byte number == 43*2 */
	break;

    /* Unknown (invalid) channel type. */
    default:
        *pStatus = Cc_InvalidChannelType;
        return (CcsParmError);
    }

    MsgBuffer[0] = MSG_CONFIGURE_CHANNEL << 8;
    MsgBuffer[1] = (DSP_WORD) ((ChannelId << 8) | (ChannelType & 0xFF));

    /* Attempt to send the Configure Channel message to the DSP and receive it's
       reply. */
	if (!TransactCmd(DspId, MsgBuffer, MsgLength, MSG_CONFIG_CHAN_REPLY, 4, 1,
                     (DSP_WORD) ChannelId))
        return (CcsDspCommFailure);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    *pStatus = (GPAK_ChannelConfigStat_t) (MsgBuffer[1] & 0xFF);
    if (*pStatus == Cc_Success)
        return (CcsSuccess);
    else
        return (CcsParmError);
}
EXPORT_SYMBOL(gpakConfigureChannel);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakTearDownChannel - Tear Down a DSP's Channel.
 *
 * FUNCTION
 *  This function tears down a DSP's Channel.
 *
 * RETURNS
 *  Status code indicating success or a specific error.
 *
 */
gpakTearDownStatus_t gpakTearDownChannel(
    unsigned short int DspId,           /* DSP Id (0 to MaxDSPCores-1) */
    unsigned short int ChannelId,       /* Channel Id (0 to MaxChannels-1) */
    GPAK_TearDownChanStat_t *pStatus    /* pointer to Tear Down Status */
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (TdsInvalidDsp);

    /* Make sure the Channel Id is valid. */
    if (ChannelId >= MaxChannels[DspId])
        return (TdsInvalidChannel);

    /* Build the Tear Down Channel message. */
    MsgBuffer[0] = MSG_TEAR_DOWN_CHANNEL << 8;
    MsgBuffer[1] = (DSP_WORD) (ChannelId << 8);

    /* Attempt to send the Tear Down Channel message to the DSP and receive it's
       reply. */
	if (!TransactCmd(DspId, MsgBuffer, 3, MSG_TEAR_DOWN_REPLY, 4, 1,
	                 (DSP_WORD) ChannelId))
        return (TdsDspCommFailure);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    *pStatus = (GPAK_TearDownChanStat_t) (MsgBuffer[1] & 0xFF);
    if (*pStatus == Td_Success)
        return (TdsSuccess);
    else
        return (TdsError);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakAlgControl - Control an Algorithm.
 *
 * FUNCTION
 *  This function controls an Algorithm
 *
 * RETURNS
 *  Status code indicating success or a specific error.
 *
 */
gpakAlgControlStat_t gpakAlgControl(
    unsigned short int  DspId,          // DSP identifier
    unsigned short int  ChannelId,		// channel identifier
    GpakAlgCtrl_t       ControlCode,    // algorithm control code
    GPAK_AlgControlStat_t *pStatus      // pointer to return status
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */
    
    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (AcInvalidDsp);

    /* Make sure the Channel Id is valid. */
    if (ChannelId >= MaxChannels[DspId])
        return (AcInvalidChannel);

    MsgBuffer[0] = MSG_ALG_CONTROL << 8;
    MsgBuffer[1] = (DSP_WORD) ((ChannelId << 8) | (ControlCode & 0xFF));

    /* Attempt to send the Tear Down Channel message to the DSP and receive it's
       reply. */
	//need_reply_len;       
	if (!TransactCmd(DspId, MsgBuffer, 4, MSG_ALG_CONTROL_REPLY, 4, 1,  
	                 (DSP_WORD) ChannelId))
        return (AcDspCommFailure);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    *pStatus = (GPAK_AlgControlStat_t) (MsgBuffer[1] & 0xFF);
    if (*pStatus == Ac_Success)
        return (AcSuccess);
    else
        return (AcParmError);

}
EXPORT_SYMBOL(gpakAlgControl);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakReadEventFIFOMessage - read from the event fifo
 * 
 * FUNCTION
 *  This function reads a single event from the event fifo if one is available
 * 
 * RETURNS
 *  Status  code indicating success or a specific error.
 *
 * Notes: This function should be called in a loop until the return status 
 *        indicates that the fifo is empty.
 *      
 *        If the event code equals "EventLoopbackTeardownComplete", then the 
 *        contents of *pChannelId hold the coderBlockId that was assigned to
 *        the loopback coder that was torn down.
 */
gpakReadEventFIFOMessageStat_t gpakReadEventFIFOMessage(
    unsigned short int      DspId,          // DSP identifier
    unsigned short int      *pChannelId,    // pointer to channel identifier
    GpakAsyncEventCode_t    *pEventCode,    // pointer to Event Code
    GpakAsyncEventData_t    *pEventData     // pointer to Event Data Struct
    )
{
    DSP_WORD WordBuffer[WORD_BUFFER_SIZE];  /* DSP words buffer */
    GpakAsyncEventCode_t EventCode;         /* DSP's event code */
    DSP_WORD EventDataLength;               /* Length of event to read */
    DSP_WORD ChannelId;                     /* DSP's channel Id */
    DSP_ADDRESS EventInfoAddress;     /* address of EventFIFO info structure */
    DSP_ADDRESS BufrBaseAddress;      /* base address of EventFIFO buffer */
    DSP_ADDRESS BufrLastAddress;      /* last address of EventFIFO buffer */
    DSP_ADDRESS TakeAddress;          /* current take address in fifo buffer */
    DSP_WORD BufrSize;      /* size (in words) of event FIFO buffer */
    DSP_WORD PutIndex;      /* event fifo put index */
    DSP_WORD TakeIndex;     /* event fifo take index */
    DSP_WORD WordsReady;    /* number words ready for read out of event fifo */
    DSP_WORD EventError;    /* flag indicating error with event fifo msg  */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (RefInvalidDsp);

    /* Lock access to the DSP. */
    gpakLockAccess(DspId);

    /* Check if the DSP was reset and is ready. */
    if (CheckDspReset(DspId) == -1)
    {
        gpakUnlockAccess(DspId);
        return (RefDspCommFailure);
    }

    /* Check if an event message is ready in the DSP. */
    EventInfoAddress = pEventFifoAddress[DspId];
    gpakReadDspMemory(DspId, EventInfoAddress, CIRC_BUFFER_INFO_STRUCT_SIZE, 
                                                                 WordBuffer);
    RECONSTRUCT_LONGWORD(BufrBaseAddress, ((DSP_WORD *)&WordBuffer[CB_BUFR_BASE]));
    BufrSize = WordBuffer[CB_BUFR_SIZE];
    PutIndex = WordBuffer[CB_BUFR_PUT_INDEX];
    TakeIndex = WordBuffer[CB_BUFR_TAKE_INDEX];
    if (PutIndex >= TakeIndex)
        WordsReady = PutIndex - TakeIndex;
    else
        WordsReady = PutIndex + BufrSize - TakeIndex;

    if (WordsReady < 2)
    {
        gpakUnlockAccess(DspId);
        return (RefNoEventAvail);
    }

    /* Read the event header from the DSP's Event FIFO. */
    TakeAddress = BufrBaseAddress + TakeIndex;
    BufrLastAddress = BufrBaseAddress + BufrSize - 1;
    ReadCircBuffer(DspId, BufrBaseAddress, BufrLastAddress, &TakeAddress,
                   WordBuffer, 2);
    TakeIndex += 2;
    if (TakeIndex >= BufrSize)
        TakeIndex -= BufrSize;

    ChannelId = (WordBuffer[0] >> 8) & 0xFF;
    EventCode = (GpakAsyncEventCode_t)(WordBuffer[0] & 0xFF);
    EventDataLength = WordBuffer[1];
    EventError = 0;

    switch (EventCode)
    {
        case EventToneDetect:
            if (EventDataLength > WORD_BUFFER_SIZE)
            {
                gpakUnlockAccess(DspId);
                return (RefInvalidEvent);
            }
            ReadCircBuffer(DspId, BufrBaseAddress, BufrLastAddress, &TakeAddress,
                   WordBuffer, EventDataLength);
            pEventData->toneEvent.ToneCode = (GpakToneCodes_t)
                                                (WordBuffer[0] & 0xFF);
            pEventData->toneEvent.ToneDuration = WordBuffer[1];
            pEventData->toneEvent.Direction = WordBuffer[2];
            pEventData->toneEvent.DebugToneStatus = WordBuffer[3];
            TakeIndex += EventDataLength;
            if (TakeIndex >= BufrSize)
                TakeIndex -= BufrSize;
            if (EventDataLength != 4)
                EventError = 1;
            break;

        default:
            EventError = 1;
            break;
    };

    /* Update the Take index in the DSP's Packet Out buffer information. */
    gpakWriteDspMemory(DspId, EventInfoAddress + CB_BUFR_TAKE_INDEX, 1,
                       &TakeIndex);

    /* Unlock access to the DSP. */
    gpakUnlockAccess(DspId);

    if (EventError)
        return(RefInvalidEvent);

    *pChannelId = ChannelId;
    *pEventCode = EventCode;
    return(RefEventAvail);

}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakPingDsp - ping the DSP to see if it's alive
 * 
 * FUNCTION
 *  This function checks if the DSP is still communicating with the host
 *  and returns the DSP SW version
 * 
 * RETURNS
 *  Status  code indicating success or a specific error.
 */
gpakPingDspStat_t gpakPingDsp(
    unsigned short int      DspId,          // DSP identifier
    unsigned short int      *pDspSwVersion  // DSP software version
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */
    DSP_WORD DspStatus;                     /* DSP's reply status */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (PngInvalidDsp);

    /* send value of 1, DSP increments it */
    MsgBuffer[0] = (MSG_PING << 8);

    /* Attempt to send the ping message to the DSP and receive it's
       reply. */
	if (!TransactCmd(DspId, MsgBuffer, 1, MSG_PING_REPLY, 6, 0, 0))
        return (PngDspCommFailure);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    DspStatus = (MsgBuffer[1] & 0xFF);
    if (DspStatus == 0)
    {
        *pDspSwVersion = MsgBuffer[2];
        return (PngSuccess);
    }
    else
        return (PngDspCommFailure);
}
EXPORT_SYMBOL(gpakPingDsp);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakSerialTxFixedValue - transmit a fixed value on a timeslot
 * 
 * FUNCTION
 *  This function controls transmission of a fixed value out onto a serial 
 *  port's timeslot.
 * 
 * RETURNS
 *  Status  code indicating success or a specific error.
 */
gpakSerialTxFixedValueStat_t gpakSerialTxFixedValue(
    unsigned short int      DspId,          // DSP identifier
    unsigned short int      ChannelId,      // channel identifier
    GpakSerialPort_t        PcmOutPort,     // PCM Output Serial Port Id
    unsigned short int      PcmOutSlot,     // PCM Output Time Slot
    unsigned short int      Value,          // 16-bit value 
    GpakActivation          State 		    // activation state
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */
    DSP_WORD DspStatus;                     /* DSP's reply status */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (TfvInvalidDsp);

    /* Make sure the Channel Id is valid. */
    if (ChannelId >= MaxChannels[DspId])
        return (TfvInvalidChannel);


    /* Build the message. */
    MsgBuffer[0] = MSG_SERIAL_TXVAL << 8;
    MsgBuffer[1] = (DSP_WORD) ((ChannelId << 8) | (State & 0xFF));
    MsgBuffer[2] = (DSP_WORD) ((PcmOutPort << 8) | (PcmOutSlot & 0xFF));
    MsgBuffer[3] = (DSP_WORD) Value;

    /* Attempt to send the message to the DSP and receive it's
       reply. */
    //need_reply_len;
	if (!TransactCmd(DspId, MsgBuffer, 8, MSG_SERIAL_TXVAL_REPLY, 4,
	                                                    1, ChannelId))
        return (TfvDspCommFailure);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    DspStatus = (MsgBuffer[1] & 0xFF);
    if (DspStatus == 0)
        return (TfvSuccess);
    else
        return (TfvDspCommFailure);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakControlTdmLoopBack - control a serial port's loopback state
 * 
 * FUNCTION
 *  This function enables/disables the tdm input to output looback mode on a
 *  serial port
 * 
 * RETURNS
 *  Status  code indicating success or a specific error.
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
gpakControlTdmLoopBackStat_t gpakControlTdmLoopBack(
    unsigned short int      DspId,          // DSP identifier
    GpakSerialPort_t        SerialPort,     // Serial Port Id
    GpakActivation          LoopBackState   // Loopback State
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */
    DSP_WORD DspStatus;                     /* DSP's reply status */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (ClbInvalidDsp);

    /* Build the message. */
    MsgBuffer[0] = MSG_TDM_LOOPBACK << 8;
    MsgBuffer[1] = (DSP_WORD) ((SerialPort << 8) | (LoopBackState & 0xFF));

    /* Attempt to send the message to the DSP and receive it's
       reply. */
    //need_reply_len;
	if (!TransactCmd(DspId, MsgBuffer, 4, MSG_TDM_LOOPBACK_REPLY, 4, 0, 0))
        return (ClbDspCommFailure);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    DspStatus = (MsgBuffer[1] & 0xFF);
    if (DspStatus == 0)
        return (ClbSuccess);
    else
        return (ClbDspCommFailure);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakReadCpuUsage - Read CPU usage statistics from a DSP.
 *
 * FUNCTION
 *  This function reads the CPU usage statistics from a DSP's memory. The
 *  average CPU usage in units of .1 percent are obtained for each of the frame
 *  rates.
 *
 * RETURNS
 *  Status code indicating success or a specific error.
 *
 */
gpakReadCpuUsageStat_t gpakReadCpuUsage(
    unsigned short int  DspId,              // Dsp Identifier
    unsigned short int  *pPeakUsage,        // pointer to peak usage variable
    unsigned short int  *pPrev1SecPeakUsage // peak usage over previous 1 second   
    )
{
    DSP_WORD ReadBuffer[2];     /* DSP read buffer */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (RcuInvalidDsp);

    /* Lock access to the DSP. */
    gpakLockAccess(DspId);

    /* Check if the DSP was reset and is ready. */
    if (CheckDspReset(DspId) == -1)
        return (RcuDspCommFailure);

    /* Read the CPU Usage statistics from the DSP. */
    gpakReadDspMemory(DspId, pDspIfBlk[DspId] + CPU_USAGE_OFFSET, 2,
                      ReadBuffer);

    /* Unlock access to the DSP. */
    gpakUnlockAccess(DspId);

    /* Store the usage statistics in the specified variables. */
    *pPrev1SecPeakUsage = ReadBuffer[0];
    *pPeakUsage = ReadBuffer[1];
    
    /* Return with an indication the usage staistics were read successfully. */
    return (RcuSuccess);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakResetCpuUsageStats - reset the cpu usage statistics
 * 
 * FUNCTION
 *  This function resets the cpu utilization statistics
 * 
 * RETURNS
 *  Status  code indicating success or a specific error.
 */
gpakResetCpuUsageStat_t gpakResetCpuUsageStats(
    unsigned short int  DspId              // DSP identifier
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */
    DSP_WORD DspStatus;                     /* DSP's reply status */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (RstcInvalidDsp);

    MsgBuffer[0] = (MSG_RESET_USAGE_STATS << 8);

    /* Attempt to send the message to the DSP and receive it's reply. */
    //need_reply_len;
	if (!TransactCmd(DspId, MsgBuffer, 2, MSG_RESET_USAGE_STATS_REPLY, 4, 0, 0))
        return (RstcDspCommFailure);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    DspStatus = (MsgBuffer[1] & 0xFF);
    if (DspStatus == 0)
        return (RstcSuccess);
    else
        return (RstcDspCommFailure);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakReadFramingStats
 * 
 * FUNCTION
 *  This function reads a DSP's framing interrupt statistics
 * 
 * RETURNS
 *  Status  code indicating success or a specific error.
 */
gpakReadFramingStatsStatus_t gpakReadFramingStats(
    unsigned short int  DspId,                // DSP identifier
    unsigned short int  *pFramingError1Count, // port 1 Framing error count
    unsigned short int  *pFramingError2Count, // port 2 Framing error count
    unsigned short int  *pFramingError3Count, // port 3 Framing error count
    unsigned short int  *pDmaStopErrorCount,   // DMA-stoppage error count
    unsigned short int  *pDmaSlipStatsBuffer   // DMA slips count
    )
{
    DSP_WORD ReadBuffer[10];     /* DSP read buffer */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (RfsInvalidDsp);

    /* Lock access to the DSP. */
    gpakLockAccess(DspId);

    /* Check if the DSP was reset and is ready. */
    if (CheckDspReset(DspId) == -1)
        return (RfsDspCommFailure);

    /* Read the framing interrupt statistics from the DSP. */
    gpakReadDspMemory(DspId, pDspIfBlk[DspId] + FRAMING_STATS_OFFSET, 10,
                      ReadBuffer);

    /* Unlock access to the DSP. */
    gpakUnlockAccess(DspId);

    /* Store the framing statistics in the specified variables. */
    *pFramingError1Count = ReadBuffer[0];
    *pFramingError2Count = ReadBuffer[1];
    *pFramingError3Count = ReadBuffer[2];
    *pDmaStopErrorCount  = ReadBuffer[3];
    
	if (pDmaSlipStatsBuffer != NULL) {
		/* If users want to get the DMA slips count */
		pDmaSlipStatsBuffer[0] = ReadBuffer[4];
		pDmaSlipStatsBuffer[1] = ReadBuffer[5];
		pDmaSlipStatsBuffer[2] = ReadBuffer[6];
		pDmaSlipStatsBuffer[3] = ReadBuffer[7];
		pDmaSlipStatsBuffer[4] = ReadBuffer[8];
		pDmaSlipStatsBuffer[5] = ReadBuffer[9];
	}
    /* Return with an indication the statistics were read successfully. */
    return (RfsSuccess);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakResetFramingStats - reset a DSP's framing interrupt statistics
 * 
 * FUNCTION
 *  This function resets a DSP's framing interrupt statistics
 * 
 * RETURNS
 *  Status  code indicating success or a specific error.
 */
gpakResetFramingStatsStatus_t gpakResetFramingStats(
    unsigned short int      DspId          // DSP identifier
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */
    DSP_WORD DspStatus;                     /* DSP's reply status */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (RstfInvalidDsp);

    MsgBuffer[0] = (MSG_RESET_FRAME_STATS << 8);

    /* Attempt to send the message to the DSP and receive it's reply. */
    //need_reply_len;
	if (!TransactCmd(DspId, MsgBuffer, 2, MSG_RESET_FRAME_STATS_REPLY, 4, 0, 0))
        return (RstfDspCommFailure);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    DspStatus = (MsgBuffer[1] & 0xFF);
    if (DspStatus == 0)
        return (RstfSuccess);
    else
        return (RstfDspCommFailure);
}

/*
 * gpakDownloadDsp - Download a DSP's Program and initialized Data memory.
 *
 * FUNCTION
 *  This function reads a DSP's Program and Data memory image from the
 *  specified file and writes the image to the DSP's memory.
 *
 * RETURNS
 *  Status code indicating success or a specific error.
 *
 */
gpakDownloadStatus_t gpakDownloadDsp(
    unsigned short DspId,   /* DSP Identifier (0 to MaxDSPCores-1) */
    GPAK_FILE_ID FileId     /* G.PAK Download File Identifier */
    )
{
    gpakDownloadStatus_t RetStatus;     /* function return status */
    int NumRead;                        /* number of file bytes read */
    DSP_ADDRESS Address;                /* DSP address */
    unsigned int WordCount;             /* number of words in record */
    unsigned int NumWords;              /* number of words to read/write */
    unsigned int i;                     /* loop index / counter */
    unsigned int j;                     /* loop index */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (GdlInvalidDsp);

    /* Lock access to the DSP. */
    gpakLockAccess(DspId);

    RetStatus = GdlSuccess;
    while (RetStatus == GdlSuccess)
    {

        /* Read a record header from the file. */
        NumRead = gpakReadFile(FileId, DlByteBufr, 6);
        if (NumRead == -1)
        {
            RetStatus = GdlFileReadError;
            break;
        }
        if (NumRead != 6)
        {
            RetStatus = GdlInvalidFile;
            break;
        }
        Address = (((DSP_ADDRESS) DlByteBufr[1]) << 16) |
                  (((DSP_ADDRESS) DlByteBufr[2]) << 8) |
                  ((DSP_ADDRESS) DlByteBufr[3]);
        WordCount = (((unsigned int) DlByteBufr[4]) << 8) |
                    ((unsigned int) DlByteBufr[5]);

        /* Check for the End Of File record. */
        if (DlByteBufr[0] == 0xFF)
            break;

        /* Verify the record is for a valid memory type. */
        if ((DlByteBufr[0] != 0x00) && (DlByteBufr[0] != 0x01))
        {
            RetStatus = GdlInvalidFile;
            break;
        }

        /* Read a block of words at a time from the file and write to the
           DSP's memory .*/
        while (WordCount != 0)
        {
            if (WordCount < DOWNLOAD_BLOCK_SIZE)
                NumWords = WordCount;
            else
                NumWords = DOWNLOAD_BLOCK_SIZE;
            WordCount -= NumWords;
            NumRead = gpakReadFile(FileId, DlByteBufr, NumWords * 2);
            if (NumRead == -1)
            {
                RetStatus = GdlFileReadError;
                break;
            }
            if (NumRead != (NumWords * 2))
            {
                RetStatus = GdlInvalidFile;
                break;
            }
            for (i = 0, j = 0; i < NumWords; i++, j += 2)
                DlWordBufr[i] = (((DSP_WORD) DlByteBufr[j]) << 8) |
                                ((DSP_WORD) DlByteBufr[j + 1]);
            gpakWriteDspMemory(DspId, Address, NumWords, DlWordBufr);
            Address += ((DSP_ADDRESS) NumWords);
        }
    }

    /* Unlock access to the DSP. */
    gpakUnlockAccess(DspId);

    /* Return with an indication of success or failure. */
    return (RetStatus);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakReadCpuUsage - Read CPU usage statistics from a DSP.
 *
 * FUNCTION
 *  This function reads the memory map register section of DSP memory. 
 *
 * RETURNS
 *  Status code indicating success or a specific error.
 *
 */
gpakReadDSPMemoryStat_t gpakReadDSPMemoryMap(
    unsigned short int  DspId,         // Dsp Identifier
    unsigned short int  *pDest,        // Buffer on host to hold DSP memory map
	DSP_ADDRESS BufrBaseAddress,       // DSP memory users want to read out
    unsigned short int   MemoryLength_Word16 // Length of memory section read out, unit is 16-bit word
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */
    DSP_WORD DspStatus;                     /* DSP reply's status */
    int i;                                  /* loop index / counter */

    
    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (RmmInvalidDsp);

    /* Verify the message buffer is large enough  */
    if (MSG_BUFFER_SIZE < MemoryLength_Word16 )
        return (RmmSizeTooBig);

    MsgBuffer[0] = MSG_READ_DSP_MEMORY << 8;
    MsgBuffer[1] = (DSP_WORD) ((BufrBaseAddress >> 16) & 0xFFFF);
    MsgBuffer[2] = (DSP_WORD) (BufrBaseAddress & 0xFFFF);
    MsgBuffer[3] = (DSP_WORD) MemoryLength_Word16;

    /* Attempt to send the Read memory section message to the DSP and receive it's
       reply. */
	//need_reply_len;       
	if (!TransactCmd(DspId, MsgBuffer, 8, MSG_READ_DSP_MEMORY_REPLY, 
			(MemoryLength_Word16+2)*2, 0,  0) )
        return (RmmInvalidAddress);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    DspStatus = (MsgBuffer[1] & 0xFF);
    if (DspStatus != 0)
        return (RmmFailure);

	for (i = 0; i < MemoryLength_Word16; i++)
        pDest[i] = (short int) MsgBuffer[2 + i];


    return (RmmSuccess);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakAccessGPIO - change Direction/read/write the GPIO on DSP 
 * 
 * FUNCTION
 *  This function read/write GPIO and change the GPIO direction
 *  
 * 
 * RETURNS
 *  Status  code indicating success or a specific error.
 */
gpakAccessGPIOStat_t gpakAccessGPIO(
    unsigned short int      DspId,          // DSP identifier
	GpakGPIOCotrol_t        gpakControlGPIO,// select oeration, changeDIR/write/read
    unsigned short int      *pGPIOValue  // DSP software version
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */
    DSP_WORD DspStatus;                     /* DSP's reply status */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (GPIOInvalidDsp);

    /* send value of 1, DSP increments it */
    MsgBuffer[0] = (MSG_ACCESSGPIO << 8);
	MsgBuffer[1] = (DSP_WORD) ((gpakControlGPIO << 8) | (*pGPIOValue & 0xFF) );
    /* Attempt to send the ping message to the DSP and receive it's
       reply. */
	if (!TransactCmd(DspId, MsgBuffer, 4, MSG_ACCESSGPIO_REPLY, 6, 0, 0))
        return (GPIODspCommFailure);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    DspStatus = (MsgBuffer[1] & 0xFF);
    if (DspStatus == 0)
    {
        *pGPIOValue = MsgBuffer[2];
        return (GPIOSuccess);
    }
    else
        return (GPIODspCommFailure);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakWriteSystemParms - Write a DSP's System Parameters.
 *
 * FUNCTION
 *  This function writes a DSP's System Parameters information.
 *
 *    Note:
 *      Or-together the desired bit-mask #defines that are listed below. Only
 *      those algorithm parameters whose bit-mask is selected in the UpdateBits
 *      function parameter will be updated.
 *
 * RETURNS
 *  Status code indicating success or a specific error.
 *
 */

gpakWriteSysParmsStatus_t gpakWriteSystemParms(
    unsigned short int      DspId,  // DSP identifier
    GpakSystemParms_t *pSysParms,   /* pointer to System Parms info var */
    unsigned short int UpdateBits, /* input: flags indicating which parms to update */
    GPAK_SysParmsStat_t *pStatus    /* pointer to Write System Parms Status */
    )
{
    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */
    DSP_WORD DspStatus;                     /* DSP's reply status */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (WspInvalidDsp);

    /* Build the Write System Parameters message. */
    MsgBuffer[0] = MSG_WRITE_SYS_PARMS << 8;

    if (UpdateBits & DTMF_UPDATE_MASK)
    {
        MsgBuffer[1] |= DTMF_UPDATE_MASK;
        MsgBuffer[8] = (DSP_WORD) pSysParms->MinSigLevel;
        MsgBuffer[9] = (DSP_WORD) (pSysParms->FreqDeviation & 0xff);
        if (pSysParms->SNRFlag)
            MsgBuffer[9] |= (1<<8);
    }

    MsgBuffer[10] = (DSP_WORD) 0;
    if (UpdateBits & DTMF_TWIST_UPDATE_MASK)
    {
        MsgBuffer[1] |= DTMF_TWIST_UPDATE_MASK;
        MsgBuffer[10] |= (DSP_WORD) (pSysParms->DtmfFwdTwist & 0x000f);
        MsgBuffer[10] |= (DSP_WORD) ((pSysParms->DtmfRevTwist << 4) & 0x00f0);
    }


    if (UpdateBits & DTMF_VALID_MASK)
    {
        MsgBuffer[1] |= DTMF_VALID_MASK;
	    MsgBuffer[11] = (DSP_WORD) (pSysParms->DtmfValidityMask & 0x00ff);
	}

    /* Attempt to send the ping message to the DSP and receive it's
       reply. */
	if (!TransactCmd(DspId, MsgBuffer, 24, MSG_WRITE_SYS_PARMS_REPLY, 6, 0, 0))
        return (WspDspCommFailure);

    /* Return with an indication of success or failure based on the return
       status in the reply message. */
    *pStatus = (GPAK_SysParmsStat_t) (MsgBuffer[2] );

    DspStatus = (MsgBuffer[1] & 0xFF);
    if (DspStatus == 0)
        return (WspSuccess);
    else
        return (WspDspCommFailure);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gpakReadSystemParms - Read a DSP's System Parameters.
 *
 * FUNCTION
 *  This function reads a DSP's System Parameters information.
 *
 * RETURNS
 *  Status code indicating success or a specific error.
 *
 */
gpakReadSysParmsStatus_t gpakReadSystemParms(
    unsigned short int      DspId,  // DSP identifier
    GpakSystemParms_t *pSysParms    /* pointer to System Parms info var */
    )
{

    DSP_WORD MsgBuffer[MSG_BUFFER_SIZE];    /* message buffer */

    /* Make sure the DSP Id is valid. */
    if (DspId >= MAX_DSP_CORES)
        return (RspInvalidDsp);

    /* Build the Read System Parameters message. */
    MsgBuffer[0] = MSG_READ_SYS_PARMS << 8;

    /* Attempt to send the ping message to the DSP and receive it's
       reply. */
	if (!TransactCmd(DspId, MsgBuffer, 2, MSG_READ_SYS_PARMS_REPLY, 22, 0, 0))
        return (RspDspCommFailure);

    /* Extract the System Parameters information from the message. */
    pSysParms->DtmfValidityMask = (short int)(MsgBuffer[7]) ;

    pSysParms->MinSigLevel   =  (short int)MsgBuffer[8];
    pSysParms->SNRFlag       = (short int)((MsgBuffer[9]>>8) & 0x1);
    pSysParms->FreqDeviation =  (short int)(MsgBuffer[9] & 0xff);
    pSysParms->DtmfFwdTwist  = (short int)MsgBuffer[10] & 0x000f;
    pSysParms->DtmfRevTwist  = (short int)(MsgBuffer[10] >> 4) & 0x000f;

    /* Return with an indication that System Parameters info was obtained. */
    return (RspSuccess);
}
