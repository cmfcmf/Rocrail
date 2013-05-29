/*
 Rocrail - Model Railroad Software

 Copyright (C) 2002-2013 Rob Versluis, Rocrail.net

 Without an official permission commercial use is not permitted.
 Forking this project is not permitted.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "rocdigs/impl/srcp_impl.h"

#include "rocdigs/impl/srcp/serial.h"
#include "rocdigs/impl/srcp/srcp.h"

#include "rocrail/wrapper/public/DigInt.h"
#include "rocrail/wrapper/public/SRCP.h"



Boolean serialInit( obj inst ) {
  iOSRCPData data = Data(inst);
  TraceOp.trc( name, TRCLEVEL_INFO, __LINE__, 9999, "  device    : %s", wDigInt.getdevice(data->ini) );

  data->serial = SerialOp.inst( wDigInt.getdevice( data->ini ) );

  SerialOp.setFlow( data->serial, none );

  SerialOp.setLine( data->serial, 115200, 8, 1, none, wDigInt.isrtsdisabled( data->ini ) );
  SerialOp.setTimeout( data->serial, wDigInt.gettimeout( data->ini ), wDigInt.gettimeout( data->ini ) );

  data->serialMux = MutexOp.inst( NULL, True );

  return True;
}


int serialConnect( obj inst, Boolean info ) {
  iOSRCPData data = Data(inst);

  if( data->serial != NULL && !data->serialOK ) {
    TraceOp.trc( name, TRCLEVEL_INFO, __LINE__, 9999, "connecting to [%s]", wDigInt.getdevice( data->ini ) );
    data->serialOK = SerialOp.open( data->serial );
    return SRCPCONNECT_RECONNECTED;
  }

  if( data->serial != NULL && data->serialOK ) {
    return SRCPCONNECT_OK;
  }

  if( data->serial != NULL ) {
    SerialOp.close( data->serial );
  }
  return SRCPCONNECT_ERROR;
}


void serialDisconnect( obj inst, Boolean info ) {
  iOSRCPData data = Data(inst);
  /*
  TraceOp.trc( name, TRCLEVEL_INFO, __LINE__, 9999, "disconnecting from [%s]", wDigInt.getdevice( data->ini ) );
  if( data->serial != NULL ) {
    SerialOp.base.del( data->serial );
    data->serial = NULL;
  }
  */
}


static int __serialReadInternal ( obj inst, char *cmd, Boolean info ) {
  iOSRCPData data = Data(inst);
  int idx = 0;
  cmd[idx] = '\0';

  while( data->run && data->serial != NULL ) {
    if( !SerialOp.read( data->serial, &cmd[idx], 1 ) ) {
      if( idx > 0 )
        TraceOp.dump( NULL, TRCLEVEL_WARNING, cmd, idx );
      return 0;
    }
    if( cmd[idx] == '\n' ) {
      break;
    }
    idx++;
  };

  return StrOp.len(cmd);
}


int serialRead ( obj inst, char *cmd, Boolean info ) {
  iOSRCPData data = Data(inst);
  int len = 0;

  if( MutexOp.trywait( data->serialMux, 100 ) ) {
    if( serialAvailable(inst) ) {
      len = __serialReadInternal(inst, cmd, info);
    }
    MutexOp.post(data->serialMux);
  }

  return len;
}


int serialWrite( obj inst, const char *cmd, char* rsp, Boolean info ) {
  iOSRCPData data = Data(inst);
  int retstate = 0;
  Boolean rc = False;

  if( data->serial == NULL ) {
    return retstate;
  }

  if( MutexOp.trywait( data->serialMux, 100 ) ) {

    rc = SerialOp.write( data->serial, cmd, StrOp.len(cmd) );

    if(rc) {
      char inbuf[1024] = {'\0'};
      char szResponse[1024] = {'\0'};

      if( __serialReadInternal(inst, inbuf, info) > 0 ) {
        StrOp.replaceAll(inbuf, '\n', ' ');
        TraceOp.trc( name, TRCLEVEL_MONITOR, __LINE__, 9999, "srcp response [%s] on command [%s]",inbuf, cmd);

        /* Scan for SM return? */
        MemOp.set(szResponse,0,900);
        sscanf(inbuf,"%*s %d %900c",&retstate,szResponse);

        data->state = (SRCP_OK(rc)?SRCP_STATE_OK:SRCP_STATE_ERROR);

        if( rsp != NULL )
          StrOp.copy( rsp, szResponse );

      }
    }
    MutexOp.post(data->serialMux);
  }

  return retstate;
}


Boolean serialAvailable( obj inst ) {
  iOSRCPData data = Data(inst);
  int available = SerialOp.available(data->serial);
  return available > 0 ? True:False;
}
