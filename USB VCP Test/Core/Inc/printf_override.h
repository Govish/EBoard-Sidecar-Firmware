#ifndef PRINTF_OVRD_H
#define PRINTF_OVRD_H

#include  <errno.h>
#include  <sys/unistd.h> // STDOUT_FILENO, STDERR_FILENO
#include "usb_device.h"
#include "usbd_cdc_if.h"

int _write(int file, char *data, int len)
{
   if ((file != STDOUT_FILENO) && (file != STDERR_FILENO))
   {
      errno = EBADF;
      return -1;
   }

   uint8_t status;
   status = CDC_Transmit_FS((uint8_t*)data, len);

   // return # of bytes written - as best we can tell
   return (status == USBD_OK ? len : 0);
}

#endif
