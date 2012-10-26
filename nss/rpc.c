/*
   rpc.c - NSS lookup functions for rpc database

   Copyright (C) 2006 West Consulting
   Copyright (C) 2006, 2007, 2008, 2010, 2012 Arthur de Jong
   Copyright (C) 2010 Symas Corporation

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301 USA
*/

#include "config.h"

#include <string.h>
#include <errno.h>

#include "prototypes.h"
#include "common.h"
#include "compat/attrs.h"

/* read a sinlge rpc entry from the stream */
static nss_status_t read_rpcent(
        TFILE *fp,struct rpcent *result,
        char *buffer,size_t buflen,int *errnop)
{
  int32_t tmpint32,tmp2int32,tmp3int32;
  size_t bufptr=0;
  memset(result,0,sizeof(struct rpcent));
  READ_BUF_STRING(fp,result->r_name);
  READ_BUF_STRINGLIST(fp,result->r_aliases);
  READ_INT32(fp,result->r_number);
  return NSS_STATUS_SUCCESS;
}

#ifdef NSS_FLAVOUR_GLIBC

/* get a rpc entry by name */
nss_status_t _nss_ldap_getrpcbyname_r(
        const char *name,struct rpcent *result,
        char *buffer,size_t buflen,int *errnop)
{
  NSS_BYNAME(NSLCD_ACTION_RPC_BYNAME,
             name,
             read_rpcent(fp,result,buffer,buflen,errnop));
}

/* get a rpc entry by number */
nss_status_t _nss_ldap_getrpcbynumber_r(
        int number,struct rpcent *result,
        char *buffer,size_t buflen,int *errnop)
{
  NSS_BYINT32(NSLCD_ACTION_RPC_BYNUMBER,
              number,
              read_rpcent(fp,result,buffer,buflen,errnop));
}

/* thread-local file pointer to an ongoing request */
static __thread TFILE *rpcentfp;

/* request a stream to list all rpc entries */
nss_status_t _nss_ldap_setrpcent(int UNUSED(stayopen))
{
  NSS_SETENT(rpcentfp);
}

/* get an rpc entry from the list */
nss_status_t _nss_ldap_getrpcent_r(
        struct rpcent *result,
        char *buffer,size_t buflen,int *errnop)
{
  NSS_GETENT(rpcentfp,NSLCD_ACTION_RPC_ALL,
             read_rpcent(rpcentfp,result,buffer,buflen,errnop));
}

/* close the stream opened by setrpcent() above */
nss_status_t _nss_ldap_endrpcent(void)
{
  NSS_ENDENT(rpcentfp);
}

#endif /* NSS_FLAVOUR_GLIBC */

#ifdef NSS_FLAVOUR_SOLARIS

#ifdef HAVE_STRUCT_NSS_XBYY_ARGS_RETURNLEN
static char *rpcent2str(struct rpcent *result,char *buffer,size_t buflen)
{
  int res,i;
  res=snprintf(buffer,buflen,"%s %d",result->r_name,result->r_number);
  if ((res<0)||(res>=buflen))
    return NULL;
  if (result->r_aliases)
    for (i=0;result->r_aliases[i];i++)
    {
      strlcat(buffer," ",buflen);
      strlcat(buffer,result->r_aliases[i],buflen);
    }
  if (strlen(buffer)>=buflen-1)
    return NULL;
  return buffer;
}
#endif /* HAVE_STRUCT_NSS_XBYY_ARGS_RETURNLEN */

static nss_status_t read_result(TFILE *fp,nss_XbyY_args_t *args)
{
  nss_status_t retv;
#ifdef HAVE_STRUCT_NSS_XBYY_ARGS_RETURNLEN
  struct rpcent result;
  char *buffer;
  /* try to return in string format if requested */
  if (args->buf.result==NULL)
  {
    /* read the entry into a temporary buffer */
    buffer=(char *)malloc(args->buf.buflen);
    if (buffer==NULL)
      return NSS_STATUS_UNAVAIL;
    retv=read_rpcent(fp,&result,buffer,args->buf.buflen,&args->erange);
    /* format to string */
    if (retv==NSS_STATUS_SUCCESS)
      if (rpcent2str(&result,args->buf.buffer,args->buf.buflen)==NULL)
      {
        args->erange=1;
        retv=NSS_NOTFOUND;
      }
    /* clean up and return result */
    free(buffer);
    if (retv!=NSS_STATUS_SUCCESS)
      return retv;
    args->returnval=args->buf.buffer;
    args->returnlen=strlen(args->returnval);
    return NSS_STATUS_SUCCESS;
  }
#endif /* HAVE_STRUCT_NSS_XBYY_ARGS_RETURNLEN */
  /* read the entry */
  retv=read_rpcent(fp,args->buf.result,args->buf.buffer,args->buf.buflen,&args->erange);
  if (retv!=NSS_STATUS_SUCCESS)
    return retv;
  args->returnval=args->buf.result;
  return NSS_STATUS_SUCCESS;
}

static nss_status_t rpc_getrpcbyname(nss_backend_t UNUSED(*be),void *args)
{
  NSS_BYNAME(NSLCD_ACTION_RPC_BYNAME,
             NSS_ARGS(args)->key.name,
             read_result(fp,args));
}

static nss_status_t rpc_getrpcbynumber(nss_backend_t UNUSED(*be),void *args)
{
  NSS_BYINT32(NSLCD_ACTION_RPC_BYNUMBER,
              NSS_ARGS(args)->key.number,
              read_result(fp,args));
}

static nss_status_t rpc_setrpcent(nss_backend_t *be,void UNUSED(*args))
{
  NSS_SETENT(LDAP_BE(be)->fp);
}

static nss_status_t rpc_getrpcent(nss_backend_t *be,void *args)
{
  NSS_GETENT(LDAP_BE(be)->fp,NSLCD_ACTION_RPC_ALL,
             read_result(LDAP_BE(be)->fp,args));
}

static nss_status_t rpc_endrpcent(nss_backend_t *be,void UNUSED(*args))
{
  NSS_ENDENT(LDAP_BE(be)->fp);
}

static nss_backend_op_t rpc_ops[]={
  nss_ldap_destructor,
  rpc_endrpcent,
  rpc_setrpcent,
  rpc_getrpcent,
  rpc_getrpcbyname,
  rpc_getrpcbynumber
};

nss_backend_t *_nss_ldap_rpc_constr(const char UNUSED(*db_name),
                  const char UNUSED(*src_name),const char UNUSED(*cfg_args))
{
  return nss_ldap_constructor(rpc_ops,sizeof(rpc_ops));
}

#endif /* NSS_FLAVOUR_SOLARIS */
