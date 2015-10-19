/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013 - 2015  LiteSpeed Technologies, Inc.                 *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/
#include "../include/ls.h"
#include <lsr/ls_base64.h>
#include <lsr/ls_loopbuf.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <lsdef.h>

/////////////////////////////////////////////////////////////////////////////
//DEFINE the module name, MUST BE the same as .so file name
//ie.  if MNAME is testmodule, then the .so must be testmodule.so
#define     MNAME       updatetcpin2
/////////////////////////////////////////////////////////////////////////////

lsi_module_t MNAME;
#define MAX_BLOCK_BUFSIZE   8192

typedef struct _MyData2
{
    ls_loopbuf_t inBuf;
    ls_loopbuf_t outBuf;
} MyData2;

static int l4release2(void *data)
{
    MyData2 *myData = (MyData2 *)data;
    g_api->log(NULL, LSI_LOG_DEBUG, "#### updatetcpin1 test %s\n",
               "l4release");

    if (myData)
    {
        ls_loopbuf_d(&myData->inBuf);
        ls_loopbuf_d(&myData->outBuf);
        free(myData);
    }

    return 0;
}


static int l4init2(lsi_param_t *rec)
{
    MyData2 *myData = (MyData2 *)g_api->get_module_data(rec->session, &MNAME,
                      LSI_DATA_L4);
    if (!myData)
    {
        myData = (MyData2 *) malloc(sizeof(MyData2));
        ls_loopbuf(&myData->inBuf, MAX_BLOCK_BUFSIZE);
        ls_loopbuf(&myData->outBuf, MAX_BLOCK_BUFSIZE);

        g_api->log(NULL, LSI_LOG_DEBUG, "#### updatetcpin2 test %s\n", "l4init");
        g_api->set_module_data(rec->session, &MNAME, LSI_DATA_L4,
                               (void *)myData);
    }
    else
    {
        ls_loopbuf_clear(&myData->inBuf);
        ls_loopbuf_clear(&myData->outBuf);
    }

    return 0;
}

//expand the recieved data to base64 encode
static int l4recv2(lsi_param_t *rec)
{
#define ENCODE_BLOCK_SIZE 800
#define DECODE_BLOCK_SIZE (ENCODE_BLOCK_SIZE * 3 / 4 + 4)

    MyData2 *myData = NULL;
    char *pBegin;
    char tmpBuf[ENCODE_BLOCK_SIZE];
    int len, sz;

    myData = (MyData2 *)g_api->get_module_data(rec->session, &MNAME,
             LSI_DATA_L4);
    if (!myData)
        return LS_FAIL;

    while ((len = g_api->stream_read_next(rec, tmpBuf, ENCODE_BLOCK_SIZE)) > 0)
    {
        g_api->log(NULL, LSI_LOG_DEBUG,
                   "#### updatetcpin2 test l4recv, inLn = %d\n", len);
        ls_loopbuf_append(&myData->inBuf, tmpBuf, len);
    }

    while (!ls_loopbuf_empty(&myData->inBuf))
    {
        ls_loopbuf_straight(&myData->inBuf);
        pBegin = ls_loopbuf_begin(&myData->inBuf);
        sz = ls_loopbuf_size(&myData->inBuf);
        if (sz > ENCODE_BLOCK_SIZE)
            sz = ENCODE_BLOCK_SIZE;

        len = ls_base64_decode((const char *)pBegin, sz, tmpBuf);
        if (len > 0)
        {
            ls_loopbuf_append(&myData->outBuf, tmpBuf, len);
            ls_loopbuf_popfront(&myData->inBuf, sz);
        }
        else
            break;
    }

    ////////////////////////////////////////////////////////////////////////////////
    if (ls_loopbuf_size(&myData->outBuf) < rec->len1)
        rec->len1 = ls_loopbuf_size(&myData->outBuf);

    if (rec->len1 > 0)
        ls_loopbuf_moveto(&myData->outBuf, (char *)rec->ptr1, rec->len1);

    return rec->len1;
}

static lsi_serverhook_t serverHooks[] =
{
    {LSI_HKPT_L4_BEGINSESSION, l4init2, LSI_HOOK_NORMAL, LSI_FLAG_ENABLED},
    {LSI_HKPT_L4_RECVING, l4recv2, LSI_HOOK_EARLY + 1, LSI_FLAG_TRANSFORM | LSI_FLAG_ENABLED},
    LSI_HOOK_END   //Must put this at the end position
};

static int init(lsi_module_t *pModule)
{
    g_api->init_module_data(pModule, l4release2, LSI_DATA_L4);
    return 0;
}

lsi_module_t MNAME = { LSI_MODULE_SIGNATURE, init, NULL, NULL, "", serverHooks };
