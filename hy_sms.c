/******************************************************************************
 * File Name   : sms.c
 *
 * Author      :
 *
 * Version     :
 *
 * Date        :
 *
 * DESCRIPTION : -
 *
 * --------------------
 * Copyright 2009-2019 Hymost Co.,Ltd.
 *
 ******************************************************************************/

#include "hy_public.h"

u8 const ATCMGF1[] = {"AT+CMGF=1\r"};
u8 const ATCMGF0[] = {"AT+CMGF=0\r"};
u8 const ATCMGL[] = {"AT+CMGL=\"ALL\"\r"};
u8 const ATCMGR[] = {"AT+CMGR=00\r"};
u8 const ATCMGD[] = {"AT+CMGD=00\r"};
u8 const ATCMGS[] = {"AT+CMGS="};
//HYN001-15 Modify sms code add by hedaihua at 20201128 start
u8 const ATCNMI[] = {"AT+CNMI=2,1\r"};
//HYN001-15 Modify sms code add by hedaihua at 20201128 end
u8 const ATCSCSGSM[] = {'A', 'T', '+', 'C', 'S', 'C', 'S', '=', '"', 'G', 'S', 'M', '"', 0x0d};

u8 *SmsOptStr[] =
{
    "<SPHYMOST",    //配置多项参数
    "CKHYMOST",     //查询参数
    "CKLOG",        //查询日志
    "<SPBSJ",       //配置多项参数(兼容BSJ)
    "CKBSJ",        //配置多项参数(兼容BSJ)
    /*[HMB001-107] add unattach data network times by tangmingyu at 20190814 start*/
    "<SPHYDBG",
    "CKHYDBG"
    /*[HMB001-107] add unattach data network times by tangmingyu at 20190814 end*/
};

#if 0  //PassWordFunc
u8 *PassWordStr[] =
{
    "*P:HYMOSTGPS",
    "*P:BSJGPS",
    "*P:HYMOSTINI",
    "*P:BSJINI"
};
#endif

//UserType_SMS SMSB;
UserType_SMS rSmsBufA;
//UserType_TuartConfig gsmtxd;   //接收数据绶冲区结构类型
//UserType_UartConfig gsmrxd;      //发送数据绶冲区结构类型
u8 g_cPublicBuffer[PUBLIC_BUFFER_LEN];   //用于发送短信字符串
u8 CacheSmsBuf[200];
#ifdef NOTE_GROUP
    UT_LongSms g_struLongsms[MAX_LONGSMS_COUNT];
    u8 g_cLongsmsIndex = 0;
    u8 g_cLongsmsIndexTimeOut = 0;
#endif

//u8 gsmATCGMRG510(u8 *Text)
//void GSM_NetTimPro(void)

//-----------------------------------------------------------------------------
void gsmATCNMI(void)
{
    u8 j = 0;

    j = sendgsmDat((u8 *)ATCNMI, sizeof(ATCNMI) - 1, 25, 3, 3, 4);
    if (j == 0)
    {
        //GsmStu.ResetEvt = CNMI_ERR_RESET;
        //res_Mian_Sys();//实现系统复位
    }
}
//-----------------------------------------------------------------------------
void gsmATCSCSGSM(void)
{
    sendgsmDat((u8 *)ATCSCSGSM, sizeof(ATCSCSGSM), 2, 2, 2, 3);
}

void gsmATCMGF1(void)
{
    sendgsmDat((u8 *)ATCMGF1, sizeof(ATCMGF1) - 1, 3, 2, 2, 3);
}
//------------------------------------------------------------------------------
void gsmATCMGF0(void)
{
    sendgsmDat((u8 *)ATCMGF0, sizeof(ATCMGF0) - 1, 2, 2, 2, 3);
}
//------------------------------------------------------------------------------
void gsmATCMGL(void)
{
    sendgsmDat((u8 *)ATCMGL, sizeof(ATCMGL) - 1, 2, 2, 2, 60);
}
//------------------------------------------------------------------------------
u8 gsmATCMGR(u8 index)
{
    u8 j = 0, tempBuf[20];
    #ifdef NOTE_GROUP
    gsmATCMGF0();       //PDU模式读取短信
    #else
    gsmATCMGF1();       //文本模式读取短信
    #endif
    conver((u8 *)ATCMGR, &tempBuf[0], sizeof(ATCMGR) - 1);
    tempBuf[8] = asc(index / 10);
    tempBuf[9] = asc(index % 10);
    j = sizeof(ATCMGR) - 1;
    j = sendgsmDat(&tempBuf[0], j, 2, 1, 2, 10);
    return (j);
}
//------------------------------------------------------------------------------
void gsmATCMGD(u8 index)
{
    u8 j = 0;
    u8 TmpBuf[12];

    conver((u8 *)ATCMGD, &TmpBuf[0], sizeof(ATCMGD) - 1);
    TmpBuf[8] = asc(index / 10);
    TmpBuf[9] = asc(index % 10);
    j = sizeof(ATCMGD) - 1;
    j = sendgsmDat(&TmpBuf[0], j, 2, 1, 2, 20);
}
//------------------------------------------------------------------------------
u8 gsmATCMGS(u8 *num, u8 len, u8 t)
{
    //at+cmgs=
    u8 i, j, tempBuf[40];

    conver((u8 *)ATCMGS, &tempBuf[0], sizeof(ATCMGS) - 1);
    j = 8;
    if (t == TXT)
    {
        tempBuf[j++] = '"';
    }
    conver(num, &tempBuf[j], len);
    j += len;
    if (t == TXT)
    {
        tempBuf[j++] = '"';
    }
    //HYN001-15 Modify sms code add by hedaihua at 20201128 start
    tempBuf[j++] = 0x0d;
    tempBuf[j++] = 0x0a;
    i = sendgsmDat(&tempBuf[0], j, 1, 2, 2, 20);
    //HYN001-15 Modify sms code add by hedaihua at 20201128 end

    return (i);
}

//------------------------------------------------------------------------------
#ifdef NOTE_GROUP
void iniLongsmsVlau(void)
{
    u8 i = 0;
    for (i = 0; i < MAX_LONGSMS_COUNT; i++)
    {
        memset(&g_struLongsms[i], 0, sizeof(g_struLongsms[i]));
    }
}

u8 Longsmspro(UserType_SMS *Text)
{
    static u8 struP = 0xFF;
    u8 k = 0;
    u8 p = 0;
    u8 c = 0;
    u8 l = 0;
    u8 p1 = 0, c1 = 0;

    if ((g_cLongsmsIndex > 0) && (g_cLongsmsIndex < 0x0A))
    {
        if (g_struLongsms[g_cLongsmsIndex - 1].head[0] == 0)
        {
            return 0;
        }
        //05000350 0202
        //0600035000 0301
        k = g_struLongsms[g_cLongsmsIndex - 1].head[1];   /*长信息头长度*/
        p = g_struLongsms[g_cLongsmsIndex - 1].head[k];   /*长信息总包数*/
        c = g_struLongsms[g_cLongsmsIndex - 1].head[k + 1]; /*长信息当前包数*/
        p1 = p;
        c1 = c;

        /*数据起始地址*/
        if (Text->smsType == TXT)
        {
            k = 8;
            Text->sDataLen -= 7;
        }
        else
        {
            Text->sDataLen -= 6;
            k += 2;
        }

        if (p > MAX_LONGSMS_COUNT)
        {
            return 0;
        }

        if ((g_cLongsmsIndexTimeOut == 0) || (p != struP))
        {
            g_cLongsmsIndexTimeOut = 30;
            struP = p;
            iniLongsmsVlau();
        }
        /*先把该条存起来*/
        if (Text->sDataLen > 170)
        {
            Text->sDataLen = 170;
        }
        for (l = 1; l <= Text->sDataLen; l++)
        {
            g_struLongsms[c - 1].cSmsbuf[l] = Text->GlobBuf[k++];
        }
        g_struLongsms[c - 1].cSmsbuf[0] = Text->sDataLen;
        g_struLongsms[c - 1].cType = Text->smsType;

        /*检查是否全部收完*/
        for (l = 0; l < p; l++)
        {
            if (g_struLongsms[l].cSmsbuf[0] == 0)
            {
                return 1;  /*符合长信息, 但没有接收完*/
            }
        }

        /*合并长信息*/
        Text->sDataLen = 0;
        /*循环所有包*/
        for (l = 0; l < p; l++)
        {
            if (g_struLongsms[l].cType == TXT)
            {
                for (c = 1; c <= g_struLongsms[l].cSmsbuf[0]; c++)
                {
                    if (Text->sDataLen < 580)
                    {
                        Text->GlobBuf[++Text->sDataLen] = g_struLongsms[l].cSmsbuf[c];
                    }
                }
            }
            else
            {
                for (c = 1; c <= g_struLongsms[l].cSmsbuf[0]; c += 2)
                {
                    if (g_struLongsms[l].cSmsbuf[c] != 0)
                    {
                        if (Text->sDataLen < 580)
                        {
                            Text->GlobBuf[++Text->sDataLen] = g_struLongsms[l].cSmsbuf[c];
                        }
                    }

                    if (Text->sDataLen < 580)
                    {
                        Text->GlobBuf[++Text->sDataLen] = g_struLongsms[l].cSmsbuf[c + 1];
                    }
                }
            }
        }

        if (p1 != c1)
        {
            return 0;
        }
        else
        {
            struP = 0xFF;
            iniLongsmsVlau();
            g_cLongsmsIndexTimeOut = 0;
            return 2;  /*符合长信息, 接收完*/
        }
    }
    else
    {
        struP = 0xFF;
    }

    return 0;  /*不符合长信息*/
}



void getPDUsms(UserType_SMS *getBuf, u8 *Rxbuf, u16 sAll)
{
    u16 j, k, RxLen;
    u8 i, k1, a1, z;
    u8 const br[] = {0, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};
    u8 const bl[] = {0, 1, 2, 3, 4, 5, 6, 7};
    u8 const bh[] = {0, 1, 3, 7, 0x0f, 0x1f, 0x3f, 0x7f};
    u8 const bh1[] = {0, 6, 5, 4, 3, 2, 1};

    //短信息类型[0]
    //号码getBuf[1....30]
    //日期getBuf[31...38]
    //时间getBuf[39...46]
    //内容getBuf[50...n]
    //12345678
    //0891683108705505F0 04 0D91683197481378F8 0000 706050 211214 23 08 31D98C56B3DD70

    //0891683108200345F0 2405A14000F3         0000 706040 711114 00 21(18) A0100804028142A00D08040281402010080402814020100804D880361B
    //0891683108705505F0 040D91683128331255F2 0000 707071 516073 23 08 41E1901893CD68

    //PDU数据
    //第一包
    //0891683108705505F040 0D91 685189894895F2 0008 51903261356323 8C 050003AF0301 0061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061
    //第二包
    //0891683108705505F040 0D91 685189894895F2 0008 51903261352423 8C 050003AF0302 0061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100610061006100620062006200620062006200620062006200620062006200620062
    //第三包
    //0891683108705505F060 0D91 685189894895F2 0008 51903261358423 76 050003AF0303 0062003C0053005000420053004A002A00420053004A004700500053002A00310044003A0031003100320032003300330032002C0032003500350035003500350035003600360036003600360036003700370037003700370037002C5C0F5F3A003E0071007100710071007100710071

    //第一包
    //0891683108705505F040 0D91 685189894895F2 0000 51903251927323 A0 050003AE0201 C2E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3E170381C0E87C3
    getBuf->number[0] = 0; //手机号长度
    getBuf->sDataLen = 0;

    i = (chr(*Rxbuf) << 4) + chr(*(Rxbuf + 1)); //短信服务中心长度 08
    j = (i * 2) + 4; //指针指向目标地址长度处

    a1 = (chr(*(Rxbuf + j)) << 4) + chr(*(Rxbuf + j + 1)); //取目标地址长度 0D(13)
    getBuf->number[0] = a1; //目标地址长度存入 0D(13)
    j += 4; //指针指向目标地址处
    z = 1;
    if ((a1 % 2) != 0)
        a1++;//把奇数变为偶数
    a1 = a1 / 2;       //(13+1)/2=7
    if (a1 > 24)
        a1 = 24;
    for (i = 1; i <= a1; i++)
    {
        getBuf->number[z++] = *(Rxbuf + j + 1); //两两互反
        if (*(Rxbuf + j) != 'F')
        {
            getBuf->number[z++] = *(Rxbuf + j); //没结束则存
        }
        j += 2; //指针指向下一字节
    }
    z = 0;
    getBuf->smsType = (chr(*(Rxbuf + j + 2)) << 4) + chr(*(Rxbuf + j + 3)); //短信息类型（00/08） 00是编码的，08是汉字方式的
    getBuf->sDataLen = (chr(*(Rxbuf + j + 18)) << 4) + chr(*(Rxbuf + j + 19)); //实际信息内容长度
    //日期
    getBuf->sDate[0] = *(Rxbuf + j + 5);
    getBuf->sDate[1] = *(Rxbuf + j + 4);
    getBuf->sDate[2] = '-';
    getBuf->sDate[3] = *(Rxbuf + j + 7);
    getBuf->sDate[4] = *(Rxbuf + j + 6);
    getBuf->sDate[5] = '-';
    getBuf->sDate[6] = *(Rxbuf + j + 9);
    getBuf->sDate[7] = *(Rxbuf + j + 8);
    //时间
    getBuf->sTime[0] = *(Rxbuf + j + 11);
    getBuf->sTime[1] = *(Rxbuf + j + 10);
    getBuf->sTime[2] = ':';
    getBuf->sTime[3] = *(Rxbuf + j + 13);
    getBuf->sTime[4] = *(Rxbuf + j + 12);
    getBuf->sTime[5] = ':';
    getBuf->sTime[6] = *(Rxbuf + j + 15);
    getBuf->sTime[7] = *(Rxbuf + j + 14);

    j += 20; //指针指向编码处开始

    /*
    05000350 0202
    002C00420053004A002D
    */
    //050003AE0201
    g_cLongsmsIndex = 0;
    i = (chr(*(Rxbuf + j)) << 4) + chr(*(Rxbuf + j + 1));
    if ((i > 0) && (i < 0x0a))
    {
        a1 = (chr(*(Rxbuf + j + i * 2)) << 4) + chr(*(Rxbuf + j + 1 + i * 2));
        if (a1 <= MAX_LONGSMS_COUNT)
        {
            g_cLongsmsIndex = a1;
            g_struLongsms[ a1 - 1 ].head[ 0 ] = (i + 1);
            k1 = j;
            for (z = 0 ; z < (i + 1) ; z++)
            {
                g_struLongsms[ a1 - 1 ].head[ z + 1 ] = (chr(*(Rxbuf + k1)) << 4) + chr(*(Rxbuf + k1 + 1));
                k1 += 2;
            }
        }
    }
    else
    {
        for (a1 = 0;  a1 < MAX_LONGSMS_COUNT ; a1 ++)
        {
            g_struLongsms[ a1 ].head[ 0 ] = 0;
        }
        g_cLongsmsIndex = 0;
    }



    if (getBuf->smsType == TXT) //编码方式，需解码
    {
        k = 0;
        while (j < (sAll - 1)) //59 -71
        {
            //------------    内容暂存400以后
            //270804028136A0
            Rxbuf[++k] = (chr(*(Rxbuf + j)) << 4) + chr(*(Rxbuf + j + 1));
            j += 2;
        }
        RxLen = k;
        j = 1;
        k = 0;
        k1 = 0;
        a1 = 0;
        if (RxLen >= 7) //13 9d c8 65 3b dd 07 93 85 c0
        {
b_d:
            a1 = 0;
            k += 8;
            k1 = 0;
            for (i = 1; i <= 7; i++)
            {
                if ((i + 1) <= 7)
                {
                    z = (Rxbuf[j + 7 - i] & br[i]) >> bl[i];
                    a1 |= z;
                    getBuf->GlobBuf[k - k1] = a1;
                    k1++;
                    a1 = (Rxbuf[j + 7 - i] & bh[i]) << bh1[i];
                }
                else
                {
                    z = (Rxbuf[j + 7 - i] & br[i]) >> bl[i];
                    a1 |= z;
                    getBuf->GlobBuf[k - k1] = a1;
                    k1++;
                    getBuf->GlobBuf[k - k1] = Rxbuf[j + 7 - i] & bh[i];
                }
            }
            j += 7;
            RxLen -= 7;
            if (RxLen >= 7)
                goto b_d;
        }
        k++;
        if (RxLen > 1)
        {
            a1 = 7;
            z = 0;
            k1 = 1;
            for (i = 1; i <= (RxLen - 1); i++)
            {
                if (i == 1)
                {
                    getBuf->GlobBuf[k] = (Rxbuf[j] & 0x7f);
                    k++;
                }
                z = Rxbuf[j] >> (8 - i); //1111 1111
                getBuf->GlobBuf[k] = ((Rxbuf[++j] << i) & 0x7f) | z;
                k++;
            }
        }
        else if (RxLen == 1)
        {
            getBuf->GlobBuf[k] = (Rxbuf[j] & 0x7f);
            k++;
        }
    }
    else
    {
        //0891683108705505F0040D91683128331255F20008707071512322230462114EEC
        k = 0;
        for (RxLen = 1; RxLen <= getBuf->sDataLen; RxLen++)
        {
            getBuf->GlobBuf[++k] = (chr(*(Rxbuf + j)) << 4) + chr(*(Rxbuf + j + 1));
            j += 2;
        }
    }
}

void rSMSdat(UserType_SMS *Text)
{
    //\r\n+CMGR: 2,,36\r\n
    //9(15)F99 (\r19)
    //O(21)K
    u16 bidx = 0, k1 = 0, k2 = 0;

    bidx = instr(&gsmrxd.buf[0], gsmrxd.Len, "+CMGR:", 6);
    if (bidx > 0)
    {
        k1 = instr(&gsmrxd.buf[bidx], gsmrxd.Len, "\r\n", 2);
        if (k1 > 0)
        {
            k1 += (bidx - 1);
            k1 += 2;
            k2 = instr(&gsmrxd.buf[k1], gsmrxd.Len - k1, "\r\n", 2);
            if (k2 > 0)
            {
                getPDUsms(Text, &gsmrxd.buf[k1], k2 - 1);
            }
        }
    }
}

#else
void rSMSdat(UserType_SMS *Text)
{
    /*+CMGR: "REC READ","+8615919835024","","14/03/12,10:36:50+08"
    <You Config Pawss Err>*/

    u8 NumIdx  = 0;
    u8 NumLen = 0;
    u8 DataIdx = 0;
    u16 DataLen = 0;
    u8 i = 0;

    Text->smsType = TXT;
    i = instr(&gsmrxd.buf[0], gsmrxd.Len, "+CMGR:", 6);
    if (i > 0)
    {
        NumIdx = instr(&gsmrxd.buf[0], gsmrxd.Len, ",", 1);
        if (NumIdx > 0)
        {
            NumIdx += 1;      //去掉'"'符号
            NumLen = instr(&gsmrxd.buf[NumIdx], gsmrxd.Len - NumIdx, "\"", 1);
            NumLen -= 1;
            if (NumLen <= 16)
            {
                Text->number[0] = NumLen;
                conver(&gsmrxd.buf[NumIdx], &Text->number[1], NumLen);
            }
        }
        DataIdx = instr(&gsmrxd.buf[i], gsmrxd.Len, "\r\n", 2);
        if (DataIdx > 0)
        {
            DataIdx += 1;       //跳过"\n"
            DataLen = instr(&gsmrxd.buf[DataIdx + i], gsmrxd.Len - (DataIdx + i), "\r\n", 2);
            DataLen -= 1;
            if (DataLen > gsmBufSize - 20)
            {
                DataLen = gsmBufSize - 20;
            }
            Text->sDataLen = DataLen;
            conver(&gsmrxd.buf[DataIdx + i], &Text->GlobBuf[1], DataLen);
        }
    }
}
#endif


u8 sms_done(UserType_SMS *Text)
{
    u8  Item = 0;
    u16 Len = 0, k = 1, Idx = 0;
    u8  TmpItem = 0;
    u8  ResSms = 0;

    #ifdef NOTE_GROUP
    u8 longsmsflag = 0;
    longsmsflag = Longsmspro(Text);
    if (longsmsflag == 1)
    {
        return 0;
    }
    else if ((longsmsflag == 0) || (longsmsflag == 2))
    {
    #endif
        if (Text->smsType != TXT)
        {
            for (Len = 1; Len <= Text->sDataLen; Len++)
            {
                Text->GlobBuf[ k++ ] = Text->GlobBuf[ Len + 1 ];
                Len++;
            }
            Text->sDataLen /= 2;
        }
        #ifdef NOTE_GROUP
    }
        #endif

    #if 0 //PassWordFunc
    for (Item = 0; Item < sizeof(PassWordStr) / sizeof(PassWordStr[0]); Item++)
    {
        Idx = instr(&Text->GlobBuf[1], Text->sDataLen, PassWordStr[Item], strlen((char const *)PassWordStr[Item]));
        if (Idx > 0)
        {
            GetPassWord = TRUE;
            break;
        }
        Idx = 0;
    }
    #endif

    //printf("-----------------> sizeof(SmsOptStr) = %d\r\n",sizeof(SmsOptStr));
    for (Item = 0; Item < sizeof(SmsOptStr) / sizeof(SmsOptStr[0]); Item++)
    {
        Idx = instr(&Text->GlobBuf[1], Text->sDataLen, SmsOptStr[Item], strlen((char const *)SmsOptStr[Item]));
        if (Idx > 0)
        {
            TmpItem = (Item + 1);
            switch (TmpItem)
            {
                case SMS_SPBSJ:
                case SMS_SPHYMOST:
                    NRF_LOG_INFO("---> Receive SMS SPHYMOST\r\n");
                    #if 0 //PassWordFunc
                    if (GetPassWord == FALSE)
                    {
                        DEBUG_PrintStrInfo(UART_TARGET, "---> SMS Pass Word Error\r\n");
                        Len = sizeof("<SMS Pass Word Error>") - 1;
                        conver("<SMS Pass Word Error>", &g_cPublicBuffer[0], Len);
                        Send_SmsBg(g_cPublicBuffer, Len, TXT, &Text->number[0], 9);
                        break;
                    }
                    #endif
                    if (Text->sDataLen > Idx)
                    {
                        if (AppStu.ResetMsg == REMOTE_RESET_FG)
                        {
                            Len = sizeof("<Reset Device!!!>") - 1;
                            conver("<Reset Device!!!>", &g_cPublicBuffer[0], Len);
                            Send_SmsBg(g_cPublicBuffer, Len, TXT, &Text->number[0], 9);
                        }

                    }
                    break;

                case SMS_CKBSJ:
                case SMS_CKHYMOST:
                    NRF_LOG_INFO("---> Receive SMS CKHYMOST\r\n");
                    Gsm_QuePara();
                    break;

                case QUE_SMS_LOG: //处理CKLOG
                    //GSM_SmsQueLogData();
                    //GSM_PackDataToServer(TE_LOG_CALLER_SMS, 2);
                    break;
                case SMS_SPHYDBG:
                    NRF_LOG_INFO("---> Receive SMS SPHYDBG\r\n");
                    break;

                case SMS_CKHYDBG:
                    NRF_LOG_INFO("---> Receive SMS CKHYDBG\r\n");
                    Gsm_QueDbg();
                    break;

                default:
                    break;
            }
            break;
        }
    }
    switch (ResSms)
    {
        case CIG_NOMAL_STA:
        case CIG_GPRS_PARA:
            Len = sizeof("<OK>") - 1;
            conver("<OK>", &g_cPublicBuffer[0], Len);
            Send_SmsBg(g_cPublicBuffer, Len, TXT, &Text->number[0], 9);
            break;

        case CIG_PWD_EER:
            Len = sizeof("<You Config Pawss Err>") - 1;
            conver("<You Config Pawss Err>", &g_cPublicBuffer[0], Len);
            Send_SmsBg(g_cPublicBuffer, Len, TXT, &Text->number[0], 8);
            break;
        case CIG_FMA_ERR:
            Len = sizeof("<Config Format Err>") - 1;
            conver("<Config Format Err>", &g_cPublicBuffer[0], Len);
            Send_SmsBg(g_cPublicBuffer, Len, TXT, &Text->number[0], 7);
            break;
        default:
            break;
    }
    //if (RetCigParFg == true)
    //{
    //Gsm_QuePara();
    //}
    return (0);
}


u8 readsmsIndex(void)
{
    GsmStu.SmsIndex = 0;
    gsmATCMGF1();   //设置为文本模式
    gsmATCMGL();    //查询短消息列表
    return (GsmStu.SmsIndex);
}

u8 sms_send_buf[150];
//HYN001-15 Modify sms code add by hedaihua at 20201128 start
void Send_SmsBg(u8 *bg, u8 len, u8 SendType, u8 *num, u8 num_len)
{
    #if 0//def SMS_PDU_SEND
    u8 i = 0;
    u8 SmsCACANum[ 24 ];

    i = i;
    if (SendType != TXT && SendType != PDU)
    {
        return;
    }
    gsmATCNMI();
    gsmATCSCA(SmsCACANum);
    //gsmATCSCSGSM();
    GSM_SendSmsByPdu(bg, len, SendType, num, SmsCACANum);
    #else
    u8 i = 0;
    u8 *pBg = sms_send_buf;
    //pBg = bg;
    memset(sms_send_buf, 0, sizeof(sms_send_buf));
    memcpy(pBg, bg, sizeof(sms_send_buf));


    if (len > SMS_TXT_SEND_MAX_SIZE)
    {
        len = SMS_TXT_SEND_MAX_SIZE - 2;
    }
    //SMSB.smsType = TXT;//回复为文本方式
    gsmATCMGF1();       //设置文本模式
    gsmATCNMI();
    if (gsmATCMGS(num, num_len, TXT) == OK)
    {
        nrf_delay_ms(400);
        *(pBg + len) = 0x1a;
        len += 1;
        i = sendgsmDat(pBg, len, 1, 2, 1, 120);
    }
    #endif
}
//HYN001-15 Modify sms code add by hedaihua at 20201128 end
void Gsm_QuePara(void)
{
    static u8 Len = 0;
    Len = 0;
    Send_SmsBg(rSmsBufA.GlobBuf, Len, TXT, &rSmsBufA.number[0], 4);

}
void Gsm_QueDbg(void)
{
    static u8 Len = 0;
    Len = 0;

    Len = ck_hydbg(&rSmsBufA.GlobBuf[0], SMS_QUE);
    if (Len > SMS_TXT_SEND_MAX_SIZE) //大于140字节分两条发送
    {
        //Len = GSM_GetDnsAndIp(&rSmsBufA.GlobBuf[0]);
        Send_SmsBg(&(rSmsBufA.GlobBuf[0]), 140, TXT, &rSmsBufA.number[0], 6);
        delayms(500);
        //Len = GSM_GetOtherPara(&rSmsBufA.GlobBuf[0]);
        Send_SmsBg(&(rSmsBufA.GlobBuf[140]), (Len - 140), TXT, &rSmsBufA.number[0], 5);
        delayms(10);
    }
    else
    {
        Send_SmsBg(rSmsBufA.GlobBuf, Len, TXT, &rSmsBufA.number[0], 4);
    }

}


void CheckSMS(u8 Data)  // 检查是否有短信
{
    // +CMTI: 短信提示
    static u8 sta = 0;

    switch (sta)
    {
        case 0:
        {
            if (Data == '+')
                sta = 1;
            else
                sta = 0;
            break;
        }
        case 1:
        {
            if (Data == 'C')
                sta = 2;
            else
                sta = 0;
            break;
        }
        case 2:
        {
            if (Data == 'M')
                sta = 3;
            else
                sta = 0;
            break;
        }
        case 3:
        {
            if (Data == 'T')
                sta = 4;
            else
                sta = 0;
            break;
        }
        case 4:
        {
            if (Data == 'I')
                sta = 5;
            else
                sta = 0;
            break;
        }
        case 5:
        {
            if (Data == ':')
                sta = 6;
            else
                sta = 0;
        }
        case 6:
        {
            GsmStu.ReadSmsFg = CNMI;
            sta     = 0;
            break;
        }
        default:
            break;
    }
}

#if 0
    void GSM_SmsQueLogData(void)
#endif

void readsmspro(void)
{
    //UserType_SMS rSmsBuf;
    u8 i, index;

    if (GsmStu.creg != REG_LOC_NET && GsmStu.creg != REG_ROAM_NET)
    {
        return;//无信号不读短信
    }

    if (GsmStu.ReadSmsFg == CNMI)
    {
        GsmStu.ReadSmsFg = 0;
        NRF_LOG_INFO("---> Check And Receive SMS\r\n");
        //into_gsm(); //mwtang sms
        gsmATCMGF1();
        i = 50;
        index = 1;
        while (index > 0 && (--i > 0))
        {
            index = readsmsIndex();//读信息索引
            if (index > 0)
            {
                if (gsmATCMGR(index) == OK) //读当前索引信息
                {
                    memset(&rSmsBufA, 0, sizeof(rSmsBufA));
                    rSMSdat(&rSmsBufA);//取信息
                    gsmATCMGD(index);//删除信息
                    if (rSmsBufA.sDataLen > 0) //有信息
                    {
                        sms_done(&rSmsBufA);
                    }
                }
                else
                {
                    gsmATCMGD(index);//删除信息
                }
            }
        }
        GsmStu.qInternalTime = 0;
    }
}


